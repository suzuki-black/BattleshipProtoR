/* scene_intro.c — ★新ルール第1段: 空戦イントロ(海のみ・縦スクロールのみ)。
   自機(下部固定)を相手に敵戦闘機が上から降ってくる導入部。約5秒で船首出現→戦艦ボス(SC_BOSS)へ。
   縦スクロールは R#23(スプライトにも効く)＝vdp が自機/敵のスプライトYを補正し画面固定に見せる。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "sprites.h"
#include "fire.h"

/* 敵戦闘機の発砲: 45f毎に自機狙い＋散らし円錐(±3ステップ≒±34°)。狙いすぎない。 */
static const u8 fd_faim[] = { 45, FIRE_AIMED, 3, 1, 2, FIRE_END };

static u16 intro_t;
static u8  scroll;
static u16 rng;

static u8 rnd(void) { rng = rng * 25173 + 13849; return (u8)(rng >> 8); }

/* 海を page1(VRAM 0x8000+, DY=256+)へ描く。全256行を青＋16px毎に波線 → 縦スクロールで seamless。
   スプライトテーブルは page0 末尾に居るため、page1 表示中はスクロールしても可視域に出ない(ゴミ防止)。 */
#define P1 256   /* page1 の DY オフセット */
static void draw_sea(void) {
    u16 y = 0;
    vdp_fill(0, P1, 256, 256, 4);
    for (;;) { vdp_fill(0, P1 + y, 256, 2, 5); if (y >= 240) break; y += 16; }
}

static u8 last_kills;

void intro_init(void) {
    Entity *e;
    draw_sea();
    vdp_set_display_page(1);   /* page1(海)を表示 */
    vdp_sprite_init();
    sprites_load();
    ent_reset();
    intro_t = 0; scroll = 0; rng = 0x1234;
    g_kills = 0; g_playerhit = 0; last_kills = 0;
    vdp_set_vscroll(0);

    /* 自機(下部中央・入力操作) */
    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 176; e->color = 15; e->pat = SPR_BLOCK; }
}

u8 intro_update(void) {
    Entity *e;

    /* 縦スクロール(世界が下へ流れる=前進) */
    scroll = (u8)(scroll - 1);
    vdp_set_vscroll(scroll);

    /* 敵戦闘機を定期スポーン(上から侵入) */
    intro_t++;
    if ((intro_t % 32) == 0) {
        e = ent_spawn(ET_FIGHTER);
        if (e) {
            e->x = 24 + (rnd() % 200);
            e->y = -16;
            e->vx = (rnd() & 1) ? 1 : -1;
            e->vy = 2 + (rnd() % 2);
            e->color = 8; e->pat = SPR_FIGHTER;
            /* 約半数は自機狙い(散らし付き)で撃ってくる */
            if (rnd() & 1) { e->fire = fd_faim; e->ftimer = 20 + (rnd() % 30); }
        }
        sfx(1, SFX_HIT);
    }

    ent_update_all();
    ent_resolve_collisions();
    if (g_kills != last_kills) { sfx(2, SFX_BOOM); last_kills = g_kills; }   /* 撃破音 */
    ent_draw_all();

    /* 約5秒で船首が見えてくる → 戦艦ボスへ遷移 */
    if (intro_t > 300) return SC_BOSS;
    return SCENE_NONE;
}
