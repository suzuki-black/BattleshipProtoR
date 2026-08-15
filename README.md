# BattleshipProtoR

turboR専用の縦スクロールSTGエンジン(「1943を凌駕する」本命作)。
前作 [`BattleshipProto`](../BattleshipProto)(コード名「零の咆哮」)と**同じゲームルール＋1つの
ルール変更(各面を「空戦イントロ→戦艦ボス」の2段構成に)**を、**初日から多バンク・データ駆動・
小さな常駐**で設計し直した新エンジン。

- 設計/引き継ぎ: [`../BattleshipProto/HANDOFF-turboR-engine.md`](../BattleshipProto/HANDOFF-turboR-engine.md)
- アーキテクチャ(常駐vsバンクの規律・バンク予算・汎用化): [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

## 状態
- **Phase 0(TODO-1)完了**: ビルド土台 + モジュール分割スケルトン + シーンFSM + Hello VDP。
- **§7-2 進行中**:
  - 汎用エンティティプール＋behavior骨格(scene_boot→scene_demo で bouncer×4)。
  - **H.TIMI 60Hz割込みPSG音ドライバ**。openMSXのRAM/PSGレジスタ直読で「割込み発火(ticks)＋
    エンベロープ実出力(PSG音量の減衰)」を確認済み。
  - **実バンクコール(bcall)**。bank4 の関数を `bcall_to(4)` で実行しRAM証跡(0xE000==0x5A)を確認。
  - **スプライト描画(mode2,16x16)**。エンティティをハードウェアスプライト化。VRAM属性直読＋運動を確認。
  - **run_fire＋emit(データ駆動発砲)**。FireDescから中央射手がリング弾を放射(弾=エンティティ→スプライト)を確認。
  - **run_ops(データ駆動描画)**。op配列から艦体(船体/艦橋/砲)を描画。
- **§7-3 新ルール2段構成プロト**: `SC_BOOT→SC_INTRO→SC_BOSS`。
  - **空戦イントロ**: 縦スクロール海(R#23)＋降下する敵戦闘機＋画面固定の自機(スプライトY補正)。
  - **戦艦ボス**: run_opsのデータ駆動戦艦＋蛇行(横スクロールR#26/27)＋主砲の下向き散弾。
  - openMSX で「海スクロール・戦闘機降下・自機固定・遷移・艦体・蛇行・発砲」を確認。
- **自機操作＋当たり判定**: カーソルで移動・スペースで上方発砲(クールダウン)。自機弾×敵戦闘機で撃破、
  敵弾/戦闘機×自機で被弾(team+16x16 AABB)。openMSXのキー注入で移動(px 120→240)・発砲・撃破(kills 0→1)を確認。
- **スクロール時のVRAMゴミ修正**: 縦スクロールでスプライトテーブル(page0末尾)が可視域に出る問題を、
  海を page1 に描いて page1 表示にすることで根治(スプライトテーブルは page0 のまま非表示)。openMSXで確認。
- **run_fire AIMED(敵が自機を狙う＋散らし)**: 方向を32分割化。AIMED(狙い＋一様乱数の不正確さ円錐)/
  AIMFAN(自機狙いn-way散弾, 偶数は自機直線上に隙間)。前作の「狙いすぎ」反省を、狙い弾へブレを混ぜて公平化
  (出典: dev.to "Simple Bullet Spread for AI", Sparen's Danmaku Design)。ボス主砲=不可視発砲点のAIMFAN、
  空戦の戦闘機半数=AIMED。openMSXで自機左右移動に弾の狙いが追従・被弾(hit 0→5)を確認。
- **冷たいシーンのバンク化(実運用)**: バンクコードが常駐関数(vdp_fill/run_ops/vdp_text等)を
  「常駐シンボル番地の注入(rom.noi→gen_symdefs)」で呼ぶ2パスビルドを確立。title(bank5)/config(bank6)/
  ending(bank7)を bcall で実行。**冷たい画面のコードは常駐24KBを消費しない**。追加は汎用ルール＋1行。
  - **テキスト表示** `vdp_text`(BIOSフォント)を常駐に追加。config/ending の文字はこれで描画。
  - **config**: カーソル/難易度・残機変更/START。difficultyを常駐 g_difficulty へ書き gameplay が参照。
  - フロー boot→title→config→intro→boss→ending→title。openMSXで各バンクシーンの描画・遷移・
    設定変更(NORMAL→HARD)・START→intro を確認。
  - 常駐コード 6321B / 24KB(残り17.8KB)、bank4-7使用、残り 8 バンク空き(64KB)。
- **ボスHP＋撃破判定(§7-4 #1)**: 破壊可能な砲台(ET_TURRET, hp)＋撃破エフェクト(ET_EXPLOSION)。
  自機弾で砲台のhpを削り、全砲台撃破でクリア→ending(HANDOFF §1 の「全砲台撃破=クリア」)。
  openMSXで砲台3基を全撃破→gun_kills 0→3→クリア遷移を確認。
  - **[修正] Makefileヘッダ依存**: 構造体変更後に一部モジュールが再コンパイルされず、新旧の
    構造体レイアウト混在でメモリ破損→ハングする stale-object バグを踏んだ。`$(HDRS)` 依存で恒久修正。
  - 常駐コード 6668B / 24KB(残り17.5KB)。

## ビルド & 起動
```bash
make rom     # → GAME.ROM (128KB MegaROM ASCII8)。常駐24KB超過はビルドエラー＋空き容量表示
make run     # openMSX headless 起動 → build/boot.png にスクショ (既定 C-BIOS_MSX2+_JP)
make clean
```
turboR実機ROM所持時: `make run MACHINE=Panasonic_FS-A1GT`(R800/ラスタ/バンクは最終的に実機確認推奨)。

## 必要ツール
- SDCC (sdcc / sdasz80)
- Node.js (rompack 等)
- openMSX (起動検証。任意)

## ディレクトリ
```
src/crt0rom.s     起動コード＋bcallトランポリン
src/include/      共通ヘッダ(types/msx/vdp/bank/input/scene/sys)
src/core/         常駐モジュール(main/sys/vdp/bank/input/scene)
src/scenes/       シーン(scene_boot = Hello VDP。以降 title/空戦/ボス…を追加)
tools/            rompack.mjs / test_boot.tcl
docs/             ARCHITECTURE.md
```

## 次の一手(HANDOFF §7)
1. (済) ビルド土台＋Hello VDP
2. コアエンジン骨格: エンティティプール＋run_ops＋run_fire＋音ドライバ＋バンクコール
3. 新ルールのプロト: 縦スクロール空戦イントロ→船首出現で蛇行ボスへ遷移(1面)
4. 1面ビスマルクを2段＋ゼロ距離抑え込みで完成→フォーマット確立
5. データ駆動で5艦＋難易度メカを横展開
