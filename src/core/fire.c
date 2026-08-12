/* fire.c — 発砲プリミティブ emit と発砲スクリプト run_fire。
   emit が弾 ET_BULLET を spawn し、run_fire が FireDesc を解釈して発砲パターンを撒く。
   狙い弾は「不正確さの円錐/扇」で散らし公平化(fire.h 参照)。 */
#include "fire.h"
#include "player.h"   /* g_player_x/y(自機狙い) */

#define BULLET_PAT 4   /* 弾スプライトのパターン番号(scene 側で投入) */

/* 32分割方向の単位速度(半径8, dir0=上, 時計回り 11.25°刻み)。実速度 = tab * spd / 8。 */
static const s8 dvx[32] = {
    0,  2,  3,  4,  6,  7,  7,  8,  8,  8,  7,  7,  6,  4,  3,  2,
    0, -2, -3, -4, -6, -7, -7, -8, -8, -8, -7, -7, -6, -4, -3, -2
};
static const s8 dvy[32] = {
   -8, -8, -7, -7, -6, -4, -3, -2,  0,  2,  3,  4,  6,  7,  7,  8,
    8,  8,  7,  7,  6,  4,  3,  2,  0, -2, -3, -4, -6, -7, -7, -8
};

/* 弾種→色(0-15)。 */
static const u8 kind_col[4] = { 15, 10, 9, 7 };

/* 発砲用の簡易PRNG(LCG)。散らしに使う。 */
static u16 fr = 0x2B7D;
static u8 frand(void) { fr = fr * 25173 + 13849; return (u8)(fr >> 8); }

Entity *emit(s16 x, s16 y, u8 dir, u8 kind, u8 spd) {
    Entity *b = ent_spawn(ET_BULLET);
    if (!b) return (Entity *)0;
    dir &= 31;
    b->x = x; b->y = y;
    b->vx = (s16)dvx[dir] * spd / 8;
    b->vy = (s16)dvy[dir] * spd / 8;
    b->color = kind_col[kind & 3];
    b->pat = BULLET_PAT;
    return b;
}

u8 aim_dir(s16 ex, s16 ey, s16 px, s16 py) {
    s16 dx = px - ex, dy = py - ey;
    s16 best = -32767;
    u8 bi = 16, i;
    for (i = 0; i < 32; i++) {
        s16 s = dx * (s16)dvx[i] + dy * (s16)dvy[i];   /* 内積が最大=最も近い方向 */
        if (s > best) { best = s; bi = i; }
    }
    return bi;
}

void run_fire(Entity *e) {
    const u8 *p = e->fire;
    u8 interval;
    if (!p) return;
    if (e->ftimer) { e->ftimer--; return; }

    interval = p[0];
    e->ftimer = interval;
    p++;
    while (*p != FIRE_END) {
        u8 op   = *p++;
        u8 a    = *p++;
        u8 kind = *p++;
        u8 spd  = *p++;
        s16 ox = e->x + 4, oy = e->y + 4;
        if (op == FIRE_FIXED) {
            emit(ox, oy, a, kind, spd);
        } else if (op == FIRE_RING) {
            u8 i;
            for (i = 0; i < a; i++) emit(ox, oy, (u8)((u16)i * 32 / a), kind, spd);
        } else if (op == FIRE_AIMED) {
            /* 狙い方向 ± 一様乱数 a ステップ(不正確さの円錐)。a=0で厳密狙い。 */
            u8 base = aim_dir(ox, oy, g_player_x, g_player_y);
            s8 off = (a == 0) ? 0 : (s8)(frand() % (u8)(2 * a + 1)) - (s8)a;
            emit(ox, oy, (u8)((base + off) & 31), kind, spd);
        } else if (op == FIRE_AIMFAN) {
            /* 自機中心の a-way 散弾(2ステップ間隔で扇)＋扇全体を乱数で微回転。
               偶数 a なら自機の直線上に弾が来ない(隙間)=狙いすぎ回避。 */
            u8 base = aim_dir(ox, oy, g_player_x, g_player_y);
            s8 jit = (s8)(frand() & 1);
            u8 i;
            for (i = 0; i < a; i++) {
                s8 d = (s8)(2 * i) - (s8)(a - 1) + jit;
                emit(ox, oy, (u8)((base + d) & 31), kind, spd);
            }
        }
    }
}
