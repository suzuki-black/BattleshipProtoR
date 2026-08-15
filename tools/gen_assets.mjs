// gen_assets.mjs — BGM 曲データ(＋将来の他アセット)をデータバンク bin へパック。
//   出力: build/assets.bin   … データバンク(rompack が --bank へ生配置)の内容
//         build/bgm_data.h   … 常駐Cが読む定数(音階periodテーブル/曲オフセット/長さ)
//
//   曲データ(トラック)1本のバイト列(=バンクに置き、再生時に data_read で RAM へコピー):
//     [ nMel, nBas, melNote(nMel), melLen(nMel), basNote(nBas) ]
//       melNote/basNote = 音階index(0=C2..47=B5) or 255=休符
//       melLen          = その音符のフレーム数(60Hz)。bass は固定ステップ(engine: BAS_STEP)
//
// 使い方: node tools/gen_assets.mjs <bank番号> build/assets.bin build/bgm_data.h

import { writeFileSync } from 'node:fs';

const [, , bankArg, binOut, hdrOut] = process.argv;
const BGM_BANK = parseInt(bankArg ?? '8', 10);

// ---- 音階名 → index(0=C2 .. 47=B5) / 休符=255 ----
const SEMI = { C:0,'C#':1,D:2,'D#':3,E:4,'F':5,'F#':6,G:7,'G#':8,A:9,'A#':10,B:11 };
function nidx(s) {
  if (s === 'R') return 255;
  const m = s.match(/^([A-G]#?)(\d)$/);
  if (!m) throw new Error(`bad note: ${s}`);
  const v = (parseInt(m[2], 10) - 2) * 12 + SEMI[m[1]];
  if (v < 0 || v > 47) throw new Error(`note out of C2..B5: ${s}`);
  return v;
}

// ---- PSG tone period テーブル(12bit)。TP = 1.7897725MHz/(16*freq) ----
const notetp = [];
for (let i = 0; i < 48; i++) {
  const freq = 440 * Math.pow(2, (36 + i - 69) / 12);   // midi=36+i, A4(midi69)=440Hz
  notetp.push(Math.round(111860.78125 / freq));
}

// ---- 曲データ(手書き。melody=[名,長], bass=[名](固定ステップ)) ----
const TRACKS = [
  { // 0: タイトル(荘厳・行進曲風, Cメジャー)
    mel: [['G4',8],['G4',8],['C5',16],['E5',8],['D5',8],['C5',8],['B4',8],
          ['C5',16],['G4',16],['A4',8],['B4',8],['C5',16],['D5',8],['C5',8],['B4',8],['G4',16]],
    bas: ['C3','C3','G3','G3','F3','F3','G3','G3'],
  },
  { // 1: ステージ(疾走・行進, Cメジャー)
    mel: [['C4',8],['E4',8],['G4',8],['C5',8],['A4',8],['F4',8],['A4',8],['C5',8],
          ['G4',8],['B4',8],['D5',8],['G4',8],['E4',8],['G4',8],['C5',8],['E5',8]],
    bas: ['C3','C3','A2','A2','F2','F2','G2','G2'],
  },
];

// ---- トラックをバイト列へ ----
function packTrack(t) {
  const mel = t.mel.map(([n]) => nidx(n));
  const mln = t.mel.map(([, l]) => l);
  const bas = t.bas.map((n) => nidx(n));
  return Buffer.from([mel.length, bas.length, ...mel, ...mln, ...bas]);
}

const blobs = TRACKS.map(packTrack);
const off = [];
let cur = 0;
for (const b of blobs) { off.push(cur); cur += b.length; }
const bin = Buffer.concat(blobs);
if (bin.length > 0x2000) throw new Error(`assets ${bin.length}B > 8KB bank`);
writeFileSync(binOut, bin);

// ---- 常駐C用ヘッダ ----
const ramMax = Math.max(...blobs.map((b) => b.length));
const h = [];
h.push('/* 自動生成(tools/gen_assets.mjs)。手で編集しない。 */');
h.push('#ifndef BGM_DATA_H');
h.push('#define BGM_DATA_H');
h.push(`#define BGM_BANK ${BGM_BANK}`);
h.push(`#define BGM_TRACK_COUNT ${TRACKS.length}`);
h.push(`#define BGM_RAM_MAX ${ramMax}`);
h.push(`static const unsigned int bgm_notetp[48] = { ${notetp.join(',')} };`);
h.push(`static const unsigned int bgm_off[BGM_TRACK_COUNT] = { ${off.join(',')} };`);
h.push(`static const unsigned char bgm_len[BGM_TRACK_COUNT] = { ${blobs.map((b) => b.length).join(',')} };`);
h.push('#endif /* BGM_DATA_H */');
h.push('');
writeFileSync(hdrOut, h.join('\n'));

console.log(`assets: ${bin.length}B (bank${BGM_BANK}), ${TRACKS.length} tracks, RAM_MAX=${ramMax}B → ${binOut}, ${hdrOut}`);
