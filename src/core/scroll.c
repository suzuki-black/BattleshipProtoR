/* scroll.c — 連続縦スクロール地形＋疑似多重スクロール海(旧版 SEA13 移植)。scroll.h 参照。 */
#include "scroll.h"
#include "vdp.h"

#define PAGE1_Y   256           /* 表示リング(page1)の基準Y */

u16 g_cam;
static s16 drawn_top, drawn_bot;

/* 海テンプレート用の乱数(旧版と同LCG。斑点の見た目のみ) */
static u16 srng = 12345;
static u8 srnd(void) { srng = (u16)(srng * 25173 + 13849); return (u8)(srng >> 8); }

/* 海テンプレート(512, 256x16)を構築: 色1地＋色2を420点＋色7を320点(継目を隠す粗ノイズ)。 */
void scroll_build_sea(void) {
    u16 k;
    vdp_fill(0, SC_SEATMPL_Y, 256, 16, 1);
    for (k = 0; k < 420; k++) vdp_fill(srnd(), (u16)(SC_SEATMPL_Y + (srnd() & 15)), 1, 1, 2);
    for (k = 0; k < 320; k++) vdp_fill(srnd(), (u16)(SC_SEATMPL_Y + (srnd() & 15)), 1, 1, 7);
}

/* 世界行 r を page1 リングの該当16pxスロットへ。艦行=バッファB, それ以外=海テンプレ。 */
static void draw_row(s16 r) {
    u16 dy = (u16)(PAGE1_Y + (u8)((u16)r << 4));
    if (r >= SC_SHIP_R0 && r < SC_SHIP_R1)
        vdp_copy(0, (u16)(SC_SHIPBUF_Y + (u16)(r - SC_SHIP_R0) * 16), 0, dy, 256, 16);
    else
        vdp_copy(0, SC_SEATMPL_Y, 0, dy, 256, 16);
}

/* ===== SEA13: 海コラムだけ位相流し(艦とその影は不可侵=帯で避ける) ===== */
static u16 sea_phase;
static u8  sea_acc, sea_strip;
/* {startX,width,...} 艦＋影を避けた海コラム(艦種別に手調整)。順=BB/Iowa/Carrier/Hood/Twins。 */
static const u8 sea_bb[4] = { 0, 72, 196, 60 };            /* ビスマルク/アイオワ(半幅54): x0..72 と x196.. */
static const u8 sea_cv[4] = { 0, 66, 190, 66 };            /* 空母(半幅58,対称影) */
static const u8 sea_hd[4] = { 0, 80, 190, 66 };            /* フッド(半幅46) */
static const u8 sea_tw[6] = { 0, 50, 116, 38, 220, 36 };   /* 双子: 左/船間/右の3帯 */
static const u8 *sea_ranges = sea_bb;
static u8 sea_nranges = 2;

void sea_init(u8 stage) {
    switch (stage) {
        case 2:  sea_ranges = sea_cv; sea_nranges = 2; break;   /* 空母 */
        case 3:  sea_ranges = sea_hd; sea_nranges = 2; break;   /* フッド */
        case 4:  sea_ranges = sea_tw; sea_nranges = 3; break;   /* 双子(3帯) */
        default: sea_ranges = sea_bb; sea_nranges = 2; break;   /* 0=BB / 1=Iowa */
    }
    sea_phase = 0; sea_acc = 0; sea_strip = 0;
}

/* 1帯を16px周期wrapでテンプレから塗る(上[p..16]＋下[0..p])。source X=dest X=YMMM相当。 */
static void sea13_paint_range(u16 vy, u8 rx, u8 rw, u8 p) {
    u8 t = (u8)(16 - p);
    vdp_copy(rx, (u16)(SC_SEATMPL_Y + p), rx, vy, rw, t);
    if (p) vdp_copy(rx, SC_SEATMPL_Y, rx, (u16)(vy + t), rw, p);
}

void sea_frame(void) {
    u16 vy = (u16)(PAGE1_Y + (u16)sea_strip * 16);
    u8 p = (u8)(sea_phase & 15), i;
    for (i = 0; i < sea_nranges; i++)
        sea13_paint_range(vy, sea_ranges[2 * i], sea_ranges[2 * i + 1], p);
    sea_strip = (u8)((sea_strip + 7) & 15);      /* 歩幅7(16と互素)=掃引を散らす */
    if (++sea_acc >= 8) { sea_acc = 0; sea_phase += 1; }   /* 0.125px/f */
}

void scroll_init(void) {
    s16 r;
    vdp_set_display_page(1);          /* page1 リングを表示。スプライト表は page0 で非スクロール */
    /* sea_init(stage) は呼び元(scene_stage)が艦種に合わせて呼ぶ */

    g_cam = SC_CAM_START;
    drawn_top = (s16)(g_cam >> 4);
    drawn_bot = (s16)((g_cam + 211) >> 4);
    for (r = drawn_top; r <= drawn_bot; r++) draw_row(r);
    vdp_set_vscroll((u8)g_cam);
}

void scroll_to(u16 cam) {
    s16 vt = (s16)(cam >> 4);
    s16 vb = (s16)((cam + 211) >> 4);
    while (drawn_bot < vb) { drawn_bot++; draw_row(drawn_bot); if (drawn_top < drawn_bot - 15) drawn_top = drawn_bot - 15; }
    while (drawn_top > vt) { drawn_top--; draw_row(drawn_top); if (drawn_bot > drawn_top + 15) drawn_bot = drawn_top + 15; }
    g_cam = cam;
    vdp_set_vscroll((u8)cam);
}
