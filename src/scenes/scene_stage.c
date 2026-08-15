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

/* 戦艦の地形(長い=画面より縦416px)。OPS_RECTのyはu8(255まで)なので上下2パスで描く。
   船首(細)=上端(先に見える)、船尾=下端。砲塔は後段でスプライト化。 */
/* 砲塔は破壊可能スプライト(ET_TURRET)なので艦グラフィックには描かない。 */
static const u8 ship_top[] = {      /* 世界 y0..255 を SHIPBUF_Y へ */
    OPS_RECT, 26,   0, 12, 16, 14,  /* 船首テーパ(細) */
    OPS_RECT, 22,  16, 20, 16, 14,
    OPS_RECT, 16,  32, 32, 16, 14,
    OPS_RECT,  8,  48, 48, 207, 14, /* 船体前半 y48..255 */
    OPS_RECT, 14,  92, 36, 40,  7,  /* 艦橋 */
    OPS_RECT, 20, 150, 24, 26,  6,  /* 煙突 */
    OPS_END
};
static const u8 ship_bot[] = {      /* 世界 y256..416 を SHIPBUF_Y+256 へ(local y=worldY-256) */
    OPS_RECT,  8,   0, 48, 116, 14, /* 船体後半 world256..372 */
    OPS_RECT, 16, 116, 32, 16, 14,  /* 船尾テーパ world372 */
    OPS_RECT, 22, 132, 20, 16, 14,
    OPS_RECT, 26, 148, 12, 12, 14,
    OPS_END
};

/* 砲塔の発砲: 55f毎に自機狙い4-way散弾(偶数=自機直線上に隙間)。 */
static const u8 fd_gun[] = { 55, FIRE_AIMFAN, 4, 2, 3, FIRE_END };

/* 砲塔(破壊可能)を艦上の世界座標に配置。船体中央 x=120。世界Y=艦頭(SC_SHIP_R0*16)+艦内y。 */
static void spawn_turret(u16 shipY, u8 delay) {
    Entity *e = ent_spawn(ET_TURRET);
    if (e) {
        e->ax = 120; e->ay = (s16)(SC_SHIP_R0 * 16 + shipY);
        e->color = 8; e->pat = SPR_BLOCK; e->hp = 2; e->fire = fd_gun; e->ftimer = delay;
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
static u8  sdiv, wtimer;
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
    ent_reset();
    cam = SC_CAM_START; phase = 0; sdiv = 0; wtimer = 0;
    weaveX = 0; wdir = 1; camdir = -1; g_meander = 0;
    vdp_set_hscroll(0, 0);

    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 176; e->color = 15; e->pat = SPR_BLOCK; }

    /* 破壊可能砲塔(艦上の世界座標に配置。海フェーズ中は画面外)。全撃破でクリア。 */
    g_gun_kills = 0;
    spawn_turret(60,  30);   /* 主砲(前) */
    spawn_turret(200, 50);   /* 主砲(中) */
    spawn_turret(310, 70);   /* 主砲(後) */
}

u8 stage_update(void) {
    if (phase == 0) {
        /* 海: 蛇行なしの直進。船尾が見えたら(=cam<=STERN)交戦フェーズへ地続きに移行 */
        if (++sdiv >= 2) { sdiv = 0; if (cam > SC_CAM_STERN) cam--; }
        scroll_to(cam);
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

    /* 全砲台撃破でクリア → エンディング(戦艦フェーズでのみ0になる) */
    if (phase == 1 && ent_count(ET_TURRET) == 0) return SC_ENDING;
    return SCENE_NONE;
}
