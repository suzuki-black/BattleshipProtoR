/* hud.h — スプライトHUD(スコア/残機)。
   面表示は page1(スクロールする環状バッファ)なので vdp_text(page0直書き)は使えない。
   代わりに数字をスプライトで先頭スロット(0..HUD_SLOTS-1)に固定表示する。
   スプライトは縦スクロール補正(g_vscroll)済みなので Y を画面座標で置けば画面固定になる。 */
#ifndef HUD_H
#define HUD_H

#include "types.h"

#ifdef DEBUG_FPS
#define HUD_SLOTS 13  /* 通常7 ＋ g_fps2桁(7,8) ＋ フレームカウンタ4桁(9-12)。ent_draw_all は slot13 以降 */
extern u8  g_fps;     /* JIFFY基準の推定FPS(実機turboRでJIFFYが60Hz前提と異なり誤値。参考) */
extern u16 g_frame;   /* ★JIFFY非依存: ループ毎+1のフレームカウンタ。ストップウォッチ実測用(真値) */
#else
#define HUD_SLOTS 7   /* スコア5桁(0-4)＋残機アイコン(5=零戦シルエット)＋残機数(6)。ent_draw_all は slot7 以降 */
#endif

void hud_init(void);                 /* 数字パターン投入＋HUD色＋g_spr_base 確保 */
void hud_draw(u16 score, u8 lives);  /* スコア5桁＋残機を HUD スロットへ(毎フレーム) */

#endif /* HUD_H */
