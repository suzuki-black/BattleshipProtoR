/* scene_boss.c — ★新ルール第2段: 戦艦ボス(蛇行スクロール)。
   船首出現後の本番。艦体は run_ops(データ駆動)で描画。蛇行=横スクロール R#26/27(スプライト非影響)。
   主砲は艦首/艦尾の砲塔位置に置いた「不可視の発砲点(ET_SHOOTER, hidden)」が自機狙いの散弾(AIMFAN)を撒く。
   ※プロト簡略化: 蛇行(横スクロール)は艦体(VRAM)のみ揺らし、発砲点(スプライト空間)は画面固定。
     本番では砲塔追従(スプライト側の横補正)を入れる。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "sprites.h"
#include "fire.h"
#include "ops.h"

/* データ駆動の戦艦(縦長・上面図): 船体(灰)＋前部艦橋(シアン)＋中央構造(暗赤)＋艦首/艦尾主砲(赤) */
static const u8 ship_boss[] = {
    OPS_RECT,  0,   0, 64, 160, 14,
    OPS_RECT,  8,  20, 48,  28,  7,
    OPS_RECT, 12,  74, 40,  36,  6,
    OPS_RECT, 20,   2, 24,  12,  8,
    OPS_RECT, 20, 144, 24,  12,  8,
    OPS_END
};

/* 主砲(不可視発砲点)の発砲: 70f毎に自機狙いの 4-way 散弾(偶数=自機直線上は隙間)。 */
static const u8 fd_gun[] = { 70, FIRE_AIMFAN, 4, 2, 3, FIRE_END };

static u16 boss_t;
static u8  mtimer;

/* 砲塔位置に不可視の発砲点を1つ置く。 */
static void spawn_gun(s16 x, s16 y, u8 delay) {
    Entity *e = ent_spawn(ET_SHOOTER);
    if (e) { e->x = x; e->y = y; e->hidden = 1; e->fire = fd_gun; e->ftimer = delay; }
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

    /* 自機(下部・入力操作) */
    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 188; e->color = 15; e->pat = SPR_BLOCK; }

    /* 艦首/艦尾の砲塔に不可視の発砲点(自機狙い散弾) */
    spawn_gun(120, 40, 30);
    spawn_gun(120, 150, 65);
}

u8 boss_update(void) {
    u8 ph, o;

    /* 蛇行 = 横スクロールを 0..15..0 で緩やかに振る */
    mtimer++;
    ph = mtimer & 63;
    o = (ph < 32) ? (u8)(ph >> 1) : (u8)((63 - ph) >> 1);
    vdp_set_hscroll(o >> 3, o & 7);

    /* 発砲は不可視発砲点の run_fire(AIMFAN)が担う。ここは発砲音のみ(周期一致)。 */
    boss_t++;
    if ((boss_t % 70) == 0) sfx(2, SFX_BOOM);

    ent_update_all();
    ent_resolve_collisions();
    ent_draw_all();
    /* プロト: 撃破判定(ボスHP)未実装のため、一定時間で「撃破」とみなしエンディングへ。 */
    if (boss_t > 900) return SC_ENDING;
    return SCENE_NONE;
}
