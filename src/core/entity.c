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
/* ★A1: 各スロットに最後に書いた行別色表(coltab)のポインタ(0=単色書込み後で無効)。
   同一slotに同一coltabが既に載っていれば16B書込みを省く。単色書込み時は必ず 0 にして
   「そのslotのVRAMはもう coltab でない」を記録する(=以後の同ポインタ判定が VRAM 実体と一致)。 */
static const u8 *slot_ctab[32];

/* ---- behavior: 種別ごとの毎フレーム更新 ---- */
/* ET_NONE/ET_BOUNCER/ET_SHOOTER の no-op behavior(実ゲームでspawnしないデモ枠。enum添字対応の
   ため behaviors[] のスロットは残すが、コードは共有stubで常駐サイズを節約)。 */
static void bh_noop(Entity *e) { (void)e; }

/* 弾: 直進し画面外(±16マージン)で消滅。
   ★スクロールとは完全に独立=毎フレーム画面座標を vx/vy だけ進める(表示上の見かけ速度が一定)。
     蛇行の上り/下り・左右に一切影響されない(甲板追従の補正は入れない)。 */
static void bh_bullet(Entity *e) {
    e->x += e->vx;
    e->y += e->vy;
    /* ★敵弾は艦(背景)と一緒に縦スクロールへ流れる。論理Y自体を流すので、描画・当たり判定・
       画面外消滅・弾数リミッタが全て視覚と一致する(自機弾=TEAM_PLAYERは画面固定のまま)。 */
    if (e->team == TEAM_ENEMY) e->y -= g_scroll_dy;
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
    e->y += e->vy;
    e->y -= g_scroll_dy;         /* ★信管弾も敵弾=艦と一緒に縦スクロールへ流れる(論理Yを流す) */
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
                    g_ebul++;   /* ★敵弾カウンタ(上限O(1)判定用) */
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
                     b->color = 12; b->pat = SPR_BULLET; g_ebul++; } } }   /* ★敵弾カウンタ */
        ent_spawn_explosion(e->x, e->y); sfx(2, SFX_HIT);
        e->active = 0;
    }
}

/* 合体弾の予告(双子艦): 左右2発(cx±off)が中心へ収束(off→0)。合体で自機狙いの高速大弾を発射。
   ay=中心x, y=中心y, x=半間隔off(収束), ftimer=残収束フレーム。実体は hidden=1(描画は ent_draw_all の合体パス)。 */
#define CB_GAP      52    /* 収束開始の半間隔(左右の艦=中心±52) */
#define CB_CONVERGE 24    /* 収束フレーム数 */
static void bh_combo(Entity *e) {
    e->y -= g_scroll_dy;                               /* ★合体弾(予告)も敵弾=艦と一緒に流れる */
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
    bh_noop,      /* ET_BOUNCER(デモ枠=no-op) */
    bh_bullet,    /* ET_BULLET    */
    bh_noop,      /* ET_SHOOTER(デモ枠=no-op)  */
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
    u8 i, n = 0; Entity *e = pool;   /* ポインタ加算走査=pool[i]の乗算を排除 */
    for (i = 0; i < ENT_MAX; i++, e++) if (e->active && e->type == type) n++;
    return n;
}

Entity *ent_at(u8 i) { return &pool[i]; }
Entity *ent_pool(void) { return pool; }   /* 先頭ポインタ(ポインタ加算走査で乗算を避ける用) */

/* ★生存(hp>0)砲台数のO(1)カウンタ。従来は毎フレーム2回(rage/クリア判定)プールをO(30)走査していた。
   spawn_turretで++、当たり判定で撃破(hp→0)時に--。面リスタートは stage_build が0初期化して再spawn。 */
u8 g_lturret;
u8 ent_live_turrets(void) { return g_lturret; }

/* ★敵弾(TEAM_ENEMY通常弾＋信管弾)の同時数のO(1)カウンタ。従来は ent_enemy_bullet_full が発砲弾ごとに
   O(30)全走査していた(4主砲×5way＋AA＋破片で1フレーム数百〜千イテレーション=砲台/敵増で線形悪化)。
   毎フレーム頭で ent_recount_ebul() が1回だけ数え直し(seed)、フレーム内の敵弾spawnで g_ebul++。
   死亡時の減算は次フレームの数え直しが吸収するので不要(=減算漏れバグが原理的に起きない安全設計)。 */
u8 g_ebul;
void ent_recount_ebul(void) {
    u8 i, n = 0; Entity *e = pool;
    for (i = 0; i < ENT_MAX; i++, e++)
        if (e->active && ((e->type == ET_BULLET && e->team == TEAM_ENEMY) || e->type == ET_AABURST)) n++;
    g_ebul = n;
}
u8 ent_enemy_bullet_full(void) { return (u8)(g_ebul >= ENEMY_BULLET_CAP); }

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

void ent_resolve_collisions(void) {
    /* ★プール全走査を1回に統合。従来は「敵ターゲット収集」「自機弾探索」「自機探索＋脅威内側30走査」で
       都合3〜4回プールを舐めていた(各回で active/type を読み直し)。1走査で以下へ分類し、以後はコンパクト
       集合だけを回す(判定・スコア・効果は従来と完全同一):
         pb[]  = 自機弾(TEAM_PLAYER)          … 自機弾×ターゲットの外側
         tgt[] = 戦闘機/追尾/停泊/砲台         … 自機弾の当たり相手
         th[]  = 自機に当たる脅威(敵弾/戦闘機/信管弾/追尾/浮上ミサイル) … 自機×脅威
         player = 自機 */
    u8 i, j, npb = 0, nt = 0, nth = 0;
    Entity *b, *t, *e;
    Entity *pb[ENT_MAX], *tgt[ENT_MAX], *th[ENT_MAX], *player = (Entity *)0;
    e = pool;
    for (i = 0; i < ENT_MAX; i++, e++) {
        u8 ty;
        if (!e->active) continue;
        ty = e->type;
        if (ty == ET_BULLET) {
            if (e->team == TEAM_PLAYER) pb[npb++] = e;   /* 自機弾 */
            else                        th[nth++] = e;   /* 敵弾=脅威 */
        } else if (ty == ET_PLAYER) {
            player = e;
        } else if (ty == ET_FIGHTER || ty == ET_PURSUER) {
            tgt[nt++] = e; th[nth++] = e;                /* 戦闘機/追尾=ターゲットかつ脅威(体当り) */
        } else if (ty == ET_PARKED || ty == ET_TURRET) {
            tgt[nt++] = e;                               /* 停泊機/砲台=ターゲットのみ(体当り無し) */
        } else if (ty == ET_AABURST) {
            th[nth++] = e;                               /* 信管弾=脅威 */
        } else if (ty == ET_SMISSILE && e->ax >= 2) {
            th[nth++] = e;                               /* ミサイルは浮上後(相2以上)のみ危険 */
        }
    }
    /* 自機弾(TEAM_PLAYER) × 敵戦闘機/砲台 → 弾消滅、戦闘機は即撃破、砲台は hp 減算
       ★最重ホットパス(実測: 当たり判定=毎フレーム最大コスト。pb×tgt の二乗)。定数削減のため:
         (1)overlap()の関数呼びを **インライン化**(最悪225回/フレームの call/ret を排除)
         (2)弾座標 bx/by を外ループで **ホイスト**(内ループの b->x/b->y 再デリファレンスを排除)
         (3)**X軸で早期棄却**(dx>=14 なら dy 計算前に continue=大半のペアを最短で捨てる)
         (4)t->type を1回だけ読み **tt にキャッシュ**。判定・スコア・効果は従来と完全同一。 */
    for (i = 0; i < npb; i++) {
        s16 bx, by;
        b = pb[i];
        if (!b->active) continue;
        bx = b->x; by = b->y;
        for (j = 0; j < nt; j++) {
            s16 dx, dy; u8 tt;
            t = tgt[j];
            if (!t->active) continue;   /* 先に別の弾で消えた可能性(集合はstale許容) */
            dx = t->x - bx; if (dx < 0) dx = -dx; if (dx >= 14) continue;   /* X早期棄却 */
            dy = t->y - by; if (dy < 0) dy = -dy; if (dy >= 14) continue;   /* Y(=overlap成立) */
            tt = t->type;
            if (tt == ET_FIGHTER || tt == ET_PURSUER || tt == ET_PARKED) {
                b->active = 0; t->active = 0; g_kills++;
                g_score += (tt == ET_PURSUER) ? 20 : 10;   /* 追尾機20 / 戦闘機・停泊機10 */
                ent_spawn_explosion(t->x, t->y);   /* 停泊機も自機弾で破壊(体当り判定は持たない) */
                break;
            }
            if (tt == ET_TURRET && t->hp) {
                b->active = 0;
                if (--t->hp == 0) { t->hidden = 1; g_gun_kills++; g_lturret--; g_score += 60; ent_spawn_explosion(t->x, t->y);
                                    sfx(2, SFX_BOOM);              /* ★主砲撃破の爆発音(欠落バグ修正。AAだけ鳴っていた) */
                                    g_hitstop = 4; g_shake = 8; }   /* 撃破=手応え(凍結＋揺れ)。activeは維持し炎上させる。★g_lturret--=生存砲台O(1)カウンタ */
                else { t->h = 6; ent_spawn_spark(b->x, b->y); }   /* 非撃破=砲身が白フラッシュ(h)＋火花 */
                break;
            }
        }
    }
    /* 敵弾(TEAM_ENEMY)/敵戦闘機/… × 自機 → 敵を消し被弾+1 */
    if (player) {
        Entity *p = player;
        for (j = 0; j < nth; j++) {
            u8 tol;
            s16 dx, dy;
            e = th[j];
            if (!e->active) continue;
            /* ★自機の当たりは旧版準拠でタイト: 弾/破片=±6、体当り系=±9(共通±14だと避けても被弾で理不尽)。 */
            tol = (e->type == ET_BULLET || e->type == ET_AABURST) ? 6 : 9;
            dx = e->x - p->x; dy = e->y - p->y;
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

void ent_reset(void) {
    u8 i;
    for (i = 0; i < ENT_MAX; i++) pool[i].active = 0;
    for (i = 0; i < 32; i++) { slot_col[i] = 0xFF; slot_ctab[i] = 0; }   /* 色キャッシュ無効化(面開始/再開で色表を必ず書直す) */
    g_ebul = 0;
}

Entity *ent_spawn(u8 type) {
    u8 i; Entity *e = pool;   /* ★ポインタ加算走査=pool[i]の乗算を排除。発砲中は弾/爆発/火花で多発するので効く */
    for (i = 0; i < ENT_MAX; i++, e++) {
        if (!e->active) {
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
    if (slot_col[slot] != color) { vdp_sprite_color(slot, color); slot_col[slot] = color; slot_ctab[slot] = 0; }
}
/* ★draw1: 描画の最ホットパス(毎フレーム最大32回)。SDCCのCコードは Entity* をレジスタに保持できず
   フィールドアクセス毎に pop/push でスタック往復＋IXフレーム＋4引数の vdp_sat_pos 呼びでスタック渡し、と
   膨大な殻を生んでいた(生成ASMで確認)。ここを __naked ASM で e を IY に固定=全フィールドをオフセット直読み、
   SAT影は sat_shadow へ直書き(vdp_sat_pos呼びを排除)に置換。色判定/効果は従来と完全同一(検証: SATダンプ一致)。
   規約: A=slot, DE=e, 戻り A=slot+1(SDCC観測)。色関数 spr_col1(A=slot,L=color)/vdp_sprite_color_tab(A=slot,DE=coltab)。
   Entity offset: x=2, y=4, color=16, pat=17, coltab=24(2B)。sat_shadow/g_vscroll は vdp.c の非staticグローバル。 */
static u8 d1slot;              /* ASM draw1のslot退避(呼び出しを跨いで保持) */
static const u8 *d1ctab;      /* ASM draw1のcoltab退避(vdp_sprite_color_tab呼びでDE破壊のため) */
static u8 draw1(u8 slot, const Entity *e) __naked {
    (void)slot; (void)e;
    __asm
        ld   (_d1slot), a          ; slot保存
        push iy
        push de
        pop  iy                    ; IY = e
        ; --- SAT影書込(IYがe確実。呼び出し前に完了) ---
        ld   a, (_d1slot)
        ld   l, a
        ld   h, #0
        add  hl, hl
        add  hl, hl                ; slot*4
        ld   de, #_sat_shadow
        add  hl, de                ; HL = &sat_shadow[slot*4]
        ld   a, 4 (iy)             ; e->y(低位)
        ld   c, a
        ld   a, (_g_vscroll)
        add  a, c
        dec  a
        ld   (hl), a               ; p[0]=y+vscroll-1
        inc  hl
        ld   a, 2 (iy)             ; e->x(低位)
        ld   (hl), a               ; p[1]=x
        inc  hl
        ld   a, 17 (iy)            ; e->pat
        ld   (hl), a               ; p[2]=pat
        ; --- 色 ---
        ld   e, 24 (iy)            ; coltab低
        ld   d, 25 (iy)            ; coltab高
        ld   a, d
        or   e
        jr   z, 00011$             ; coltab==0 → 単色
        ; coltab有: slot_ctab[slot]==coltab? (DE=coltab)
        ld   a, (_d1slot)
        ld   l, a
        ld   h, #0
        add  hl, hl                ; slot*2
        ld   bc, #_slot_ctab
        add  hl, bc                ; &slot_ctab[slot]
        ld   a, (hl)
        inc  hl
        ld   h, (hl)
        ld   l, a                  ; HL=既存coltab
        or   a
        sbc  hl, de                ; ==coltab?
        jr   z, 00019$             ; 同一=16B書込省略
        ld   (_d1ctab), de         ; coltab退避
        ld   a, (_d1slot)
        call _vdp_sprite_color_tab ; A=slot, DE=coltab
        ld   a, (_d1slot)          ; slot_ctab[slot]=coltab
        ld   l, a
        ld   h, #0
        add  hl, hl
        ld   bc, #_slot_ctab
        add  hl, bc
        ld   de, (_d1ctab)
        ld   (hl), e
        inc  hl
        ld   (hl), d
        ld   a, (_d1slot)          ; slot_col[slot]=0xFF
        ld   l, a
        ld   h, #0
        ld   bc, #_slot_col
        add  hl, bc
        ld   (hl), #0xFF
        jr   00019$
    00011$:
        ld   l, 16 (iy)            ; e->color
        ld   a, (_d1slot)
        call _spr_col1             ; A=slot, L=color
    00019$:
        pop  iy
        ld   a, (_d1slot)
        inc  a                     ; 戻り=slot+1
        ret
    __endasm;
}

/* 落ち影: 自身のパターンを暗色13で右下へオフセット描画(海面に落ちた影)。 */
#define SHADOW_DX 5
#define SHADOW_DY 6
static u8 draw_shadow(u8 slot, const Entity *e) {
    spr_col1(slot, 13);   /* ほぼ黒(1,1,1)。単色=差分書換 */
    vdp_sat_pos(slot, (u8)(e->x + SHADOW_DX), (u8)(e->y + SHADOW_DY), e->pat);   /* ★A6: シャドウへ */
    return (u8)(slot + 1);
}
/* ★プール全走査を1回に統合(従来は自機探索/可視収集/合体弾/影 で4回走査していた=各エンティティの
   active/type/x/y を4回読み直していた)。1走査で自機・通常描画対象(vis)・合体弾(co)・影(sh)へ分類し、
   以後は各リストだけを回す。描画順(自機→回転割当vis→合体弾→影)とスロット優先/影の包含は従来と同一。 */
void ent_draw_all(void) {
    static u8 rot;
    u8 i, j, n = 0, nco = 0, nsh = 0, slot = g_spr_base;
    Entity *vis[ENT_MAX], *co[ENT_MAX], *sh[ENT_MAX];
    Entity *player = (Entity *)0;
    Entity *e = pool;
    for (i = 0; i < ENT_MAX; i++, e++) {
        u8 ty;
        if (!e->active) continue;
        ty = e->type;
        if (ty == ET_COMBO) {                                   /* 合体弾(hidden)は専用パス。y域のみ判定 */
            if (e->y > -16 && e->y < 212) co[nco++] = e;
            continue;                                           /* hidden=vis/影の対象外 */
        }
        if (e->hidden || e->y <= -16 || e->y >= 212 || e->x < 0 || e->x >= 256) continue;  /* 画面外/不可視 */
        if (ty == ET_PLAYER) player = e;                        /* 自機=最優先スロット(絶対に欠けさせない) */
        else                 vis[n++] = e;                      /* 通常描画対象 */
        if (e->shadow) sh[nsh++] = e;                           /* 影(自機/敵機。可視かつshadow) */
    }
    /* 自機を固定最優先スロット(g_spr_base)へ */
    if (player) slot = draw1(slot, player);
    /* 収集集合内で開始位置を毎フレーム回転させて割当(9枚/走査線超の欠落をちらつきへ均等分散)。
       ★D2(§D2 逆順SAT): 交互フレームで割当て順を"正順/逆順"に反転する。SATは低slot=高優先(1走査線8枚まで)
         なので、順を反転すると混雑ラインで"表示される8枚"が前半⇔後半で交互に入れ替わる=消える弾が
         30Hzで入れ替わり実質16枚/ラインに見える(自機は上で最優先slot固定=ちらつかない)。 */
    if (n) {
        u8 start = (u8)(rot % n);
        u8 rev = (u8)(rot & 1);   /* 交互フレームで反転 */
        for (j = 0; j < n && slot < 32; j++) {
            u8 d = rev ? (u8)(n - 1 - j) : j;   /* 逆順フレームは末尾から割当て=優先が反転 */
            i = (u8)(start + d); if (i >= n) i -= n;
            slot = draw1(slot, vis[i]);
        }
    }
    /* 合体弾パス(双子艦): 中心±off の2発として描く。 */
    for (i = 0; i < nco && slot < 31; i++) {
        s16 cx = co[i]->ay, cy = co[i]->y, off = co[i]->x, lx = cx - off, rx = cx + off;
        if (lx >= 0 && lx < 256) { spr_col1(slot, 12); vdp_sat_pos(slot, (u8)lx, (u8)cy, SPR_EBSHELL); slot++; }
        if (slot < 32 && rx >= 0 && rx < 256) { spr_col1(slot, 12); vdp_sat_pos(slot, (u8)rx, (u8)cy, SPR_EBSHELL); slot++; }
    }
    /* 落ち影パス: 影は最後=最も高いslot=最低優先(混雑ラインではゲーム弾/敵機に譲って先に落ちる)。 */
    for (i = 0; i < nsh && slot < 32; i++) slot = draw_shadow(slot, sh[i]);
    /* ★A6: 溜めた属性(g_spr_base..slot-1)を1回のバーストでSATへ(flush内で停止マーカも直書き)。 */
    vdp_sat_flush(g_spr_base, slot);
    rot++;
}
