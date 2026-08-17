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
u8 g_miss;

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
                    if (g_pinv == 0 && !g_invinc) {/* 被弾直後の無敵中/設定無敵 は無傷 */
                        g_playerhit++;
                        ent_spawn_explosion(p->x, p->y);
                        if (g_php > 1) { g_php--; g_pinv = 90; }  /* 耐久残=生存(1.5秒無敵点滅) */
                        else { g_php = 0; g_miss = 1; }           /* 耐久尽き=撃墜。残機/リスタートはシーンが処理 */
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

/* 敵戦闘機と敵弾だけを消す(自機/自機弾/砲台は残す)。空戦→戦艦の受け渡しで空襲を退かせる用。 */
void ent_clear_enemies(void) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++) {
        Entity *e = &pool[i];
        if (!e->active) continue;
        if (e->type == ET_FIGHTER || (e->type == ET_BULLET && e->team == TEAM_ENEMY)) e->active = 0;
    }
}

/* 敵戦闘機だけを消す(戦艦接近中に紛れ込む戦闘機の毎フレーム掃除用。砲台弾は残す)。 */
void ent_clear_fighters(void) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++)
        if (pool[i].active && pool[i].type == ET_FIGHTER) pool[i].active = 0;
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
   ★V9938 mode2 は1走査線8枚まで(9枚目以降は欠落)。弾幕対策:
     - HUD(slot 0..g_spr_base-1) と 自機 は固定の最優先スロットで絶対に欠けさせない。
     - 残り(弾/敵/砲塔/エフェクト)は毎フレーム割当開始を回転(rot)させ、9枚以上の走査線での
       欠落を「常に同じ弾が消える」でなく「フレーム毎に入れ替わるちらつき」に分散する。
     - 総数は 32 枚で頭打ち(それ以上は描かない)。 */
static u8 draw1(u8 slot, const Entity *e) {   /* 1体を slot へ描画し、次 slot を返す */
    vdp_sprite_color(slot, e->color);
    vdp_sprite_pos(slot, (u8)e->x, (u8)e->y, e->pat);
    return (u8)(slot + 1);
}
static u8 visible(const Entity *e, u8 skip_player) {
    if (!e->active || e->hidden) return 0;
    if (skip_player ? (e->type == ET_PLAYER) : (e->type != ET_PLAYER)) return 0;
    return (e->y > -16 && e->y < 212);
}
void ent_draw_all(void) {
    static u8 rot;
    u8 i, j, n = 0, slot = g_spr_base;
    u8 vis[ENT_MAX];   /* 描画対象(自機以外)の pool 添字 */
    /* 自機を固定最優先スロット(g_spr_base)へ=絶対に欠けさせない */
    for (i = 0; i < ENT_MAX; i++)
        if (visible(&pool[i], 0)) { slot = draw1(slot, &pool[i]); break; }
    /* 描画対象を収集 */
    for (i = 0; i < ENT_MAX; i++)
        if (visible(&pool[i], 1)) vis[n++] = i;
    /* 収集集合内で開始位置を毎フレーム回転させて割当。クラスタ位置に依らず均等に回るので、
       同一走査線9枚以上の欠落が「フレーム毎に入れ替わるちらつき」へ均等分散する。 */
    if (n) {
        u8 start = (u8)(rot % n);
        for (j = 0; j < n && slot < 32; j++) {
            i = (u8)(start + j); if (i >= n) i -= n;
            slot = draw1(slot, &pool[vis[i]]);
        }
    }
    vdp_sprite_hide_from(slot);
    rot++;
}
