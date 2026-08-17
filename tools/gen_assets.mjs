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

const blobs = TRACKS.map(packTrack);
const off = [];
let cur = 0;
for (const b of blobs) { off.push(cur); cur += b.length; }
const bin = Buffer.concat(blobs);
if (bin.length > 0x2000) throw new Error(`assets ${bin.length}B > 8KB bank`);
writeFileSync(binOut, bin);

const ramMax = Math.max(...blobs.map((b) => b.length));
const h = [
  '/* 自動生成(tools/gen_assets.mjs)。手で編集しない。 */',
  '#ifndef BGM_DATA_H', '#define BGM_DATA_H',
  `#define BGM_BANK ${BGM_BANK}`,
  `#define BGM_TRACK_COUNT ${TRACKS.length}`,
  `#define BGM_RAM_MAX ${ramMax}`,
  `static const unsigned int bgm_notetp[48] = { ${notetp.join(',')} };`,
  `static const unsigned int bgm_off[BGM_TRACK_COUNT] = { ${off.join(',')} };`,
  `static const unsigned int bgm_len[BGM_TRACK_COUNT] = { ${blobs.map((b) => b.length).join(',')} };`,
  '#endif /* BGM_DATA_H */', '',
];
writeFileSync(hdrOut, h.join('\n'));
console.log(`assets: ${bin.length}B (bank${BGM_BANK}), ${TRACKS.length} tracks, RAM_MAX=${ramMax}B → ${binOut}, ${hdrOut}`);
