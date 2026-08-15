/* scroll.h — 連続縦スクロール地形エンジン(海→戦艦 地続き。画面カット無し)。
   前作cportの実証手法を移植:
     - 表示 = page1 を 256px リングバッファ。R#23 = cam&0xFF で縦スクロール。
     - 戦艦は上位VRAM(バッファB=page2)へ run_ops で一度だけ事前描画。
     - 露出した16px世界行だけ draw_row=LMMM で page1 の該当スロットへ流し込む(高速)。
     - スプライト表は page0(0x7600)で非スクロール→スクロールでゴミが出ない。
   世界は16px行。手前の海→戦艦(船首→船尾)→船尾後の海。cam(世界Y)を前進/往復させる。 */
#ifndef SCROLL_H
#define SCROLL_H

#include "types.h"

/* 戦艦の世界行レンジ。艦は画面より長く(往復蛇行のため)。船首=先頭行, 船尾=末尾行。 */
#define SC_SHIP_R0    2                        /* 戦艦の先頭世界行(船首) */
#define SC_SHIP_ROWS  26                       /* 戦艦の行数(416px, 画面より長い) */
#define SC_SHIP_R1    (SC_SHIP_R0 + SC_SHIP_ROWS)   /* =28 */
#define SC_WORLD_ROWS (SC_SHIP_R1 + 16)        /* 手前の海(=開始側)を含む世界行数 */
#define SC_CAM_START  ((SC_WORLD_ROWS - 14) * 16)   /* 開始=手前の海(戦艦の先) */
#define SC_CAM_BOW    (SC_SHIP_R0 * 16)             /* 船首が画面上端(往復の下限) */
#define SC_CAM_STERN  (SC_SHIP_R1 * 16 - 212)       /* 船尾が画面下端(往復の上限=交戦開始) */

#define SC_SHIPBUF_Y  512   /* 戦艦事前描画バッファB(page2/3)の基準Y。艦はここへ run_ops で描く */

/* 呼び出し側は scroll_init の前に SC_SHIPBUF_Y へ艦を描いておくこと(OPS_RECTのyはu8=255まで
   なので、255超の高い艦は run_ops を SC_SHIPBUF_Y と SC_SHIPBUF_Y+256 の2パスで描く)。 */
void scroll_init(void);                 /* 表示page1へ＋開始窓を描画(艦はB既描画前提) */
void scroll_to(u16 cam);                /* cam(世界Y)へ移動。露出行を流し R#23 更新 */

extern u16 g_cam;

#endif /* SCROLL_H */
