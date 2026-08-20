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

/* 艦を バッファB へ描画。kind=0単艦(BB/Iowa/Hood) / 1双子(2隻) / 2空母(飛行甲板+2パス)。
   海タイル→(艦種別に船体/艦首/甲板)→OPS(→ops2は空母のみ)→対空砲23基。
   ops/ops2 = data_read でRAMへ読んだ艦OPS(7B/レコード, op=0終端)。aagp=[gb,gs,ab,as]。 */
void ship_render(u8 kind, u8 hull, u8 bow_cnt, u16 bow_yb, u8 aag_tbl, const u8 *aagp, const u8 *ops, const u8 *ops2);

#endif /* SHIP_H */
