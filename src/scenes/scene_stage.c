/* scene_stage.c — ★1本の連続縦スクロール面(海→戦艦 地続き。画面カット無し)。
   フェーズ0: 海=蛇行なしの直進スクロール(前進)。戦艦の船尾が見えたら交戦へ。
   フェーズ1: 戦艦=船首↔船尾の往復蛇行スクロール(縦の往復＋横揺れ weaveX)＝前作の戦艦戦。
   自機は下部固定(scroll側のY補正＋weaveXは自機に非適用)。砲塔/戦闘機/撃破は次段で統合。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sprites.h"
#include "scroll.h"
#include "ops.h"
#include "fire.h"
#include "sound.h"
#include "hud.h"
#include "gamestate.h"

/* ★1面ビスマルク艦体(上面視, 縦416px)。OPS_RECTのyはu8(255まで)なので上下2パスで描く。
   艦の中心x=128(=base96 + rectの中心)。船首(細)=上端(先に到達)、船尾=下端。
   色: 船体/テーパ=灰14 / 木甲板=暗黄10 / 上構(艦橋・煙突・後部)=黒1 / 艦橋頂=白15。
   主砲塔は破壊可能スプライト(ET_TURRET)。ここでは甲板に砲塔マウント(黒)だけ描き、砲身は sprite。
   前部=Anton/Bruno(超越射撃), 後部=Caesar/Dora の4基(ビスマルク配置)。 */
static const u8 ship_top[] = {      /* 世界 y0..255 を SHIPBUF_Y へ(船首側) */
    /* 船首テーパ(先細り, 中心x=128) */
    OPS_RECT, 24,   0, 16,  8, 14,
    OPS_RECT, 18,   8, 28,  8, 14,
    OPS_RECT, 12,  16, 40, 10, 14,
    OPS_RECT,  6,  26, 52, 14, 14,
    /* 主船体(前半) x100..156, y40..255 */
    OPS_RECT,  4,  40, 56, 215, 14,
    /* 木甲板ストライプ(内側) */
    OPS_RECT, 14,  48, 36, 207, 10,
    /* 前部主砲マウント(Anton y72 / Bruno y108=超越) */
    OPS_RECT, 22,  72, 20, 16,  1,
    OPS_RECT, 22, 108, 20, 16,  1,
    /* 艦橋タワー(前部構造) y126..176 ＋頂部(白) */
    OPS_RECT, 16, 126, 32, 50,  1,
    OPS_RECT, 22, 134, 20, 20, 15,
    /* 煙突 y196..232 ＋キャップ(灰) */
    OPS_RECT, 20, 196, 24, 36,  1,
    OPS_RECT, 24, 196, 16,  6, 14,
    OPS_END
};
static const u8 ship_bot[] = {      /* 世界 y256..416 を SHIPBUF_Y+256 へ(船尾側, local y=worldY-256) */
    /* 主船体(後半) x100..156, world256..376 */
    OPS_RECT,  4,   0, 56, 120, 14,
    OPS_RECT, 14,   0, 36, 116, 10,  /* 木甲板続き */
    /* 後部構造(メインマスト基部) local12..42 */
    OPS_RECT, 18,  12, 28, 30,  1,
    /* 後部主砲マウント(Caesar world300→local44 / Dora world344→local88) */
    OPS_RECT, 22,  44, 20, 16,  1,
    OPS_RECT, 22,  88, 20, 16,  1,
    /* 船尾テーパ local120..160 */
    OPS_RECT,  6, 120, 52, 12, 14,
    OPS_RECT, 12, 132, 40, 12, 14,
    OPS_RECT, 20, 144, 24, 10, 14,
    OPS_RECT, 26, 154, 12,  6, 14,
    OPS_END
};

/* 主砲の発砲: 50f毎に自機狙い4-way散弾(偶数=自機直線上に隙間)。
   ★suppress=24: 半径内(≒ゼロ距離)に自機が居ると発射スキップ=肉薄で撃たせない。難易度で半径増減。 */
static const u8 fd_gun[]  = { 50, 24, FIRE_AIMFAN, 4, 2, 3, FIRE_END };
/* 戦闘機の発砲: 45f毎に自機狙い＋散らし円錐(±3)。空中の的なので抑え込みは無し(suppress=0)。 */
static const u8 fd_faim[] = { 45,  0, FIRE_AIMED, 3, 1, 2, FIRE_END };

static u16 rng;
static u8  rnd(void) { rng = rng * 25173 + 13849; return (u8)(rng >> 8); }

/* 主砲塔(破壊可能)を艦上の世界座標に配置。艦中心 x=120(sprite左上→中心128)。世界Y=艦頭(SC_SHIP_R0*16)+艦内y。
   hp=3: 抑え込み(肉薄で撃たせない)で安全に連射しないと落としにくい=「ゼロ距離抑え込み=最速撃破」を要求。 */
static void spawn_turret(u16 shipY, u8 delay) {
    Entity *e = ent_spawn(ET_TURRET);
    if (e) {
        e->ax = 120; e->ay = (s16)(SC_SHIP_R0 * 16 + shipY);
        e->color = 14; e->pat = SPR_TURRET; e->hp = 3; e->fire = fd_gun; e->ftimer = delay;
    }
}

/* 戦艦をバッファB(page2/3)へ事前描画: 海地＋上下2パスの艦体。 */
static void prerender_ship(void) {
    vdp_fill(0, SC_SHIPBUF_Y, 256, SC_SHIP_ROWS * 16, 4);   /* 海地(青) */
    run_ops(96, SC_SHIPBUF_Y,       ship_top);
    run_ops(96, SC_SHIPBUF_Y + 256, ship_bot);
}

#define WMAX 40   /* 横揺れ(weaveX)の振幅 */

static u16 cam;
static u8  phase;     /* 0=海(直進) / 1=戦艦(往復蛇行) */
static u8  sdiv, wtimer, ftick;
static s8  camdir;    /* 往復方向: -1=船首へ / +1=船尾へ */
static s16 weaveX;
static s8  wdir;

/* weaveX を横HWスクロール(R#26/27)へ。滑らかな左寄せは R#26=ceil(s/8)/R#27=(8-frac)。 */
static void apply_weave(void) {
    u8 s = (u8)((256 - (weaveX & 0xFF)) & 0xFF);
    u8 frac = (u8)(s & 7);
    vdp_set_hscroll((u8)(((s >> 3) + (frac ? 1 : 0)) & 0x1F), (u8)((8 - frac) & 7));
    g_meander = weaveX;   /* 砲塔スプライトの横追従(次段の砲塔統合で使用) */
}

void stage_init(void) {
    Entity *e;
    prerender_ship();     /* 艦をBへ(scroll_init前に必須) */
    scroll_init();
    vdp_sprite_init();
    sprites_load();
    hud_init();           /* 数字パターン投入＋HUDスロット確保(g_spr_base) */
    ent_reset();
    cam = SC_CAM_START; phase = 0; sdiv = 0; wtimer = 0; ftick = 0;
    weaveX = 0; wdir = 1; camdir = -1; g_meander = 0; rng = 0x1234;
    vdp_set_hscroll(0, 0);

    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 176; e->color = 15; e->pat = SPR_BLOCK; }

    /* スコア/残機の初期化(残機は config の g_lives_idx→2/3/5 機) */
    {
        static const u8 livestab[3] = { 2, 3, 5 };
        g_score = 0;
        g_kills = 0; g_playerhit = 0; g_pinv = 0;
        g_lives = livestab[(g_lives_idx < 3) ? g_lives_idx : 1];
    }

    /* 破壊可能主砲塔(ビスマルク配置=前2/後2。海フェーズ中は画面外)。全撃破でクリア。
       艦内Yは ship_top/ship_bot のマウント位置と一致(前:72/108, 後:300/344)。 */
    g_gun_kills = 0;
    spawn_turret( 72, 30);   /* Anton(前) */
    spawn_turret(108, 45);   /* Bruno(前・超越) */
    spawn_turret(300, 60);   /* Caesar(後) */
    spawn_turret(344, 75);   /* Dora(後) */
}

u8 stage_update(void) {
    if (phase == 0) {
        /* 海: 蛇行なしの直進。船尾が見えたら(=cam<=STERN)交戦フェーズへ地続きに移行 */
        if (++sdiv >= 2) { sdiv = 0; if (cam > SC_CAM_STERN) cam--; }
        scroll_to(cam);
        /* 空戦: 敵戦闘機が上から降下(約半数は自機狙い) */
        if ((++ftick % 40) == 0) {
            Entity *f = ent_spawn(ET_FIGHTER);
            if (f) {
                f->x = 24 + (rnd() % 200); f->y = -16;
                f->vx = (rnd() & 1) ? 1 : -1; f->vy = 2 + (rnd() % 2);
                f->color = 8; f->pat = SPR_FIGHTER;
                if (rnd() & 1) { f->fire = fd_faim; f->ftimer = 20 + (rnd() % 30); }
            }
            sfx(1, SFX_HIT);
        }
        if (cam <= SC_CAM_STERN) { phase = 1; camdir = -1; }
    } else {
        /* 戦艦: 船首↔船尾の往復(縦) */
        if (++sdiv >= 2) {
            sdiv = 0;
            cam = (u16)((s16)cam + camdir);
            if (cam <= SC_CAM_BOW)   camdir = 1;
            if (cam >= SC_CAM_STERN) camdir = -1;
        }
        scroll_to(cam);
        /* 蛇行(横揺れ) */
        if (++wtimer >= 3) {
            wtimer = 0;
            weaveX += wdir;
            if (weaveX >= WMAX)  wdir = -1;
            if (weaveX <= -WMAX) wdir = 1;
        }
        apply_weave();
    }

    ent_update_all();
    ent_resolve_collisions();
    ent_draw_all();
    hud_draw(g_score, g_lives);

    /* 残機尽き = ゲームオーバー → タイトルへ戻す */
    if (g_lives == 0) return SC_TITLE;
    /* 全砲台撃破でクリア → エンディング(戦艦フェーズでのみ0になる) */
    if (phase == 1 && ent_count(ET_TURRET) == 0) return SC_ENDING;
    return SCENE_NONE;
}
