/* entity.c — 汎用エンティティ・プールの常駐実装。
   behavior は type 別の関数ポインタ表(generalization の要)。描画は今は LMMV 矩形で代用し、
   本番でスプライト/run_ops に差し替える(APIは据え置き)。 */
#include "entity.h"
#include "vdp.h"
#include "fire.h"

#define SCR_W 256
#define SCR_H 212

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

/* 弾: 直進し画面外(±16マージン)で消滅。 */
static void bh_bullet(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;
    if (e->x < -16 || e->x > SCR_W || e->y < -16 || e->y > SCR_H) e->active = 0;
}

/* 射手: 発砲スクリプトを進める(発射は run_fire→emit)。位置は固定(将来 behavior 合成)。 */
static void bh_shooter(Entity *e) {
    run_fire(e);
}

/* 敵戦闘機(空戦): 下方向へ進み、左右に浅く蛇行しつつ画面下で消滅。fireを持てば発砲も。 */
static void bh_fighter(Entity *e) {
    e->y += e->vy;
    e->x += e->vx;
    if (e->x < 0 || e->x > (s16)(SCR_W - e->w)) e->vx = -e->vx;   /* 端で横反転=浅い蛇行 */
    if (e->fire) run_fire(e);
    if (e->y > SCR_H + 8) e->active = 0;                          /* 下へ抜けたら消滅 */
}

extern void bh_player(Entity *e);   /* player.c(入力/発砲を持つのでゲーム側モジュールへ) */

typedef void (*Behavior)(Entity *);
static const Behavior behaviors[ET_COUNT] = {
    0,           /* ET_NONE    */
    bh_bouncer,  /* ET_BOUNCER */
    bh_bullet,   /* ET_BULLET  */
    bh_shooter,  /* ET_SHOOTER */
    bh_fighter,  /* ET_FIGHTER */
    bh_player,   /* ET_PLAYER  */
};

/* ---- 当たり判定 ---- */
u8 g_kills;
u8 g_playerhit;

/* 16x16 実体の AABB 重なり(やや甘めのマージン14)。 */
static u8 overlap(const Entity *a, const Entity *b) {
    s16 dx = a->x - b->x, dy = a->y - b->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx < 14 && dy < 14);
}

void ent_resolve_collisions(void) {
    u8 i, j;
    /* 自機弾(TEAM_PLAYER) × 敵戦闘機 → 相打ちで両消滅、撃破+1 */
    for (i = 0; i < ENT_MAX; i++) {
        Entity *b = &pool[i];
        if (!b->active || b->type != ET_BULLET || b->team != TEAM_PLAYER) continue;
        for (j = 0; j < ENT_MAX; j++) {
            Entity *f = &pool[j];
            if (!f->active || f->type != ET_FIGHTER) continue;
            if (overlap(b, f)) { b->active = 0; f->active = 0; g_kills++; break; }
        }
    }
    /* 敵弾(TEAM_ENEMY)/敵戦闘機 × 自機 → 敵を消し被弾+1 */
    for (i = 0; i < ENT_MAX; i++) {
        Entity *p = &pool[i];
        if (!p->active || p->type != ET_PLAYER) continue;
        for (j = 0; j < ENT_MAX; j++) {
            Entity *e = &pool[j];
            if (!e->active) continue;
            if ((e->type == ET_BULLET && e->team == TEAM_ENEMY) || e->type == ET_FIGHTER) {
                if (overlap(e, p)) { e->active = 0; g_playerhit++; }
            }
        }
    }
}

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
            e->w = 16; e->h = 16; e->color = 15; e->pat = 0;
            e->team = TEAM_ENEMY;
            e->fire = (const u8 *)0; e->ftimer = 0;
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

/* スプライト描画: active な実体を先頭スロットから詰めて属性/色を書き、
   残りは停止マーカで隠す。ハードウェア合成なので消去は不要。
   ※同一走査線に5枚以上でスプライト欠けが起きる(mode2)点は本番でレイアウトに注意。 */
void ent_draw_all(void) {
    u8 i, slot = 0;
    Entity *e;
    for (i = 0; i < ENT_MAX; i++) {
        e = &pool[i];
        if (e->active) {
            vdp_sprite_color(slot, e->color);
            vdp_sprite_pos(slot, (u8)e->x, (u8)e->y, e->pat);
            slot++;
        }
    }
    vdp_sprite_hide_from(slot);
}
