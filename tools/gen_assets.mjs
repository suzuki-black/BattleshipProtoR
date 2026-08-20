// gen_assets.mjs — BGM 曲データ(＋将来の他アセット)をデータバンク bin へパック。
//   出力: build/assets.bin   … データバンク(rompack が --bank へ生配置)の内容
//         build/bgm_data.h   … 常駐Cが読む定数(音階periodテーブル/曲オフセット/長さ)
//
//   トラック1本のバイト列(バンクに置き、再生時に data_read で RAM へコピー):
//     [ nMel, nBas, basStep, melPeak, melSus, melVib, basPeak, basSus, drumOn,
//       melNote(nMel), melLen(nMel), basNote(nBas) ]
//       melNote/basNote = 音階index(0=C2..47=B5) or 255=休符
//       melLen  = 各音符のフレーム数(60Hz)。bass は固定 basStep。
//       envelope: 発音開始 peak → 毎フレーム-1 → sustain 保持、末尾2フレーム無音。
//       vib=1: 伸ばし音(el>8)に三角ビブラート。drumOn=1: 標準マーチドラム(noise)。
//   ※旧 BattleshipProto/cport の bgm_tracks.h からタイトル/1面を移植(音階index基準は同一)。
//
// 使い方: node tools/gen_assets.mjs <bank番号> build/assets.bin build/bgm_data.h

import { writeFileSync, readFileSync } from 'node:fs';

const [, , bankArg, binOut, hdrOut] = process.argv;
const BGM_BANK = parseInt(bankArg ?? '8', 10);

// ---- PSG tone period テーブル(12bit)。TP = 1.7897725MHz/(16*freq) ----
const notetp = [];
for (let i = 0; i < 48; i++) {
  const freq = 440 * Math.pow(2, (36 + i - 69) / 12);   // midi=36+i, A4(midi69)=440Hz
  notetp.push(Math.round(111860.78125 / freq));
}

// ---- 曲データ(旧cport bgm_tracks.h より移植。melody/bass=音階index, 255=休符) ----
const TRACKS = [
  { // 0: タイトル(アレスタ2風, ニ短調, 四分16f)
    mel: [33,38, 36,34,33, 29,31,33, 26,33, 34,33,31, 33,38, 36,34,33,31, 29,
          38,36, 34,33,34, 36,38, 33,33, 34,33,31, 29,31,33, 31,29,28, 26,255],
    mln: [32,32, 16,16,32, 16,16,32, 48,16, 32,16,16, 32,32, 16,16,16,16, 64,
          32,32, 16,16,32, 32,32, 48,16, 32,16,16, 16,16,32, 32,16,16, 48,16],
    bas: [2,9,14,9, 2,9,14,9, 5,5,17,5, 2,9,14,2, 10,10,22,10, 2,9,14,9, 2,9,14,9, 5,5,17,5,
          2,9,14,9, 10,10,22,10, 2,9,14,9, 9,9,21,9, 10,10,22,10, 2,9,14,9, 9,9,21,9, 2,9,14,2],
    basStep:16, melPeak:14, melSus:11, melVib:1, basPeak:11, basSus:8, drum:1,
  },
  { // 1: 1面(戦艦)マーチ(八分8f)
    mel: [26,26,33,26,29,26,33, 34,33,31,29,28,26,
          26,26,33,26,29,33,38, 36,34,33,31,29,28,
          26,26,33,26,29,26,33, 34,33,31,29,31,33,
          33,33,38,33,34,33,31, 29,28,26,25,26,
          33,38,41,40, 38,40,41,40,38,36,33,
          38,36,34,33, 31,33,34,33,31,29,
          26,26,33,26,29,26,33, 34,33,31,29,28,26,
          33,31,29,28,26,25,28,31, 26,255],
    mln: [8,8,8,8,8,8,16, 8,8,8,8,16,16,
          8,8,8,8,8,8,16, 8,8,8,8,16,16,
          8,8,8,8,8,8,16, 8,8,8,8,16,16,
          8,8,8,8,8,8,16, 8,8,8,8,32,
          16,16,16,16, 8,8,8,8,8,8,16,
          16,16,16,16, 8,8,8,8,16,16,
          8,8,8,8,8,8,16, 8,8,8,8,16,16,
          8,8,8,8,8,8,8,8, 48,16],
    bas: [2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 2,2,14,2,9,9,21,9,
          2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 9,9,21,9,9,9,21,9,
          2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 10,10,22,10,10,10,22,10, 9,9,21,9,9,9,21,9,
          2,2,14,2,2,2,14,2, 2,2,14,2,2,2,14,2, 9,9,21,9,9,9,21,9, 2,2,14,2,2,2,14,2],
    basStep:8, melPeak:14, melSus:11, melVib:1, basPeak:11, basSus:8, drum:1,
  },
  { // 2: エンディング(静かな讃歌, ヘ長調, ドラム無し・長い音長)
    mel: [33,36,38,36,33,31, 29,31,33,34,33, 36,38,40,38,36,33, 34,33,31,29,255],
    mln: [32,32,48,32,32,64, 48,32,48,32,64, 32,32,48,32,32,64, 48,32,48,64,64],
    bas: [5,17,5,17, 0,12,0,12, 2,14,2,14, 10,22,9,21],
    basStep:32, melPeak:11, melSus:9, melVib:1, basPeak:8, basSus:6, drum:0,
  },
];

function packTrack(t) {
  if (t.mel.length !== t.mln.length) throw new Error('mel/mln length mismatch');
  for (const v of [...t.mel, ...t.bas]) if (v < 0 || v > 255) throw new Error(`note out of range: ${v}`);
  return Buffer.from([
    t.mel.length, t.bas.length, t.basStep,
    t.melPeak, t.melSus, t.melVib, t.basPeak, t.basSus, t.drum,
    ...t.mel, ...t.mln, ...t.bas,
  ]);
}

// ---- 艦体 OPS データ(旧版 BattleshipProto の ship_ops/iowa_ops を忠実移植) ----
// 1レコード=7B {op, x, ylo, yhi, p1, p2, p3}, op=0 で終端。y は 16bit(0..495)。
// op: 1=GROUND 2=MAINGUN 3=DOME 4=DISK 5=DECKBOX 6=AAGUN 7=LMMV 8=BARREL (ship.h と一致)。
// ソースは 6値/レコード {op,x,y,p1,p2,p3}。ここで y を 2バイト分割して 7B に展開。
function shipops(recs) {
  const b = [];
  for (const [op, x, y, p1, p2, p3] of recs) b.push(op, x, y & 0xFF, (y >> 8) & 0xFF, p1, p2, p3);
  b.push(0);   // END
  for (const v of b) if (v < 0 || v > 255) throw new Error(`shipop byte out of range: ${v}`);
  return Buffer.from(b);
}
// 各面の艦(上面視, 艦高496px, 中心x=128)。hull=船体プロファイル(0=Bismarck/3=Iowa)、
// bowCnt/bowYb=波切り艦首シェブロン。船体/艦首/対空砲23基はコード(ship_render)、OPSは砲塔/艦橋/煙突/副砲。
const SHIPS = [
  { name: 'BISMARCK', hull: 0, bowCnt: 20, bowYb: 42,
    ops: shipops([
      [7,121,6,2,26,13], [7,135,6,2,26,13],                                  // 艦首波(暗)
      [3,118,34,5,0,0], [3,138,34,5,0,0],                                    // 前部ドーム
      [1,128,64,17,0,0], [2,128,64,15,0,0],                                  // Anton 主砲
      [1,128,104,19,0,0], [2,128,104,17,0,0],                                // Bruno 主砲
      [5,106,124,44,62,5],                                                   // 前部艦橋 基部
      [7,110,130,36,3,15],                                                   // 窓帯 下
      [5,112,134,32,46,5],                                                   // 2段
      [7,116,140,24,3,15],                                                   // 窓帯 上
      [5,116,146,24,30,4],                                                   // 3段(暗)
      [5,120,152,16,18,5],                                                   // 4段(頂)
      [3,128,130,9,0,0],                                                     // 司令塔
      [3,128,158,7,0,0],                                                     // 上部艦橋
      [3,128,170,6,0,0], [4,128,170,4,13,0],                                 // 前檣測距儀
      [1,104,200,7,0,0], [3,104,200,5,0,0],                                  // 15cm副砲 前左
      [1,152,200,7,0,0], [3,152,200,5,0,0],                                  // 前右
      [7,98,212,60,3,13],                                                    // 射出機レール
      [4,128,228,13,13,0], [4,128,226,11,5,0], [4,128,228,7,13,0],           // 煙突
      [7,114,236,28,4,4],                                                    // 煙突キャップ帯
      [1,104,250,7,0,0], [3,104,250,5,0,0],                                  // 副砲 後左
      [1,152,250,7,0,0], [3,152,250,5,0,0],                                  // 後右
      [7,128,262,2,26,9],                                                    // 主檣
      [5,112,266,32,34,5],                                                   // 後部指揮所 基部
      [5,116,272,24,24,4],                                                   // 2段(暗)
      [5,120,278,16,14,5],                                                   // 3段
      [3,128,276,8,0,0],                                                     // 後部管制
      [3,128,290,6,0,0], [4,128,290,3,13,0],                                 // 後部測距儀
      [1,128,322,19,0,0], [2,128,322,17,0,0],                               // Cäsar 主砲
      [1,128,362,17,0,0], [2,128,362,15,0,0],                               // Dora 主砲
      [7,112,396,4,48,5], [7,112,396,1,48,14],                              // 後甲板 縁
      [7,140,396,4,48,5], [7,140,396,1,48,14],
      [7,113,404,2,10,9], [7,108,407,12,2,4],
      [7,141,430,2,10,9], [7,136,433,12,2,4],
      [7,127,428,2,26,4], [3,128,452,6,0,0],                               // 艦尾
      [6,88,150,7,0,0], [6,168,150,7,0,0],                                 // 舷側対空砲ギャラリー12基
      [6,88,178,7,0,0], [6,168,178,7,0,0],
      [6,88,206,7,0,0], [6,168,206,7,0,0],
      [6,88,240,7,0,0], [6,168,240,7,0,0],
      [6,88,280,7,0,0], [6,168,280,7,0,0],
      [6,88,308,7,0,0], [6,168,308,7,0,0],
    ]),
  },
  { name: 'IOWA', hull: 3, bowCnt: 22, bowYb: 28,
    ops: shipops([
      [1,128,72,21,0,0], [2,128,72,19,0,0],                                 // 主砲1
      [1,128,108,23,0,0], [2,128,108,21,0,0],                               // 主砲2
      [5,112,146,32,56,4],                                                  // 前部艦橋 基部
      [7,116,151,24,4,15],                                                  // 窓帯 下
      [5,114,156,28,42,5],                                                  // 2段
      [7,118,161,20,3,15],                                                  // 窓帯 上
      [5,116,166,24,28,4],                                                  // 3段(暗)
      [5,120,172,16,18,5],                                                  // 4段
      [3,128,150,8,0,0], [3,128,168,7,0,0],                                 // 司令塔＋上部艦橋
      [3,128,180,6,0,0], [4,128,180,4,13,0],                                // 前檣測距儀
      [4,128,226,11,13,0], [4,128,223,8,4,0], [4,128,226,6,13,0], [7,118,232,20,4,4],  // 煙突1
      [4,128,262,10,13,0], [4,128,259,7,4,0], [4,128,262,5,13,0], [7,120,268,16,3,4],  // 煙突2
      [5,116,280,24,26,5], [5,118,285,20,16,4],                             // 後部指揮所2段
      [3,128,284,7,0,0], [3,128,298,6,0,0], [4,128,298,3,13,0],             // 後部管制＋測距儀
      [1,128,330,23,0,0], [2,128,330,21,0,0],                              // 主砲3
      [1,128,372,21,0,0], [2,128,372,19,0,0],                              // 主砲4
    ]),
  },
];

// ---- バンク配置: BGM曲 → 各艦の ops → 各艦のカード画像(事前ベイク64x88 SCREEN5ビットマップ) ----
const bgmBlobs = TRACKS.map(packTrack);
const shipBlobs = SHIPS.map((s) => s.ops);
// カード画像: 生成済み艦から吸い出した 88行×32byte(64px幅) の SCREEN5 生ビットマップ(assets/shipN_card.bin)。
const cardBlobs = SHIPS.map((_, i) => readFileSync(`assets/ship${i}_card.bin`));
const parts = [...bgmBlobs, ...shipBlobs, ...cardBlobs];
const offAll = [];
let cur = 0;
for (const b of parts) { offAll.push(cur); cur += b.length; }
const bgmOff = offAll.slice(0, bgmBlobs.length);
const shipOpsOff = SHIPS.map((_, i) => offAll[bgmBlobs.length + i]);
const shipCardOff = SHIPS.map((_, i) => offAll[bgmBlobs.length + shipBlobs.length + i]);
const bin = Buffer.concat(parts);
if (bin.length > 0x2000) throw new Error(`assets ${bin.length}B > 8KB bank`);
writeFileSync(binOut, bin);

const bgmRamMax = Math.max(...bgmBlobs.map((b) => b.length));
const shipRamMax = Math.max(...shipBlobs.map((b) => b.length));
const h = [
  '/* 自動生成(tools/gen_assets.mjs)。手で編集しない。BGM＋各面の艦体OPS をデータバンクへ。 */',
  '#ifndef ASSETS_DATA_H', '#define ASSETS_DATA_H',
  `#define ASSET_BANK ${BGM_BANK}`,
  '/* --- BGM --- */',
  `#define BGM_BANK ${BGM_BANK}`,
  `#define BGM_TRACK_COUNT ${TRACKS.length}`,
  `#define BGM_RAM_MAX ${bgmRamMax}`,
  `static const unsigned int bgm_notetp[48] = { ${notetp.join(',')} };`,
  `static const unsigned int bgm_off[BGM_TRACK_COUNT] = { ${bgmOff.join(',')} };`,
  `static const unsigned int bgm_len[BGM_TRACK_COUNT] = { ${bgmBlobs.map((b) => b.length).join(',')} };`,
  '/* --- 艦体 OPS(面ごと。data_read で SHIP_OPS_RAM_MAX の RAM へ読み ship_render で解釈) --- */',
  `#define STAGE_COUNT ${SHIPS.length}`,
  `#define SHIP_OPS_RAM_MAX ${shipRamMax}`,
  `static const unsigned int ship_ops_off[STAGE_COUNT] = { ${shipOpsOff.join(',')} };`,
  `static const unsigned int ship_ops_len[STAGE_COUNT] = { ${SHIPS.map((s) => s.ops.length).join(',')} };`,
  `static const unsigned char ship_hull[STAGE_COUNT]  = { ${SHIPS.map((s) => s.hull).join(',')} };`,
  `static const unsigned char ship_bowcnt[STAGE_COUNT] = { ${SHIPS.map((s) => s.bowCnt).join(',')} };`,
  `static const unsigned int ship_bowyb[STAGE_COUNT]  = { ${SHIPS.map((s) => s.bowYb).join(',')} };`,
  '/* --- カード用の事前ベイク艦画像(64x88=88行x32byte の SCREEN5生ビットマップ) --- */',
  `#define SHIP_CARD_LEN ${cardBlobs[0].length}`,
  `static const unsigned int ship_card_off[STAGE_COUNT] = { ${shipCardOff.join(',')} };`,
  '#endif /* ASSETS_DATA_H */', '',
];
writeFileSync(hdrOut, h.join('\n'));
console.log(`assets: ${bin.length}B (bank${BGM_BANK}), ${TRACKS.length} tracks + ${SHIPS.length} ships → ${binOut}, ${hdrOut}`);
