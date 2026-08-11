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
  - 常駐コード 2849B / 24KB(残り21.2KB)、bank4-15 全空き(96KB)。

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
