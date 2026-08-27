/* entity.c — 汎用エンティティ・プールの常駐実装。
   behavior は type 別の関数ポインタ表(generalization の要)。描画は今は LMMV 矩形で代用し、
   本番でスプライト/run_ops に差し替える(APIは据え置き)。 */
#include "entity.h"
#include "vdp.h"
#include "fire.h"
#include "sprites.h"
#include "scroll.h"     /* g_cam(砲塔の世界→画面Y変換) */
#include "gamestate.h"  /* g_score(撃破で加算) */
#include "sound.h"      /* sfx(被弾音 SFX_PHIT) */
#include "player.h"     /* g_player_x/y(艦載機の自機追尾) */

u8 g_spr_base;          /* エンティティ描画の開始スプライトスロット(先頭はHUDが確保) */

#define SCR_W 256
#define SCR_H 212

static Entity pool[ENT_MAX];

/* 各スプライトスロットに最後に書いた単色(0xFF=coltab/未確定=強制書換)。色表の重複16B書込みを省く。 */
static u8 slot_col[32];

/* ---- behavior: 種別ごとの毎フレーム更新 ---- */
static void bh_bouncer(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;
    if (e->x < 0)                       { e->x = 0;               e->vx = -e->vx; }
    else if (e->x > (s16)(SCR_W - e->w)){ e->x = SCR_W - e->w;    e->vx = -e->vx; }
    if (e->y < 0)                       { e->y = 0;               e->vy = -e->vy; }
    else if (e->y > (s16)(SCR_H - e->h)){ e->y = SCR_H - e->h;    e->vy = -e->vy; }
}

/* 弾: 直進し画面外(±16マージン)で消滅。
   ★スクロールとは完全に独立=毎フレーム画面座標を vx/vy だけ進める(表示上の見かけ速度が一定)。
     蛇行の上り/下り・左右に一切影響されない(甲板追従の補正は入れない)。 */
static void bh_bullet(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;
    if (e->x < -16 || e->x > SCR_W || e->y < -16 || e->y > SCR_H) e->active = 0;
}

/* 8方向単位ベクトル(0=上,1=右上,2=右,3=右下,4=下,5=左下,6=左,7=左上)。 */
static u8 dir8(s16 dx, s16 dy);        /* 前方宣言(bh_turret が使う。定義は下) */
static u8 step_dir(u8 cur, u8 tgt);
static const s8 dirdx8[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const s8 dirdy8[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

/* 対空砲の時限信管弾: 直進しつつ ftimer(信管)を数え、0で空中炸裂。自機帯(y>=185)より下では不発。
   炸裂時は下向き3破片(右下/下/左下)を速度3で撒き、その場に爆発演出。旧版 airburst の移植。 */
static void bh_aaburst(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;                /* ★信管弾もスクロール非依存=画面座標を一定速度で進める */
    if (e->x < 0 || e->x > 255 || e->y < 16 || e->y > 220) { e->active = 0; return; }
    if (e->ftimer == 0) {                       /* 信管作動 */
        if (e->y < 185) {                       /* 自機帯より上でのみ炸裂(下から湧かない) */
            static const u8 shdir[3] = { 3, 4, 5 };   /* 右下/下/左下の扇 */
            u8 k;
            for (k = 0; k < 3; k++) {
                Entity *f;
                if (ent_enemy_bullet_full()) break;   /* ★弾幕上限リミッタ(破片も一元管理) */
                f = ent_spawn(ET_BULLET);
                if (f) {
                    f->team = TEAM_ENEMY;
                    f->x = e->x; f->y = (s16)(e->y + k * 3);   /* Yを少しずらし同一走査線回避 */
                    f->vx = (s16)(dirdx8[shdir[k]] * 3); f->vy = (s16)(dirdy8[shdir[k]] * 3);
                    f->color = 12; f->pat = SPR_BULLET;   /* 破片=橙(敵弾統一色) */
                }
            }
            ent_spawn_explosion(e->x, e->y);    /* 炸裂の見た目 */
            sfx(2, SFX_HIT);                     /* 炸裂音(小) */
        }
        e->active = 0;
        return;
    }
    e->ftimer--;
}

/* 射手: 発砲スクリプトを進める(発射は run_fire→emit)。位置は固定。 */
static void bh_shooter(Entity *e) {
    run_fire(e);
}

/* 砲塔: 艦上の世界座標(ax,ay)から画面座標へ。縦=ay-cam(艦と一緒にスクロール)、
   横=ax+weaveX(蛇行の横揺れに追従)。画面内に居る時だけ発砲(動く的)。
   ★可動砲身: 自機を狙って段階回転(vx=向き0-7, vy=旋回冷却)。命中フラッシュ(h=残フレーム, 白coltab)。 */
s16 g_meander;
#define BARREL_OFF 5   /* 砲身スプライトを狙い方向へ突き出す量(ドームから砲身が出る) */
static void bh_turret(Entity *e) {
    s16 dx, dy; u8 d;
    if (e->hp == 0) { e->hidden = 1; return; }         /* 撃破済み: 砲身消失・不動(炎上はscene側BGで) */
    dx = e->ax + g_meander;                            /* ドーム位置(照準/発砲/当たりの基準) */
    dy = e->ay - (s16)g_cam;
    e->x = dx; e->y = dy;
    if (e->h) { e->h--; e->coltab = barrel_flash; }   /* 命中で白フラッシュ */
    else        e->coltab = barrel_col;               /* 通常=金属シェード */
    if (dy > -16 && dy < 212) {
        u8 tgt = dir8((s16)g_player_x - dx, (s16)g_player_y - dy);
        if (e->vy) e->vy--;                            /* 旋回冷却 */
        else { e->vx = (s16)step_dir((u8)e->vx, tgt); e->vy = 5; }   /* 5fごとに1段 */
        run_fire(e);                                   /* 発砲(ドーム位置から, 画面内のみ) */
    } else e->ftimer = 1;                              /* 画面外はチャージ据置 */
    d = (u8)e->vx & 7;
    e->pat = (u8)(SPR_BARREL0 + d * 4);                /* 向きに応じた砲身バー(上下=縦, 左右=横, 斜め) */
    e->x = dx + (s16)dirdx8[d] * BARREL_OFF;           /* ★砲身をドームから狙い方向へ突き出す=切れて見えない */
    e->y = dy + (s16)dirdy8[d] * BARREL_OFF;
}

/* 敵戦闘機(空戦): 下方向へ進み画面下で消滅。fireを持てば発砲も。
   ★所属国別の飛び方(archetype=e->ax, 位相=e->ay):
     0=独(急降下): 徐々に加速する直進降下(一撃離脱)。
     1=英(旋回機): 16fごとにvx反転=横蛇行しながら降下。
     2=米(直進/数): 一定速の直進(端で反転)。scene側で出現間隔を詰めて数で押す。 */
static void bh_fighter(Entity *e) {
    e->y += e->vy;
    e->x += e->vx;
    if ((u8)e->ax == 0) {                                 /* 独: 急降下(加速) */
        if ((++e->ay & 31) == 0 && e->vy < 6) e->vy++;
    } else if ((u8)e->ax == 1) {                          /* 英: 横蛇行 */
        if ((++e->ay & 15) == 0) e->vx = (s16)(-e->vx);
    }
    if (e->x < 0 || e->x > (s16)(SCR_W - e->w)) e->vx = (s16)(-e->vx);  /* 端で横反転 */
    if (e->fire) run_fire(e);
    if (e->y > SCR_H + 8) e->active = 0;                  /* 下へ抜けたら消滅 */
}

/* (dx,dy)に最も近い8方向index(0=上,時計回り)。旧版 dir8 移植。 */
static u8 dir8(s16 dx, s16 dy) {
    s16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax > ay * 2) return dx > 0 ? 2 : 6;    /* 右/左 */
    if (ay > ax * 2) return dy > 0 ? 4 : 0;    /* 下/上 */
    if (dx > 0) return dy > 0 ? 3 : 1;         /* 右下/右上 */
    return dy > 0 ? 5 : 7;                      /* 左下/左上 */
}
/* cur から tgt へ8方向を短い方に1歩(旧版 step_dir)。 */
static u8 step_dir(u8 cur, u8 tgt) {
    u8 d = (u8)((tgt - cur) & 7);
    if (d == 0) return cur;
    return (u8)((d <= 4) ? (cur + 1) & 7 : (cur + 7) & 7);
}

/* 艦載機(F6F/F4U): ホバー(展開)→8方向で自機を大回り追尾。20fに1歩だけ向き直り speed2 で直進。
   旧版の空母/アイオワ機を移植。HP1(弾/体当りで消滅)。画面外(±18マージン)で消滅。 */
static void bh_pursuer(Entity *e) {
    if (e->ftimer) {                           /* ホバー/展開: 動かず常時自機を向く */
        e->ftimer--;
        e->ax = (s16)dir8((s16)g_player_x - e->x, (s16)g_player_y - e->y);
    } else {                                   /* 追尾: 20fに1歩向き直り(大回り), speed2直進 */
        if (e->ay) e->ay--;
        else { e->ax = (s16)step_dir((u8)e->ax, dir8((s16)g_player_x - e->x, (s16)g_player_y - e->y)); e->ay = 20; }
        e->x += (s16)dirdx8[e->ax & 7] * 2;
        e->y += (s16)dirdy8[e->ax & 7] * 2;
    }
    if (e->x < -18 || e->x > 274 || e->y < -18 || e->y > 226) e->active = 0;
}

/* 潜水艦ミサイル(フッド固有): 舷側から発進(ph1)→浮上し弱誘導(ph2)→点火点滅→8方向炸裂(ph3)。
   ph>=2(浮上後)は体自体が危険。撃墜不可(ライフサイクル完遂)。旧版4相を3相にコンパクト移植。 */
static void bh_smissile(Entity *e) {
    u8 ph = (u8)e->ax;
    if (ph == 1) {                              /* 発進: 舷側から外へ、水中(暗青の小弾) */
        e->x += (s16)e->ay * 2;
        if (e->x < 6) e->x = 6; else if (e->x > 250) e->x = 250;
        e->pat = SPR_BULLET; e->color = 7;
        if (e->ftimer) e->ftimer--; else { e->ax = 2; e->ftimer = 40; }
    } else if (ph == 2) {                        /* 浮上→弱誘導(自機へ1px。舷側の外に留まる) */
        s16 shipC = 128 + g_meander, wantx = (s16)g_player_x;
        if (e->ay < 0) { if (wantx > shipC - 52) wantx = shipC - 52; }
        else           { if (wantx < shipC + 52) wantx = shipC + 52; }
        if (e->x < wantx) e->x++; else if (e->x > wantx) e->x--;
        if (e->y < (s16)g_player_y) e->y++; else if (e->y > (s16)g_player_y) e->y--;
        e->pat = SPR_EBSHELL; e->color = 11;    /* 浮上=赤い誘導弾頭(艦の白/AA橙と区別) */
        if (e->ftimer) e->ftimer--; else { e->ax = 3; e->ftimer = 16; }
    } else {                                     /* 点火(点滅)→8方向炸裂 */
        e->color = 12; e->hidden = (e->ftimer & 2) ? 1 : 0;
        if (e->ftimer) { e->ftimer--; return; }
        e->hidden = 0;
        { u8 k; for (k = 0; k < 8; k++) {
            Entity *b = ent_spawn(ET_BULLET);
            if (b) { b->team = TEAM_ENEMY; b->x = e->x; b->y = e->y;
                     b->vx = (s16)dirdx8[k] * 2; b->vy = (s16)dirdy8[k] * 2;
                     b->color = 12; b->pat = SPR_BULLET; } } }
        ent_spawn_explosion(e->x, e->y); sfx(2, SFX_HIT);
        e->active = 0;
    }
}

/* 合体弾の予告(双子艦): 左右2発(cx±off)が中心へ収束(off→0)。合体で自機狙いの高速大弾を発射。
   ay=中心x, y=中心y, x=半間隔off(収束), ftimer=残収束フレーム。実体は hidden=1(描画は ent_draw_all の合体パス)。 */
#define CB_GAP      52    /* 収束開始の半間隔(左右の艦=中心±52) */
#define CB_CONVERGE 24    /* 収束フレーム数 */
static void bh_combo(Entity *e) {
    if (e->ftimer) {                                   /* 収束中: off を 0 へ */
        e->ftimer--;
        e->x = (s16)((u16)e->ftimer * CB_GAP / CB_CONVERGE);
        return;
    }
    { s16 cx = e->ay, cy = e->y;                        /* 合体→自機狙いの高速大弾 */
      u8 dir = aim_dir(cx, cy, (s16)g_player_x, (s16)g_player_y);
      Entity *b = emit(cx, cy, dir, 0, 9);             /* kind0=橙, spd9=速い */
      if (b) b->pat = SPR_EBSHELL;                     /* 太い合体弾 */
      ent_spawn_explosion(cx, cy); sfx(2, SFX_HIT); }
    e->active = 0;
}

/* 撃破エフェクト: 4コマの火球アニメ(核→炸裂→大輪→残火)を進めつつ 白→橙→赤→暗 と冷めて消滅。
   spawn 時 ftimer=16。elapsed=16-ftimer を >>2 でコマ(0..3)に。 */
static const u8 exp_pat[4]  = { SPR_EXP0, SPR_EXP1, SPR_EXP2, SPR_EXP3 };
static const u8 exp_fcol[4] = { 15, 12, 11, 7 };            /* 白→橙→赤→暗 */
static void bh_explosion(Entity *e) {
    u8 f;
    if (e->ftimer == 0) { e->active = 0; return; }
    e->ftimer--;
    f = (u8)((16 - e->ftimer) >> 2); if (f > 3) f = 3;
    e->pat = exp_pat[f]; e->color = exp_fcol[f];
}

/* 火花(被弾ヒット/マズルフラッシュ): SPR_FLASH を短時間 白→淡→橙 と明滅して消滅。 */
static const u8 spark_col[4] = { 15, 14, 12, 11 };
static void bh_spark(Entity *e) {
    if (e->ftimer == 0) { e->active = 0; return; }
    e->ftimer--;
    e->color = spark_col[(e->ftimer >> 1) & 3];
}

/* 空母甲板の停泊機: 艦上世界アンカー(ax,ay)で静止し、艦と一緒にスクロール(砲塔と同じ座標系, 発砲なし)。
   自機弾で破壊可(下の当たり判定)。発艦時は scene が type を ET_PURSUER へ差し替える。 */
static void bh_parked(Entity *e) {
    e->x = e->ax + g_meander;
    e->y = e->ay - (s16)g_cam;
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
    bh_aaburst,   /* ET_AABURST(時限信管弾→下向き3破片へ炸裂) */
    bh_pursuer,   /* ET_PURSUER(艦載機の8方向追尾) */
    bh_smissile,  /* ET_SMISSILE(潜水艦ミサイル4相) */
    bh_combo,     /* ET_COMBO(双子艦の合体弾予告) */
    bh_spark,     /* ET_SPARK(火花) */
    bh_parked,    /* ET_PARKED(空母甲板の停泊機。艦上静止) */
};

u8 ent_count(u8 type) {
    u8 i, n = 0;
    for (i = 0; i < ENT_MAX; i++) if (pool[i].active && pool[i].type == type) n++;
    return n;
}

Entity *ent_at(u8 i) { return &pool[i]; }
Entity *ent_pool(void) { return pool; }   /* 先頭ポインタ(ポインタ加算走査で乗算を避ける用) */

u8 ent_live_turrets(void) {
    u8 i, n = 0;
    for (i = 0; i < ENT_MAX; i++)
        if (pool[i].active && pool[i].type == ET_TURRET && pool[i].hp) n++;
    return n;
}

/* ★画面弾幕リミッタ: 敵弾(TEAM_ENEMYの通常弾＋信管弾)の同時数が ENEMY_BULLET_CAP に達していれば 1。
   全ての敵弾spawn(emit/emit_burst/炸裂破片)がこれを見て「上限突破しないなら出す/するなら出さない」。
   上限到達で即抜け(残スロットは走査しない)＝高速。 */
u8 ent_enemy_bullet_full(void) {
    u8 i, n = 0; Entity *e = pool;
    for (i = 0; i < ENT_MAX; i++, e++)
        if (e->active && ((e->type == ET_BULLET && e->team == TEAM_ENEMY) || e->type == ET_AABURST))
            if (++n >= ENEMY_BULLET_CAP) return 1;
    return 0;
}

void ent_spawn_explosion(s16 x, s16 y) {
    Entity *e = ent_spawn(ET_EXPLOSION);
    if (e) { e->x = x; e->y = y; e->pat = SPR_EXP0; e->color = 15; e->ftimer = 16; }
}
void ent_spawn_spark(s16 x, s16 y) {
    Entity *e = ent_spawn(ET_SPARK);
    if (e) { e->x = x; e->y = y; e->pat = SPR_FLASH; e->color = 15; e->ftimer = 6; }
}

/* ---- 当たり判定 ---- */
u8 g_kills;
u8 g_gun_kills;
u8 g_playerhit;
u8 g_pinv;
u8 g_miss;
u8 g_hitstop;
u8 g_shake;

/* 16x16 実体の AABB 重なり(やや甘めのマージン14)。 */
static u8 overlap(const Entity *a, const Entity *b) {
    s16 dx = a->x - b->x, dy = a->y - b->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx < 14 && dy < 14);
}

void ent_resolve_collisions(void) {
    u8 i, j;
    Entity *b, *t, *p, *e;
    /* ★ポインタ加算で走査(pool[j]の添字アクセスは毎回 j*sizeof(Entity) の乗算=Z80で重い。
       ネスト576×2回で乗算1000回超=もっさりの主因だった。b++/t++ は定数加算で乗算を消す)。 */
    /* 自機弾(TEAM_PLAYER) × 敵戦闘機/砲台 → 弾消滅、戦闘機は即撃破、砲台は hp 減算 */
    for (i = 0, b = pool; i < ENT_MAX; i++, b++) {
        if (!b->active || b->type != ET_BULLET || b->team != TEAM_PLAYER) continue;
        for (j = 0, t = pool; j < ENT_MAX; j++, t++) {
            if (!t->active) continue;
            if ((t->type == ET_FIGHTER || t->type == ET_PURSUER || t->type == ET_PARKED) && overlap(b, t)) {
                b->active = 0; t->active = 0; g_kills++;
                g_score += (t->type == ET_PURSUER) ? 20 : 10;   /* 追尾機20 / 戦闘機・停泊機10 */
                ent_spawn_explosion(t->x, t->y);   /* 停泊機も自機弾で破壊(体当り判定は持たない) */
                break;
            }
            if (t->type == ET_TURRET && t->hp && overlap(b, t)) {
                b->active = 0;
                if (--t->hp == 0) { t->hidden = 1; g_gun_kills++; g_score += 60; ent_spawn_explosion(t->x, t->y);
                                    sfx(2, SFX_BOOM);              /* ★主砲撃破の爆発音(欠落バグ修正。AAだけ鳴っていた) */
                                    g_hitstop = 4; g_shake = 8; }   /* 撃破=手応え(凍結＋揺れ)。activeは維持し炎上させる */
                else { t->h = 6; ent_spawn_spark(b->x, b->y); }   /* 非撃破=砲身が白フラッシュ(h)＋火花 */
                break;
            }
        }
    }
    /* 敵弾(TEAM_ENEMY)/敵戦闘機 × 自機 → 敵を消し被弾+1 */
    for (i = 0, p = pool; i < ENT_MAX; i++, p++) {
        if (!p->active || p->type != ET_PLAYER) continue;
        for (j = 0, e = pool; j < ENT_MAX; j++, e++) {
            if (!e->active) continue;
            if ((e->type == ET_BULLET && e->team == TEAM_ENEMY) || e->type == ET_FIGHTER
                || e->type == ET_AABURST || e->type == ET_PURSUER
                || (e->type == ET_SMISSILE && e->ax >= 2)) {   /* ミサイルは浮上後のみ危険 */
                /* ★自機の当たりは旧版準拠でタイト: 弾/破片=±6(旧±5)、体当り系=±9(旧±8-10)。
                   共通overlap(±14)のままだと自機が大きすぎて理不尽=避けても被弾する。 */
                u8 tol = (e->type == ET_BULLET || e->type == ET_AABURST) ? 6 : 9;
                s16 dx = e->x - p->x, dy = e->y - p->y;
                if (dx < 0) dx = -dx; if (dy < 0) dy = -dy;
                if (dx < tol && dy < tol) {
                    e->active = 0;                 /* 敵/敵弾は消す(すり抜け防止) */
                    if (g_pinv == 0 && !g_invinc) {/* 被弾直後の無敵中/設定無敵 は無傷 */
                        g_playerhit++;
                        sfx(0, SFX_PHIT);          /* 被弾の痛み音(tone A) */
                        ent_spawn_explosion(p->x, p->y);
                        g_hitstop = 5; g_shake = 12;   /* 被弾=強い手応え(凍結＋大きめ揺れ) */
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
    for (i = 0; i < 32; i++) slot_col[i] = 0xFF;   /* 色キャッシュ無効化(面開始/再開で色表を必ず書直す) */
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
            e->coltab = (const u8 *)0;
            e->shadow = 0;
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
/* ★色表(mode2=16B/枚)を毎フレーム全枚書くと弾数比例で重い(もたつきの一因)。前フレームと同じ単色なら
   VRAMに既にその色=16B書込みを省く(弾は殆ど橙12で殆ど省ける)。coltabは毎回書きキャッシュ無効(0xFF)。
   隠し(Y=216)は属性のみ変更で色表は残るためキャッシュ有効。ent_reset で 0xFF 初期化。 */
static void spr_col1(u8 slot, u8 color) {
    if (slot_col[slot] != color) { vdp_sprite_color(slot, color); slot_col[slot] = color; }
}
static u8 draw1(u8 slot, const Entity *e) {   /* 1体を slot へ描画し、次 slot を返す */
    if (e->coltab) { vdp_sprite_color_tab(slot, e->coltab); slot_col[slot] = 0xFF; }  /* 行別色(陰影) */
    else           spr_col1(slot, e->color);                                          /* 単色(差分のみ書換) */
    vdp_sprite_pos(slot, (u8)e->x, (u8)e->y, e->pat);
    return (u8)(slot + 1);
}

/* 落ち影: 自身のパターンを暗色13で右下へオフセット描画(海面に落ちた影)。 */
#define SHADOW_DX 5
#define SHADOW_DY 6
static u8 draw_shadow(u8 slot, const Entity *e) {
    spr_col1(slot, 13);   /* ほぼ黒(1,1,1)。単色=差分書換 */
    vdp_sprite_pos(slot, (u8)(e->x + SHADOW_DX), (u8)(e->y + SHADOW_DY), e->pat);
    return (u8)(slot + 1);
}
static u8 visible(const Entity *e, u8 skip_player) {
    if (!e->active || e->hidden) return 0;
    if (skip_player ? (e->type == ET_PLAYER) : (e->type != ET_PLAYER)) return 0;
    /* スプライトX/Yは u8。画面外(特に x<0/y<0)は (u8)化で反対端へ折り返すので描画しない
       (左端を抜けた弾が右端に出る等を防ぐ)。x は 0..255(=画面幅), y は -16..212 を可視域とする。 */
    return (e->y > -16 && e->y < 212 && e->x >= 0 && e->x < 256);
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
    /* 合体弾パス(双子艦): hidden な ET_COMBO を「中心±off の2発」として描く。 */
    for (i = 0; i < ENT_MAX && slot < 31; i++) {
        const Entity *e = &pool[i];
        if (e->active && e->type == ET_COMBO && e->y > -16 && e->y < 212) {
            s16 cx = e->ay, cy = e->y, off = e->x, lx = cx - off, rx = cx + off;
            if (lx >= 0 && lx < 256) { spr_col1(slot, 12); vdp_sprite_pos(slot, (u8)lx, (u8)cy, SPR_EBSHELL); slot++; }
            if (slot < 32 && rx >= 0 && rx < 256) { spr_col1(slot, 12); vdp_sprite_pos(slot, (u8)rx, (u8)cy, SPR_EBSHELL); slot++; }
        }
    }
    /* 落ち影パス: 影は最後=最も高いslot=最低優先で描く(混雑ラインではゲーム弾/敵機に譲って先に落ちる)。 */
    for (i = 0; i < ENT_MAX && slot < 32; i++) {
        const Entity *e = &pool[i];
        if (e->active && !e->hidden && e->shadow &&
            e->x >= 0 && e->x < 256 && e->y > -16 && e->y < 212)
            slot = draw_shadow(slot, e);
    }
    vdp_sprite_hide_from(slot);
    rot++;
}
