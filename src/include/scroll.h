/* scroll.h — 連続縦スクロール地形エンジン＋疑似多重スクロール海(旧版 BattleshipProto の SEA13 移植)。
   VRAM構成(SCREEN5, y=0..1023):
     - 海テンプレート  : y 512..527 (256x16, 色1地＋2/7ノイズ)。タイル可能な水の一片。
     - 艦バッファB(B)  : y 528..1023 (256x496=31行)。海テンプレを敷いた上に艦を事前描画。
     - 表示リング      : page1(y256..) を 256px リング。R#23=cam&0xFF で縦スクロール。
   露出した16px世界行だけ draw_row=LMMM で page1 の該当スロットへ流し込む。
   SEA13: 毎フレーム1stripぶん、海コラム(艦とその影を避けた範囲)だけを位相 p でテンプレから塗り直し
          =水が艦に対して 0.125px/f で流れる擬似多重スクロール。 */
#ifndef SCROLL_H
#define SCROLL_H

#include "types.h"

/* 戦艦の世界行レンジ(艦高496px=31行)。 */
#define SC_SHIP_R0    2
#define SC_SHIP_ROWS  31                       /* 496px(旧版準拠) */
#define SC_SHIP_R1    (SC_SHIP_R0 + SC_SHIP_ROWS)
#define SC_WORLD_ROWS (SC_SHIP_R1 + 24)        /* 手前の海(空戦イントロ)を含む */
#define SC_CAM_START  ((SC_WORLD_ROWS - 14) * 16)
#define SC_CAM_BOW    (SC_SHIP_R0 * 16)
#define SC_CAM_STERN  (SC_SHIP_R1 * 16 - 212)
#define SC_CAM_SHIP   (SC_SHIP_R1 * 16)

#define SC_SEATMPL_Y  512   /* 海テンプレート(16px) */
#define SC_SHIPBUF_Y  528   /* 戦艦事前描画バッファB(=旧版 B) */

/* 海テンプレート(512)を構築(ノイズ入り)。艦を描く前に呼ぶ(艦がこれを下地にタイルする)。 */
void scroll_build_sea(void);
/* SEA13 初期化(イントロ=全幅アニメ)。艦が出たら sea_set_ship で艦回避帯へ切替。 */
void sea_init(u8 stage);
void sea_set_ship(u8 stage);
/* SEA13 毎フレーム: 1stripの海コラムを位相流し(vdp_frame/scroll_to の後に呼ぶ)。 */
void sea_frame(void);

void scroll_init(void);                 /* 表示page1へ＋開始窓を描画(艦はB既描画前提) */
void scroll_to(u16 cam);                /* cam(世界Y)へ移動。露出行を流し R#23 更新 */

extern u16 g_cam;

#endif /* SCROLL_H */
