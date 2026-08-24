/* ship_aag.c — 艦システムの「常駐」部分。
   (1)対空砲23基の艦内座標表(艦種別)＝ aa_update が毎フレーム参照するので常駐に置く。
   (2)ship_aag_pos ＝ 上表の読み出し。
   (3)ship_render ＝ 薄いラッパ。引数を g_shipargs に退避し、重い描画本体(banked/ship_render.c)を bcall。
   艦の描画コード本体は冷たい(ステージ開始時のみ)ので SHIP_RENDER_BANK へ追い出し常駐を空ける。 */
#include "ship.h"
#include "bank.h"   /* bcall_to */

/* 対空砲23基(艦種別配置 bb=0/cv=1/hd=2/nl=3)。前14=大径, 後9=小径。
   banked/ship_render.c の draw_aag もこの表(extern)を参照する。 */
const u8  aag_x_bb[SHIP_NAAG] = { 108,148,88,168,88,168,88,168,88,168,88,168,88,168,110,146,110,146,110,146,118,138,128 };
const u16 aag_y_bb[SHIP_NAAG] = { 196,196,150,150,178,178,206,206,240,240,280,280,308,308,232,232,282,282,300,300,34,34,452 };
const u8  aag_x_cv[SHIP_NAAG] = { 74,182,74,182,74,182,74,182,74,182,74,182,74,182,74,182,144,144,128,100,156,128,172 };
const u16 aag_y_cv[SHIP_NAAG] = { 62,62,106,106,150,150,194,194,238,238,282,282,326,326,370,370,140,220,22,442,442,448,405 };
const u8  aag_x_hd[SHIP_NAAG] = { 100,156,100,156,100,156,100,156,100,156,100,156,100,156,128,118,138,118,138,128,118,138,128 };
const u16 aag_y_hd[SHIP_NAAG] = { 145,145,180,180,215,215,250,250,285,285,315,315,345,345,195,235,235,270,270,330,388,388,452 };
const u8  aag_x_nl[SHIP_NAAG] = { 62,90,62,90,62,90,76,166,194,166,194,166,194,180,76,76,62,90,180,180,166,194,180 };
const u16 aag_y_nl[SHIP_NAAG] = { 140,140,220,220,300,300,180,140,140,220,220,300,300,180,110,260,340,340,110,260,340,340,380 };

ShipArgs g_shipargs;
u8 g_card_ram[1536];   /* 開始カード艦画像(常駐RAM。scene が data_read→draw_card_banked が拡大) */

/* 開始カード画像の2倍拡大を冷たいバンクで実行(常駐節約)。呼ぶ前に g_card_ram を data_read で満たす。 */
void draw_card_banked(void) {
    g_shipargs.mode = 1;
    bcall_to(SHIP_RENDER_BANK);
}
/* 冷たい終盤画面をバンクで実行(常駐節約)。data_read は内部で行わない=窓を差し替えない。 */
void play_death_banked(u16 cam) {
    g_shipargs.mode = 2; g_shipargs.cam = cam;
    bcall_to(SHIP_RENDER_BANK);
}
u8 game_over_banked(void) {
    g_shipargs.mode = 3;
    bcall_to(SHIP_RENDER_BANK);
    return g_shipargs.ret;
}

/* 艦を バッファB へ描画(常駐ラッパ)。引数を退避して重い実体を SHIP_RENDER_BANK で実行。
   ★カード表示中(BGM停止)に一度だけ呼ばれる冷たい経路なので bcall のバンク差替コストは無視できる。 */
void ship_render(u8 kind, u8 hull, u8 bow_cnt, u16 bow_yb, u8 aag_tbl,
                 const u8 *aagp, const u8 *ops, const u8 *ops2) {
    g_shipargs.kind = kind; g_shipargs.hull = hull; g_shipargs.bow_cnt = bow_cnt;
    g_shipargs.bow_yb = bow_yb; g_shipargs.aag_tbl = aag_tbl;
    g_shipargs.aagp = aagp; g_shipargs.ops = ops; g_shipargs.ops2 = ops2;
    g_shipargs.mode = 0;   /* 艦描画モード */
    bcall_to(SHIP_RENDER_BANK);
}

/* 対空砲 i(0..22)の艦内座標を返す(発砲システム=scene_stage が毎フレーム使う)。tbl=艦種(bb0/cv1/hd2/nl3)。 */
void ship_aag_pos(u8 tbl, u8 i, s16 *px, u16 *py) {
    const u8 *ax; const u16 *ay;
    switch (tbl) {
        case 1:  ax = aag_x_cv; ay = aag_y_cv; break;
        case 2:  ax = aag_x_hd; ay = aag_y_hd; break;
        case 3:  ax = aag_x_nl; ay = aag_y_nl; break;
        default: ax = aag_x_bb; ay = aag_y_bb; break;
    }
    *px = (s16)ax[i]; *py = ay[i];
}
