/* sprites.h — スプライトパターンの集約。各シーンは init で sprites_load() を1回呼ぶ。
   パターン番号は 4 の倍数(mode2 16x16 は 4パターン=32B 消費)。 */
#ifndef SPRITES_H
#define SPRITES_H

#include "types.h"

#define SPR_BLOCK   0   /* 16x16 中実(自機/汎用マーカ) */
#define SPR_BULLET  4   /* 弾(中央 6x6)                */
#define SPR_FIGHTER 8   /* 敵戦闘機(下向き△)          */
#define SPR_DIGIT0  12  /* 数字0のパターン番号。数字d = SPR_DIGIT0 + d*4(HUD用, BIOSフォント) */
#define SPR_TURRET  52  /* 戦艦の主砲塔(灰の砲塔＋2連装砲身)。数字は12..48を占有→次は52 */

/* 海イントロ敵機=各面ボス艦の所属国の当時の典型機(上面視・機首下向き)。番号は4刻み。
   ハイブリッド識別: 形(主翼の平面形)＋視認性優先色(scene_stage の fighter_col)。 */
#define SPR_BF109    56  /* 独: Bf109 = 細い先細り翼 */
#define SPR_CORSAIR  60  /* 米: F4U = 逆ガル翼(曲がった翼) */
#define SPR_SPITFIRE 64  /* 英: Spitfire = 楕円翼 */
#define SPR_FW190    68  /* 独: Fw190 = 幅広翼＋太い機首 */
#define SPR_HELLCAT  72  /* 米: F6F = 幅広角形の直線翼(ずんぐり) */

/* 自機=零戦(A6M, 機首上向き)。プロペラ回転の2コマ(先頭行を交互)。色は zcol の行別陰影。 */
#define SPR_ZERO    76   /* 零戦 コマA(プロペラ細) */
#define SPR_ZERO2   80   /* 零戦 コマB(プロペラ太=回転ブラー) */
#define SPR_PBULLET 84   /* 自機弾=赤い縦ストリーク(旧pat44) */
extern const u8 zcol[16];   /* 零戦の16行カラーテーブル(緑系+ハイライト) */

void sprites_load(void);   /* 全パターンを VRAM(0x7800)へ投入 */

#endif /* SPRITES_H */
