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
- H.TIMI 60Hz 割込み音ドライバ ISR(実装済 `sound.c`) / エンティティプールの update・draw(実装済 `entity.c`)
- run_ops(描画IF `ops.c`) と run_fire(発砲IF `fire.c`)の**インタプリタ本体**(骨格実装済)

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
- **現在のフロー**: `SC_BOOT`(疎通) → `SC_INTRO`(★空戦イントロ=縦スクロールのみ) → `SC_BOSS`(★戦艦ボス=蛇行)。
  ★新ルール「各面=空戦イントロ→戦艦ボスの2段」を1面ぶんプロト実装(HANDOFF §2/§7-3)。
- 冷たいシーンは将来 `bank != 0` にして、ディスパッチャが `g_bank=bank; bcall()` で当該バンクの
  0xA000 エントリを呼ぶ(そのエントリが `g_scene_phase` で init/update を分岐)。今は常駐シーンのみ実装。

### 3.2 バンクコール (`bank.h` / `crt0rom.s`) ← 新ビルドでE2E実証済み
`bcall_to(<bank>)`(= `g_bank=<bank>; bcall();`)で任意バンクの 0xA000 エントリを実行(トランポリンは常駐)。
実証: `bank_demo.c` を bank4 に格納 → `bcall_to(4)` → RAM 0xE000 に 0x5A 書込を openMSX で確認。
冷たいシーンはこの枠(0xA000 単一エントリで自己完結)に載せ、常駐窓を食わない。

### 3.3 データ駆動(骨格実装済み。難易度メカ等はこれから拡張)
- **run_ops**(描画データ駆動 `ops.c`): 艦/敵/背景を op配列で描く。現状 OPS_RECT のみ(艦=船体/艦橋/砲の矩形)。
  将来 op を増やし(ライン/三角/艦橋段積み/パターン転送)、データは bank へ。
- **run_fire**(発砲スクリプト `fire.c`): `FireDesc{interval, [op,a,kind,spd].., 0}`。op: FIXED/RING/**AIMED/AIMFAN**。
  方向は**32分割**(11.25°)。将来 予告/レイジ/ゼロ距離抑え込み/固定弾安置を**データで**追加。原典: 前作 `docs/fire-script-spec.md`。
  - **AIMED**(自機狙い＋散らし): 狙い方向に**一様乱数±a ステップの円錐**を足す(「不正確さの円錐」)。a=0で厳密狙い。
  - **AIMFAN**(自機狙い n-way): 自機中心に a発を2ステップ間隔で扇状＋扇全体を乱数微回転。**偶数aは自機直線上に隙間**。
  - 設計意図: 前作「狙いすぎ」反省 → 狙い弾に**ブレ/スプレッド**を混ぜ公平化。出典: dev.to "Simple Bullet Spread for AI"(aim+uniform offset)、Sparen's Danmaku Design(aimed patternの inconsistency)。難易度で散らし量を可変にできる(将来)。
- **emit**(弾生成プリミティブ `fire.c`): `emit(x,y,dir,kind,spd)`。32分割方向×弾速で ET_BULLET を1発生成。`aim_dir()` で自機への最近傍方向。

### 3.4 スクロール (`vdp.c`)
- **縦スクロール R#23**(空戦イントロの海): VRAM全体を縦シフト=**スプライトにも効く**。`vdp_set_vscroll` が量を保持し
  `vdp_sprite_pos` が Y に加算 → 自機/敵を画面固定に見せる。海は256行を波線付きで seamless に。
  - **ゴミ対策(重要)**: スプライトテーブルは page0 末尾(0x7400-0x7FFF=ライン232-255)。縦スクロールでこの帯が
    可視域に回り込むと画面中央にゴミが出る。→ **海を page1(0x8000+, VDPコマンドは DY に +256)へ描き page1 を表示**。
    スプライトテーブルは page0 のまま=表示されない。`vdp_set_display_page(1)`。ボスは page0(R#23=0で無縁)。
- **横スクロール R#26/27**(ボスの蛇行): スプライト非影響。艦体(VRAM)を左右に揺らす。
  ※プロト簡略化: 弾(スプライト)の発射原点は揺れに追従しない。本番で砲塔追従を入れる。

### 3.5 自機・当たり判定 (`player.c` / `entity.c`)
- **自機**(ET_PLAYER, `player.c`): `g_input` で移動＋クランプ、トリガでクールダウン付き上方発砲(TEAM_PLAYER弾)。
  現在位置を `g_player_x/y` に公開(AIMED/UIが参照)。
- **当たり判定**(`ent_resolve_collisions`): 自機弾×敵戦闘機→両消滅・`g_kills`++、敵弾/戦闘機×自機→`g_playerhit`++。
  16x16 AABB(甘めマージン)。弾の帰属は `Entity.team`(emit=TEAM_ENEMY / 自機発砲=TEAM_PLAYER)。

> **[解決済み] スクロール時のスプライトテーブル露出ゴミ**: 縦スクロールで page0末尾(ライン232-255)の
> スプライトテーブルが可視域に出る問題。→ 海を page1 に描き page1 を表示(§3.4)。イントロで実機(openMSX)確認済み。
- **エンティティ・プール**(実装済み骨格 `entity.c`): 自機/敵機/弾/砲/エフェクトを固定長プール＋
  behavior(type別 update)の関数ポインタ表で回す。空戦の敵機も戦艦の砲も同じ枠。
  描画は**ハードウェアスプライト(mode2, 16x16)**。active を先頭スロットへ詰めて属性/色を書き、
  残りは停止マーカ(Y=208)で隠す(ハード合成なので消去不要)。大きな艦体は run_ops(将来)で描く。

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
  - **スプライトテーブルは SCREEN5 の BIOS 既定(属性0x7600/色0x7400/パターン0x7800)を使う**。
    高位VRAMへの相対再配置(R#5/6/11で0xF780等)は本環境(C-BIOS/openMSX)で描画に反映されなかった。

---

## 6. モジュール一覧(現在)

| 区分 | ファイル | 役割 |
|------|----------|------|
| 起動 | `src/crt0rom.s` | "AB"ヘッダ / page2有効化 / ASCII8窓初期化 / `_bcall` |
| 常駐 | `src/core/main.c` | エントリ(初期化→FSM委譲のみ) |
| 常駐 | `src/core/sys.c` | R800ブースト等の起動初期化 |
| 常駐 | `src/core/vdp.c` | VDPレジスタ/パレット/VRAM/LMMV/フレーム待ち/スプライト(mode2)/スクロール(R#23,26,27) |
| 常駐 | `src/core/bank.c` | バンク切替 / `g_bank` / bcall glue |
| 常駐 | `src/core/input.c` | カーソル/トリガ入力(row8直読み) |
| 常駐 | `src/core/sound.c` | PSG効果音＋H.TIMI 60Hz割込みISR(BGMは#2後) |
| 常駐 | `src/core/entity.c` | 汎用エンティティプール＋behavior＋スプライト描画＋当たり判定(team) |
| 常駐 | `src/core/player.c` | 自機(ET_PLAYER): 入力で移動＋発砲、位置を公開(AIMED/UI用) |
| 常駐 | `src/core/fire.c` | emit＋run_fire(FIXED/RING/AIMED/AIMFAN, 32分割, 狙い散らし) |
| 常駐 | `src/core/ops.c` | データ駆動描画 run_ops(艦体等) |
| 常駐 | `src/core/sprites.c` | スプライトパターン定義＋一括投入(sprites_load) |
| 常駐 | `src/core/scene.c` | シーンFSM ディスパッチャ＋registry |
| バンク | `src/banked/bank_demo.c` | 実バンクコール実証(bank4, 0xA000エントリ, 自己完結) |
| シーン | `src/scenes/scene_boot.c` | Hello VDP(疎通確認)→SC_INTROへ遷移 |
| シーン | `src/scenes/scene_intro.c` | ★空戦イントロ(縦スクロール海＋降下戦闘機＋自機固定)→SC_BOSS |
| シーン | `src/scenes/scene_boss.c` | ★戦艦ボス(run_ops艦体＋蛇行横スクロール＋主砲散弾) |
| ツール | `tools/rompack.mjs` | .ihx＋バンク → MegaROM。常駐24KB超過をエラー、空き表示 |
| ツール | `tools/test_{boot,sound,bank,spr,stage}.tcl` | openMSX headless 検証(起動/音/バンク/スプライト/2段構成) |
