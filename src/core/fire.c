/* fire.c — 発砲プリミティブ emit と発砲スクリプト run_fire。
   emit が弾 ET_BULLET を spawn し、run_fire が FireDesc を解釈して発砲パターンを撒く。 */
#include "fire.h"

#define BULLET_PAT 4   /* 弾スプライトのパターン番号(scene 側で投入) */

/* 16分割方向の単位速度(半径8, dir0=上, 時計回り)。実速度 = tab * spd / 8。 */
static const s8 dvx[16] = { 0,  3,  6,  7,  8,  7,  6,  3,  0, -3, -6, -7, -8, -7, -6, -3 };
static const s8 dvy[16] = { -8, -7, -6, -3,  0,  3,  6,  7,  8,  7,  6,  3,  0, -3, -6, -7 };

/* 弾種→色(0-15)。当面 kind 少数。将来 kind でパターンも分岐。 */
static const u8 kind_col[4] = { 15, 10, 9, 7 };

Entity *emit(s16 x, s16 y, u8 dir, u8 kind, u8 spd) {
    Entity *b = ent_spawn(ET_BULLET);
    if (!b) return (Entity *)0;
    b->x = x; b->y = y;
    b->vx = (s16)dvx[dir & 15] * spd / 8;
    b->vy = (s16)dvy[dir & 15] * spd / 8;
    b->color = kind_col[kind & 3];
    b->pat = BULLET_PAT;
    return b;
}

void run_fire(Entity *e) {
    const u8 *p = e->fire;
    u8 interval;
    if (!p) return;
    if (e->ftimer) { e->ftimer--; return; }

    interval = p[0];
    e->ftimer = interval;
    p++;                          /* オペ列の先頭へ */
    while (*p != FIRE_END) {
        u8 op   = *p++;
        u8 a    = *p++;
        u8 kind = *p++;
        u8 spd  = *p++;
        /* 発射原点は実体中心(スプライト16x16の中央付近) */
        s16 ox = e->x + 4, oy = e->y + 4;
        if (op == FIRE_FIXED) {
            emit(ox, oy, a, kind, spd);
        } else if (op == FIRE_RING) {
            u8 i;
            for (i = 0; i < a; i++) emit(ox, oy, (u8)((u16)i * 16 / a), kind, spd);
        }
    }
}
