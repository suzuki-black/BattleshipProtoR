/* ship.h — 戦艦の事前描画レンダラ(旧版 BattleshipProto の艦システムを移植)。
   上面視の艦をオフスクリーン・バッファB(=SC_SHIPBUF_Y)へ一度だけ描く。
   旧版の8種op(GROUND/MAINGUN/DOME/DISK/DECKBOX/AAGUN/LMMV/BARREL)を run_ship_ops で解釈し、
   船体(paint_hull)・波切り艦首(draw_bow)・対空砲群(draw_aag)はコード側で描く。
   艦OPSデータはバンク→RAM に読んでから渡す(常駐文脈から呼ぶこと)。 */
#ifndef SHIP_H
#define SHIP_H

#include "types.h"

/* 旧版 op 定数(gen_assets.mjs / 艦OPSデータと一致必須)。 */
#define SOP_END     0
#define SOP_GROUND  1   /* ground(x,y,rr): 対空砲の影(暗disk, +4+6 ずらし) */
#define SOP_MAINGUN 2   /* mainGun(x,y,rr): 主砲塔ベース＋ドーム */
#define SOP_DOME    3   /* dome(x,y,rr): 陰影付きドーム(5枚disk) */
#define SOP_DISK    4   /* disk(x,y,rr,color): 塗り円 */
#define SOP_DECKBOX 5   /* deckBox(x,y,w,h,fill): 3D金属ボックス(metalNoise) */
#define SOP_AAGUN   6   /* aaGun(x,y,rr): 対空砲マウント＋ドーム */
#define SOP_LMMV    7   /* lmmv(x, B+y, w, h, color): 矩形塗り(Yは絶対=B+y) */
#define SOP_BARREL  8   /* barrel(x,y,L,w): 円筒陰影の砲身 */

/* hull プロファイル(paint_hull で使う): 0=ビスマルク / 3=アイオワ(旧版 g_hull と一致)。 */
#define HULL_BISMARCK 0
#define HULL_IOWA     3

/* ★艦の重い描画は「冷たいバンク」へ(常駐圧迫回避)。ship_render は常駐の薄いラッパで、
   引数を g_shipargs に退避して bcall。実体(ship_render_impl＋描画ヘルパ群)は banked/ship_render.c。
   ship_aag_pos＋対空砲座標表は毎フレーム参照(aa_update)なので常駐(ship_aag.c)に残す。 */
#define SHIP_RENDER_BANK 16   /* banked/ship_render.ihx を置くROMバンク */

#define SHIP_NAAG 23   /* 対空砲マウント数 */

/* ship_render の引数退避(常駐→バンク)。banked_entry がこれを読んで描画する。 */
typedef struct {
    u8 kind, hull, bow_cnt;
    u16 bow_yb;
    u8 aag_tbl;
    const u8 *aagp, *ops, *ops2;
    u8 mode;   /* 0=艦描画(ship_render) / 1=開始カード艦画像の2倍拡大(draw_card) */
} ShipArgs;
extern ShipArgs g_shipargs;

/* 開始カードの艦画像(64x48, bank4)を data_read で入れる常駐RAM。バンク側 draw_card が2倍拡大して
   page0へ展開(重い拡大ループを常駐から追い出す)。読み込みは常駐で済ませ、バンク内では窓を差替えない。 */
extern u8 g_card_ram[1536];
void draw_card_banked(void);   /* g_card_ram を2倍拡大して page0 へ(bcall)。事前に data_read 済のこと */

/* 対空砲23基の艦内座標表(常駐 ship_aag.c で定義)。バンク側 draw_aag と常駐 ship_aag_pos が参照。 */
extern const u8  aag_x_bb[SHIP_NAAG], aag_x_cv[SHIP_NAAG], aag_x_hd[SHIP_NAAG], aag_x_nl[SHIP_NAAG];
extern const u16 aag_y_bb[SHIP_NAAG], aag_y_cv[SHIP_NAAG], aag_y_hd[SHIP_NAAG], aag_y_nl[SHIP_NAAG];

/* 艦を バッファB へ描画(kind=0単艦/1双子/2空母)。常駐ラッパ→bcall→banked ship_render_impl。
   ops/ops2 = data_read でRAMへ読んだ艦OPS(7B/レコード, op=0終端)。aagp=[gb,gs,ab,as]。 */
void ship_render(u8 kind, u8 hull, u8 bow_cnt, u16 bow_yb, u8 aag_tbl, const u8 *aagp, const u8 *ops, const u8 *ops2);

/* 対空砲 i(0..22)の艦内座標(艦種tbl)。発砲システム(scene_stage)が毎フレーム使う=常駐。 */
void ship_aag_pos(u8 tbl, u8 i, s16 *px, u16 *py);

#endif /* SHIP_H */
