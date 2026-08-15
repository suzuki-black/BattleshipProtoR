/* entity.c — 汎用エンティティ・プールの常駐実装。
   behavior は type 別の関数ポインタ表(generalization の要)。描画は今は LMMV 矩形で代用し、
   本番でスプライト/run_ops に差し替える(APIは据え置き)。 */
#include "entity.h"
#include "vdp.h"
#include "fire.h"
#include "sprites.h"
#include "scroll.h"     /* g_cam(砲塔の世界→画面Y変換) */
#include "gamestate.h"  /* g_score(撃破で加算) */

u8 g_spr_base;          /* エンティティ描画の開始スプライトスロット(先頭はHUDが確保) */

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

/* 射手: 発砲スクリプトを進める(発射は run_fire→emit)。位置は固定。 */
static void bh_shooter(Entity *e) {
    run_fire(e);
}

/* 砲塔: 艦上の世界座標(ax,ay)から画面座標へ。縦=ay-cam(艦と一緒にスクロール)、
   横=ax+weaveX(蛇行の横揺れに追従)。画面内に居る時だけ発砲(動く的)。 */
s16 g_meander;
static void bh_turret(Entity *e) {
    e->x = e->ax + g_meander;
    e->y = e->ay - (s16)g_cam;
    if (e->y > -16 && e->y < 212) run_fire(e);   /* 画面内のみ発砲 */
    else e->ftimer = 1;                          /* 画面外はチャージ据置(即撃ちさせない) */
}

/* 敵戦闘機(空戦): 下方向へ進み、左右に浅く蛇行しつつ画面下で消滅。fireを持てば発砲も。 */
static void bh_fighter(Entity *e) {
    e->y += e->vy;
    e->x += e->vx;
    if (e->x < 0 || e->x > (s16)(SCR_W - e->w)) e->vx = -e->vx;   /* 端で横反転=浅い蛇行 */
    if (e->fire) run_fire(e);
    if (e->y > SCR_H + 8) e->active = 0;                          /* 下へ抜けたら消滅 */
}

/* 撃破エフェクト: 寿命を ftimer で数え、色を変えながら消滅。 */
static const u8 exp_col[6] = { 15, 11, 10, 8, 6, 4 };
static void bh_explosion(Entity *e) {
    if (e->ftimer == 0) { e->active = 0; return; }
    e->ftimer--;
    e->color = exp_col[(e->ftimer >> 1) % 6];
}

extern void bh_player(Entity *e);   /* player.c(入力/発砲を持つのでゲーム側モジュールへ) */

typedef void (*Behavior)(Entity *);
static const Behavior behaviors[ET_COUNT] = {
    0,            /* ET_NONE      */
    bh_bouncer,   /* ET_BOUNCER   */
    bh_bullet,    /* ET_BULLET    */
    bh_shooter,   /* ET_SHOOTER   */
    bh_fighter,   /* ET_FIGHTER   */
    bh_player,    /* ET_PLAYER    */
    bh_turret,    /* ET_TURRET(蛇行追従＋発砲。破壊可能) */
    bh_explosion, /* ET_EXPLOSION */
};

u8 ent_count(u8 type) {
    u8 i, n = 0;
    for (i = 0; i < ENT_MAX; i++) if (pool[i].active && pool[i].type == type) n++;
    return n;
}

void ent_spawn_explosion(s16 x, s16 y) {
    Entity *e = ent_spawn(ET_EXPLOSION);
    if (e) { e->x = x; e->y = y; e->pat = SPR_BLOCK; e->color = 15; e->ftimer = 14; }
}

/* ---- 当たり判定 ---- */
u8 g_kills;
u8 g_gun_kills;
u8 g_playerhit;
u8 g_pinv;

/* 16x16 実体の AABB 重なり(やや甘めのマージン14)。 */
static u8 overlap(const Entity *a, const Entity *b) {
    s16 dx = a->x - b->x, dy = a->y - b->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx < 14 && dy < 14);
}

void ent_resolve_collisions(void) {
    u8 i, j;
    /* 自機弾(TEAM_PLAYER) × 敵戦闘機/砲台 → 弾消滅、戦闘機は即撃破、砲台は hp 減算 */
    for (i = 0; i < ENT_MAX; i++) {
        Entity *b = &pool[i];
        if (!b->active || b->type != ET_BULLET || b->team != TEAM_PLAYER) continue;
        for (j = 0; j < ENT_MAX; j++) {
            Entity *t = &pool[j];
            if (!t->active) continue;
            if (t->type == ET_FIGHTER && overlap(b, t)) {
                b->active = 0; t->active = 0; g_kills++; g_score += 10;
                ent_spawn_explosion(t->x, t->y);
                break;
            }
            if (t->type == ET_TURRET && overlap(b, t)) {
                b->active = 0;
                if (--t->hp == 0) { t->active = 0; g_gun_kills++; g_score += 50; ent_spawn_explosion(t->x, t->y); }
                break;
            }
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
                if (overlap(e, p)) {
                    e->active = 0;                 /* 敵/敵弾は消す(すり抜け防止) */
                    if (g_pinv == 0) {             /* 無敵中は残機を減らさない */
                        g_playerhit++;
                        if (g_lives) g_lives--;
                        g_pinv = 90;               /* 約1.5秒の無敵(点滅) */
                        ent_spawn_explosion(p->x, p->y);
                    }
                }
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
            e->x = 0; e->y = 0; e->vx = 0; e->vy = 0; e->ax = 0; e->ay = 0;
            e->w = 16; e->h = 16; e->color = 15; e->pat = 0;
            e->hidden = 0; e->hp = 1; e->team = TEAM_ENEMY;
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
   ※画面外(y が縦範囲外)は描画しない: スプライトYは u8 なので世界アンカーの砲塔などが
     画面上方(負のy)にある間に (u8)y へ折り返して海上に幽霊表示されるのを防ぐ。
   ※同一走査線に8枚以上でスプライト欠け(V9938 mode2)。上端HUD＋敵密集時はレイアウト注意。 */
void ent_draw_all(void) {
    u8 i, slot = g_spr_base;   /* 先頭スロットは HUD が確保(g_spr_base) */
    Entity *e;
    for (i = 0; i < ENT_MAX; i++) {
        e = &pool[i];
        if (e->active && !e->hidden && e->y > -16 && e->y < 212) {
            vdp_sprite_color(slot, e->color);
            vdp_sprite_pos(slot, (u8)e->x, (u8)e->y, e->pat);
            slot++;
        }
    }
    vdp_sprite_hide_from(slot);
}
