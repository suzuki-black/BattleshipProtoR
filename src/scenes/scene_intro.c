/* scene_intro.c — ★新ルール第1段: 空戦イントロ(海のみ・縦スクロールのみ)。
   自機(下部固定)を相手に敵戦闘機が上から降ってくる導入部。約5秒で船首出現→戦艦ボス(SC_BOSS)へ。
   縦スクロールは R#23(スプライトにも効く)＝vdp が自機/敵のスプライトYを補正し画面固定に見せる。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "sprites.h"

static u16 intro_t;
static u8  scroll;
static u16 rng;

static u8 rnd(void) { rng = rng * 25173 + 13849; return (u8)(rng >> 8); }

/* 海: VRAM全体(256行)を青で埋め、16px毎に波線 → 縦スクロールで seamless に流れる。 */
static void draw_sea(void) {
    u8 y = 0;
    vdp_fill(0, 0, 256, 256, 4);
    for (;;) { vdp_fill(0, y, 256, 2, 5); if (y >= 240) break; y += 16; }
}

void intro_init(void) {
    Entity *e;
    draw_sea();
    vdp_sprite_init();
    sprites_load();
    ent_reset();
    intro_t = 0; scroll = 0; rng = 0x1234;
    vdp_set_vscroll(0);

    /* 自機(下部中央・固定)。今は静止マーカ(入力対応は後続ステップ)。 */
    e = ent_spawn(ET_SHOOTER);
    if (e) { e->x = 120; e->y = 176; e->color = 15; e->pat = SPR_BLOCK; e->fire = (const u8 *)0; }
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
        }
        sfx(1, SFX_HIT);
    }

    ent_update_all();
    ent_draw_all();

    /* 約5秒で船首が見えてくる → 戦艦ボスへ遷移 */
    if (intro_t > 300) return SC_BOSS;
    return SCENE_NONE;
}
