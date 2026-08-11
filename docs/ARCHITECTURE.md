# アーキテクチャ — turboR専用エンジン(BattleshipProtoR)

> 前作 `BattleshipProto` の最大の教訓＝**32KBコード窓の硬い天井**と**巨大 main()**。
> 本エンジンは初手から「**スロット/バンク割り当てを意識した小モジュールの集合体**」＋
> 「**ゲームアルゴリズム/シーン管理の汎用化**」で作り、各ステージのバンク空きに常に余裕を残す。

---

## 1. メモリ地図(Z80 64KB空間 と ASCII8 MegaROM)

```
Z80アドレス空間                         ASCII8 MegaROM(128KB = 16 bank × 8KB)
┌───────────────────────────┐          ┌──────────────────────────────────┐
│ 0x0000-0x3FFF page0 = BIOS │          │ bank0  ROM 0x00000  ┐             │
│ 0x4000-0x5FFF ← bank0      │◀────────▶│ bank1  ROM 0x02000  ├ 常駐コード   │
│ 0x6000-0x7FFF ← bank1      │  ASCII8  │ bank2  ROM 0x04000  ┘ (<=24KB)    │
│ 0x8000-0x9FFF ← bank2      │  4窓      │ bank3  ROM 0x06000  = スワップ窓予約 │
│ 0xA000-0xBFFF ← スワップ窓  │◀────────▶│ bank4..15 ROM 0x08000.. = 冷コード  │
│ 0xC000-0xFFFF page3 = RAM  │          │                        ＋データ    │
└───────────────────────────┘          └──────────────────────────────────┘
   窓の切替は 0x6000/0x6800/0x7000/0x7800 への書込。0x7800 = 0xA000窓(スワップ)。
```

- **bank0-2 = 常駐コード(0x4010-0x9FFF, 24KB上限)**: 起動時に固定で居続ける“熱い”コード。
  `rompack.mjs` が **24KB超過をビルドエラー**にし、毎ビルドで残量を表示する(硬い天井を機械強制)。
- **bank3 = スワップ窓の既定ページ**: 前作はここまでコードに使い窓を潰した=失敗。**本作は温存**。
  データ先読み/バンクコールのときだけ 0xA000 窓を別バンクへ差替え、済んだら bank3 に戻す。
- **bank4-15 = 冷たいコード＋データ(各8KB)**: シーン別演出/設定/エンディング等の“冷たい”コードと、
  艦/発砲スクリプト/BGM/文字列/スプライトパターン等のデータ。ステージを足すほどここを使う。

---

## 2. 常駐 vs バンク — 何をどこに置くか(規律)

### 常駐(bank0-2, 24KB)に置いてよいもの＝“毎フレーム/割込みで必ず要る核”だけ
- crt0 + `_bcall` トランポリン(`src/crt0rom.s`)
- VDP アクセス(`vdp.c`) / バンク切替(`bank.c`) / 入力(`input.c`)
- シーンFSM ディスパッチャ(`scene.c`) / 起動初期化(`sys.c`) / エントリ(`main.c`)
- (将来) H.TIMI 60Hz 割込み音ドライバ ISR / エンティティプールの update・draw ホットループ /
  run_ops(描画IF) と run_fire(発砲IF) の**インタプリタ本体**

### バンク(bank4+)へ回すもの＝“冷たい/一度きり/データ”
- 各シーンの重い init/draw(title, 空戦イントロ setup, 撃破演出, ending, gameover, 設定メニュー)
- 艦の `ship_ops`、発砲スクリプト `FireDesc`、敵配置/パターン、BGM音符、文字列、スプライト定義
- → コードが冷たいなら **bcall**、データなら **bank_data で先読み**。

### バンクコード3原則(破ると暴走)
1. **呼び先は 0xA000 単一エントリで自己完結** — 常駐関数を直接呼ばない/データ窓(0xA000)を読まない。
2. **呼び元は必ず常駐(<0xA000)** — 切替中に 0xA000 のコードを踏まない。
3. **切替中は di、済んだら bank3 へ復元** — `_bcall` が担保(`crt0rom.s`)。

---

## 3. 汎用化した仕組み(初手から)

### 3.1 シーンFSM (`scene.h` / `scene.c`)
`Scene { init(); update()->次ID; bank }` の表 `registry[]` を1か所に集約。
`scene_run()` が「入場時 init 1回 → 毎フレーム update → 戻り値で遷移」を回す。**巨大 main() を作らない**。
- 冷たいシーンは将来 `bank != 0` にして、ディスパッチャが `g_bank=bank; bcall()` で当該バンクの
  0xA000 エントリを呼ぶ(そのエントリが `g_scene_phase` で init/update を分岐)。今は常駐シーンのみ実装。

### 3.2 バンクコール (`bank.h` / `crt0rom.s`)
`g_bank = <bank>; bcall();` で任意バンクの 0xA000 エントリを実行(トランポリンは常駐)。実機確定土台。

### 3.3 データ駆動(これから載せる、置き場所だけ先に確保)
- **run_ops**(描画データ駆動): 艦/敵/背景を op配列で描く。データは bank。
- **run_fire**(発砲スクリプト): `FireDesc[interval,rage,telegraph,suppress, emit-ops..., 0]`。
  難易度メカ(予告/レイジ/ゼロ距離抑え込み/固定弾安置)を**全部データ**に。仕様: `docs/fire-script-spec.md`(前作から移植予定)。
- **emit**(弾生成プリミティブ): `emit(px,py,dir,kind,spd)`。全発砲サイトを共通化。
- **エンティティ・プール**: 自機/敵機/弾/砲/エフェクトを汎用プール＋behavior(type別 update/draw)。
  空戦の敵機も戦艦の砲も同じ枠で扱う。

---

## 4. ビルド系(`Makefile` / `config.mk` / `tools/rompack.mjs`)

- 各常駐モジュールは**個別 .c → 個別 .rel**。`Makefile` の `RESIDENT_RELS` に列挙してリンク。
  → SDCC のレジスタ割当破綻/ビルド遅延の温床(巨大単一TU)を避ける。追加は1行。
- 冷たいコード: 単独コンパイル(`--code-loc 0xA000`)→ `--bank N build/xxx.ihx` で rompack がバンクへ格納。
- データ: `--bank N file.bin` で任意バンクへ。
- `rompack` 出力例(空き容量の可視化):
  ```
  常駐コード(bank0-2): 686B / 24576B  残り 23890B (23.3KB)
  bank3(スワップ窓)  : 予約(既定 0xFF)
  空きバンク(bank4-15): 12 / 12  (= 96.0KB 未使用)
  ```

### turboR 専用の前提
- 起動時 `sys_init()` が **R800(ROMモード)へブースト**(version<3 では自動スキップ=安全)。
- SDCC の生成命令は Z80(実績)。`-mr800` は実験扱いで当面不採用。CPU速度/RAM増の恩恵は
  「弾/敵の大幅増(真の弾幕)」「誘導/曲線弾など重い計算」「プール/バッファ拡大」で受ける。
- **MSX2+ 互換は捨てる**(割込み/速度前提を専用化)。

---

## 5. 実機/エミュ検証

- ビルド: `make rom` → `GAME.ROM`(128KB ASCII8)。
- 起動検証: `make run`(openMSX headless, 既定 `C-BIOS_MSX2+_JP`)→ `build/boot.png`。
  - turboR実機ROM(`Panasonic_FS-A1GT`)は著作物のため未同梱 → **R800経路/ラスタ/バンク切替の
    タイミングは最終的に実機 or turboR ROM で要確認**(前作の引き継ぎ通り)。
- 途中で踏んだ罠を土台に記録済み:
  - **塗りは LMMV(0x80, ピクセル単位)**。HMMV(0xC0)はバイト単位で G4 では縞になる。
  - **`vdp_cmd_wait` は di 保護必須**。S#2選択中に割込が入ると ISR が割込フラグを消せずハングする。

---

## 6. モジュール一覧(現在)

| 区分 | ファイル | 役割 |
|------|----------|------|
| 起動 | `src/crt0rom.s` | "AB"ヘッダ / page2有効化 / ASCII8窓初期化 / `_bcall` |
| 常駐 | `src/core/main.c` | エントリ(初期化→FSM委譲のみ) |
| 常駐 | `src/core/sys.c` | R800ブースト等の起動初期化 |
| 常駐 | `src/core/vdp.c` | VDPレジスタ/パレット/VRAM/コマンド(LMMV)/フレーム待ち |
| 常駐 | `src/core/bank.c` | バンク切替 / `g_bank` / bcall glue |
| 常駐 | `src/core/input.c` | カーソル/トリガ入力(row8直読み) |
| 常駐 | `src/core/scene.c` | シーンFSM ディスパッチャ＋registry |
| シーン | `src/scenes/scene_boot.c` | Hello VDP(疎通確認。将来 title へ差替) |
| ツール | `tools/rompack.mjs` | .ihx＋バンク → MegaROM。常駐24KB超過をエラー、空き表示 |
| ツール | `tools/test_boot.tcl` | openMSX headless 起動スクショ |
