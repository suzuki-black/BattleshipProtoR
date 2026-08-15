/* scroll.c — 連続縦スクロール地形エンジン。scroll.h 参照。 */
#include "scroll.h"
#include "vdp.h"

#define PAGE1_Y   256           /* 表示リングバッファ(page1)の基準Y */
#define SHIPBUF_Y SC_SHIPBUF_Y  /* 戦艦事前描画バッファB(page2/3)の基準Y */
#define SEA_C     4             /* 海(青) */
#define WAVE_C    5             /* 波(明るい青) */

u16 g_cam;
static s16 drawn_top, drawn_bot;   /* リングに描画済みの世界行レンジ */

/* 世界行 r を page1 リングの該当16pxスロットへ描く。 */
static void draw_row(s16 r) {
    u16 dy = (u16)(PAGE1_Y + (u8)((u16)r << 4));   /* (r*16)&0xFF を page1 へ */
    if (r >= SC_SHIP_R0 && r < SC_SHIP_R1) {
        /* 戦艦行: バッファB から16px strip をコピー */
        vdp_copy(0, (u16)(SHIPBUF_Y + (u16)(r - SC_SHIP_R0) * 16), 0, dy, 256, 16);
    } else {
        /* 海行: 青塗り＋波線 */
        vdp_fill(0, dy, 256, 16, SEA_C);
        vdp_fill(0, dy + 6, 256, 2, WAVE_C);
    }
}

void scroll_init(void) {
    s16 r;
    vdp_set_display_page(1);          /* page1 を表示(リング)。sprite表は page0 で非スクロール */

    /* 艦は呼び出し側が SHIPBUF_Y へ描画済みの前提(scroll.h 参照)。 */

    /* 開始窓(cam=SC_CAM_START の見える16行)を埋める */
    g_cam = SC_CAM_START;
    drawn_top = (s16)(g_cam >> 4);
    drawn_bot = (s16)((g_cam + 211) >> 4);
    for (r = drawn_top; r <= drawn_bot; r++) draw_row(r);
    vdp_set_vscroll((u8)g_cam);
}

void scroll_to(u16 cam) {
    s16 vt = (s16)(cam >> 4);
    s16 vb = (s16)((cam + 211) >> 4);
    /* 下へ露出(cam増)/上へ露出(cam減)の両方向で新規行を流す(往復対応)。窓は16行に保つ。 */
    while (drawn_bot < vb) { drawn_bot++; draw_row(drawn_bot); if (drawn_top < drawn_bot - 15) drawn_top = drawn_bot - 15; }
    while (drawn_top > vt) { drawn_top--; draw_row(drawn_top); if (drawn_bot > drawn_top + 15) drawn_bot = drawn_top + 15; }
    g_cam = cam;
    vdp_set_vscroll((u8)cam);          /* R#23=cam&0xFF。スプライトYも同量補正(画面固定) */
}
