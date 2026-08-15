/* sprites.h — スプライトパターンの集約。各シーンは init で sprites_load() を1回呼ぶ。
   パターン番号は 4 の倍数(mode2 16x16 は 4パターン=32B 消費)。 */
#ifndef SPRITES_H
#define SPRITES_H

#include "types.h"

#define SPR_BLOCK   0   /* 16x16 中実(自機/汎用マーカ) */
#define SPR_BULLET  4   /* 弾(中央 6x6)                */
#define SPR_FIGHTER 8   /* 敵戦闘機(下向き△)          */
#define SPR_DIGIT0  12  /* 数字0のパターン番号。数字d = SPR_DIGIT0 + d*4(HUD用, BIOSフォント) */

void sprites_load(void);   /* 全パターンを VRAM(0x7800)へ投入 */

#endif /* SPRITES_H */
