/* scene_boss.c — ★新ルール第2段: 戦艦ボス(蛇行スクロール)。
   艦体は run_ops(データ駆動)で描画。蛇行=横スクロール R#26/27(スプライト非影響)。
   ★砲台は破壊可能な ET_TURRET(自機狙い散弾 AIMFAN)。全砲台を撃破するとクリア→エンディング(HANDOFF §1)。
   ※プロト簡略化: 蛇行は艦体(VRAM)のみ揺らし、砲台(スプライト)は画面固定。本番で砲塔追従を入れる。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "sprites.h"
#include "fire.h"
#include "ops.h"

/* データ駆動の戦艦(縦長・上面図): 船体(灰)＋前部艦橋(シアン)＋中央構造(暗赤)。砲台はスプライトで別途。 */
static const u8 ship_boss[] = {
    OPS_RECT,  0,   0, 64, 160, 14,
    OPS_RECT,  8,  20, 48,  28,  7,
    OPS_RECT, 12,  74, 40,  36,  6,
    OPS_END
};

/* 砲台の発砲: 70f毎に自機狙いの 4-way 散弾(偶数=自機直線上に隙間)。 */
static const u8 fd_gun[] = { 70, FIRE_AIMFAN, 4, 2, 3, FIRE_END };

static u16 boss_t;
static u8  mtimer;
static u8  last_gun;   /* 撃破音のエッジ検出 */

/* 破壊可能な砲台(見える的)。hp=2、自機狙い散弾。 */
static void spawn_turret(s16 x, s16 y, u8 delay) {
    Entity *e = ent_spawn(ET_TURRET);
    if (e) { e->x = x; e->ax = x; e->y = y; e->color = 8; e->pat = SPR_BLOCK; e->hp = 2; e->fire = fd_gun; e->ftimer = delay; }
}

void boss_init(void) {
    Entity *e;
    vdp_set_vscroll(0);          /* 縦スクロール停止(スプライト補正も解除) */
    vdp_set_hscroll(0, 0);
    vdp_set_display_page(0);     /* 蛇行は横スクロールのみ=page0 で描画/表示 */
    vdp_fill(0, 0, 256, 212, 4); /* 海青 */
    run_ops(96, 26, ship_boss);  /* 艦体を中央へ描画 */
    vdp_sprite_init();
    sprites_load();
    ent_reset();
    boss_t = 0; mtimer = 0;
    g_gun_kills = 0; last_gun = 0;

    /* 自機(下部・入力操作) */
    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 188; e->color = 15; e->pat = SPR_BLOCK; }

    /* 砲台3基(艦首/中央/艦尾, 中央列に縦積み)。全撃破でクリア。 */
    spawn_turret(120, 40,  30);
    spawn_turret(120, 96,  50);
    spawn_turret(120, 150, 70);
}

u8 boss_update(void) {
    u8 ph, o;

    /* 蛇行 = 横スクロールを 0..15..0 で緩やかに振る */
    mtimer++;
    ph = mtimer & 63;
    o = (ph < 32) ? (u8)(ph >> 1) : (u8)((63 - ph) >> 1);
    vdp_set_hscroll(o >> 3, o & 7);
    g_meander = o;   /* 砲塔スプライトが同量ぶん追従(bh_turret) */

    boss_t++;
    if ((boss_t % 70) == 0) sfx(1, SFX_HIT);          /* 発砲音(周期一致) */

    ent_update_all();
    ent_resolve_collisions();
    if (g_gun_kills != last_gun) { sfx(2, SFX_BOOM); last_gun = g_gun_kills; }   /* 砲台撃破音 */
    ent_draw_all();

    /* 全砲台撃破でクリア → エンディング */
    if (ent_count(ET_TURRET) == 0) return SC_ENDING;
    return SCENE_NONE;
}
