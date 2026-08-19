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

import { writeFileSync } from 'node:fs';

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

// ---- 艦体 OPS データ(旧 scene_stage の常駐const を bank へ移設。data_read でRAMへ) ----
// OPS_RECT=1(x,y,w,h,color) / OPS_END=0。ops.h と一致。
const OPS_RECT = 1, OPS_END = 0;
function ops(recs) {
  const b = [];
  for (const r of recs) b.push(OPS_RECT, r[0], r[1], r[2], r[3], r[4]);
  b.push(OPS_END);
  for (const v of b) if (v < 0 || v > 255) throw new Error(`ops byte out of range: ${v}`);
  return Buffer.from(b);
}
// 各面の艦(上面視)。船首側(y0..255)＋船尾側(y0..160=world256..416)の2パス。
// 主砲マウント(黒矩形)は全艦とも 前72/108・後44/88(world300/344) に置く(砲塔スプライトの配置と一致)。
// ★描き込み強化: 基部(船首/船体/甲板/砲塔マウント 前72,108・後44,88)は保持し、
//   三連装砲身・段積み上部構造＋艦橋窓・舷側副砲/AA・煙突ディテール・甲板端の陰影・後檣を追加。
//   色: 船体灰14 / 甲板(ビス暗黄10・アイオワ暗赤6) / 黒1 / 白15(艦橋) / シアン7(窓) / 砲身灰14。
const SHIPS = [
  { name: 'BISMARCK', // 1面: 木甲板(暗黄10)・艦橋1＋煙突1
    top: ops([
      [24, 0, 16, 8, 14], [18, 8, 28, 8, 14], [12, 16, 40, 10, 14], [6, 26, 52, 14, 14],   // 船首テーパ
      [4, 40, 56, 215, 14], [14, 48, 36, 207, 10],                                          // 船体＋木甲板
      [12, 50, 3, 198, 1], [49, 50, 3, 198, 1],                                             // 甲板端の陰(立体感)
      [22, 72, 20, 16, 1], [25, 58, 4, 16, 14], [30, 58, 4, 16, 14], [35, 58, 4, 16, 14],   // A砲塔＋3連装砲身(前へ)
      [22, 108, 20, 16, 1], [25, 94, 4, 16, 14], [30, 94, 4, 16, 14], [35, 94, 4, 16, 14],  // B砲塔(超越)＋砲身
      [6, 130, 7, 9, 14], [47, 130, 7, 9, 14], [8, 152, 5, 5, 1], [47, 152, 5, 5, 1],       // 舷側 副砲/AA
      [13, 124, 38, 54, 14], [16, 127, 32, 47, 1], [21, 133, 22, 22, 15], [24, 139, 16, 7, 7], [27, 158, 10, 12, 1],  // 上部構造(段積み＋窓)
      [17, 192, 30, 44, 14], [20, 196, 24, 34, 1], [22, 200, 20, 7, 14],                    // 煙突(台→本体→キャップ)
      [28, 232, 4, 16, 14],                                                                 // 後檣
    ]),
    bot: ops([
      [4, 0, 56, 120, 14], [14, 0, 36, 116, 10],
      [12, 0, 3, 116, 1], [49, 0, 3, 116, 1],                                               // 甲板端の陰
      [18, 8, 28, 30, 1], [22, 12, 20, 20, 15],                                             // 後部上部構造＋司令塔
      [22, 44, 20, 16, 1], [25, 60, 4, 16, 14], [30, 60, 4, 16, 14], [35, 60, 4, 16, 14],   // C砲塔＋砲身(後へ)
      [22, 88, 20, 16, 1], [25, 104, 4, 16, 14], [30, 104, 4, 16, 14], [35, 104, 4, 16, 14],// D砲塔＋砲身
      [8, 60, 5, 5, 1], [47, 60, 5, 5, 1],                                                  // 舷側AA
      [6, 120, 52, 12, 14], [12, 132, 40, 12, 14], [20, 144, 24, 10, 14], [26, 154, 12, 6, 14],  // 艦尾テーパ
    ]),
  },
  { name: 'IOWA', // 2面: 甲板暗赤6・艦橋(低め)＋煙突2(区別)。マウント位置は同一
    top: ops([
      [24, 0, 16, 8, 14], [18, 8, 28, 8, 14], [12, 16, 40, 10, 14], [6, 26, 52, 14, 14],
      [4, 40, 56, 215, 14], [14, 48, 36, 207, 6],
      [12, 50, 3, 198, 1], [49, 50, 3, 198, 1],
      [22, 72, 20, 16, 1], [25, 58, 4, 16, 14], [30, 58, 4, 16, 14], [35, 58, 4, 16, 14],
      [22, 108, 20, 16, 1], [25, 94, 4, 16, 14], [30, 94, 4, 16, 14], [35, 94, 4, 16, 14],
      [6, 130, 7, 9, 14], [47, 130, 7, 9, 14], [8, 152, 5, 5, 1], [47, 152, 5, 5, 1],
      [13, 122, 38, 38, 14], [16, 124, 32, 32, 1], [21, 128, 22, 17, 15], [24, 133, 16, 6, 7],  // 上部構造(低め)＋窓
      [17, 160, 26, 22, 14], [20, 162, 20, 18, 1], [22, 166, 16, 6, 14],                    // 煙突1
      [17, 196, 26, 22, 14], [20, 198, 20, 18, 1], [22, 202, 16, 6, 14],                    // 煙突2(2本=識別)
    ]),
    bot: ops([
      [4, 0, 56, 120, 14], [14, 0, 36, 116, 6],
      [12, 0, 3, 116, 1], [49, 0, 3, 116, 1],
      [18, 8, 28, 24, 1], [22, 12, 20, 14, 15],
      [22, 44, 20, 16, 1], [25, 60, 4, 16, 14], [30, 60, 4, 16, 14], [35, 60, 4, 16, 14],
      [22, 88, 20, 16, 1], [25, 104, 4, 16, 14], [30, 104, 4, 16, 14], [35, 104, 4, 16, 14],
      [8, 60, 5, 5, 1], [47, 60, 5, 5, 1],
      [6, 120, 52, 12, 14], [12, 132, 40, 12, 14], [20, 144, 24, 10, 14], [26, 154, 12, 6, 14],
    ]),
  },
];

// ---- バンク配置: BGM曲 → 各艦の top/bot の順に連結 ----
const bgmBlobs = TRACKS.map(packTrack);
const shipBlobs = [];
for (const s of SHIPS) { shipBlobs.push(s.top, s.bot); }
const parts = [...bgmBlobs, ...shipBlobs];
const offAll = [];
let cur = 0;
for (const b of parts) { offAll.push(cur); cur += b.length; }
const bgmOff = offAll.slice(0, bgmBlobs.length);
const shipTopOff = SHIPS.map((_, i) => offAll[bgmBlobs.length + i * 2]);
const shipBotOff = SHIPS.map((_, i) => offAll[bgmBlobs.length + i * 2 + 1]);
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
  '/* --- 艦体 OPS(面ごと。data_read で SHIP_OPS_RAM_MAX の RAM へ読み run_ops) --- */',
  `#define STAGE_COUNT ${SHIPS.length}`,
  `#define SHIP_OPS_RAM_MAX ${shipRamMax}`,
  `static const unsigned int ship_top_off[STAGE_COUNT] = { ${shipTopOff.join(',')} };`,
  `static const unsigned int ship_top_len[STAGE_COUNT] = { ${SHIPS.map((s) => s.top.length).join(',')} };`,
  `static const unsigned int ship_bot_off[STAGE_COUNT] = { ${shipBotOff.join(',')} };`,
  `static const unsigned int ship_bot_len[STAGE_COUNT] = { ${SHIPS.map((s) => s.bot.length).join(',')} };`,
  '#endif /* ASSETS_DATA_H */', '',
];
writeFileSync(hdrOut, h.join('\n'));
console.log(`assets: ${bin.length}B (bank${BGM_BANK}), ${TRACKS.length} tracks + ${SHIPS.length} ships → ${binOut}, ${hdrOut}`);
