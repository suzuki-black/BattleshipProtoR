/* entity.c — 汎用エンティティ・プールの常駐実装。
   behavior は type 別の関数ポインタ表(generalization の要)。描画は今は LMMV 矩形で代用し、
   本番でスプライト/run_ops に差し替える(APIは据え置き)。 */
#include "entity.h"
#include "vdp.h"

#define SCR_W    256
#define SCR_H    212
#define BG_COLOR 4     /* 背景(海青)。消去色 */

static Entity pool[ENT_MAX];

/* ---- behavior: 種別ごとの毎フレーム更新 ---- */
static void bh_bouncer(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;
    if (e->x < 0)                       { e->x = 0;               e->vx = -e->vx; }
    else if (e->x > (s16)(SCR_W - e->w)){ e->x = SCR_W - e->w;    e->vx = -e->vx; }
    if (e->y < 0)                       { e->y = 0;               e->vy = -e->vy; }
    else if (e->y > (s16)(SCR_H - e->h)){ e->y = SCR_H - e->h;    e->vy = -e->vy; }
}

typedef void (*Behavior)(Entity *);
static const Behavior behaviors[ET_COUNT] = {
    0,           /* ET_NONE    */
    bh_bouncer,  /* ET_BOUNCER */
};

void ent_reset(void) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++) pool[i].active = 0;
}

Entity *ent_spawn(u8 type) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++) {
        if (!pool[i].active) {
            Entity *e = &pool[i];
            e->active = 1; e->type = type;
            e->x = 0; e->y = 0; e->vx = 0; e->vy = 0;
            e->w = 8; e->h = 8; e->color = 15;
            e->drawn = 0;
            return e;
        }
    }
    return (Entity *)0;
}

void ent_update_all(void) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++) {
        Entity *e = &pool[i];
        if (e->active && behaviors[e->type]) behaviors[e->type](e);
    }
}

/* 2パス: (1)全 active の前位置を背景で消去 → (2)現位置へ描画。
   1パスで消去+描画を混ぜると、後続実体の消去が先行実体の描画を消すため分離する。 */
void ent_draw_all(void) {
    u8 i;
    Entity *e;
    for (i = 0; i < ENT_MAX; i++) {
        e = &pool[i];
        if (e->active && e->drawn) vdp_fill((u16)e->px, (u16)e->py, e->w, e->h, BG_COLOR);
    }
    for (i = 0; i < ENT_MAX; i++) {
        e = &pool[i];
        if (e->active) {
            vdp_fill((u16)e->x, (u16)e->y, e->w, e->h, e->color);
            e->px = e->x; e->py = e->y; e->drawn = 1;
        }
    }
}
