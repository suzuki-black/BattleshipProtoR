/* scene_demo.c — 骨格デモ: エンティティ(=ハードウェアスプライト)＋音ドライバ＋バンクコール。
   bouncer×4 を毎フレーム update→スプライト描画。加えて:
     ・上段の白バー幅 = snd_ticks & 0x7F  → H.TIMI ISRが毎フレーム進む証跡
     ・右上の四角(244) = 発音中=赤/無音=青  → ISRのエンベロープ減衰の証跡
     ・右上の四角(232) = bcall成功=緑/失敗=赤 → 実バンクコールの証跡 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "bank.h"

#define BANK_PROOF (*(volatile u8 *)0xE000)

/* 16x16 中実ブロック(mode2: 前半16B=左列 rows0-15 / 後半16B=右列 rows0-15) */
static const u8 box16[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

static u16 demo_t;
static u8  bank_ok;

void demo_init(void) {
    Entity *e;
    vdp_fill(0, 0, 256, 212, 4);   /* 背景を海青で全消し */
    ent_reset();
    demo_t = 0;

    /* スプライト初期化＋パターン投入 */
    vdp_sprite_init();
    vdp_sprite_pattern(0, box16);

    /* 実バンクコール実証 */
    BANK_PROOF = 0x00;
    bcall_to(4);
    bank_ok = (BANK_PROOF == 0x5A);

    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 20;  e->y = 30;  e->vx = 3;  e->vy = 2;  e->color = 15; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 120; e->y = 50;  e->vx = -2; e->vy = 3;  e->color = 8;  }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 80;  e->y = 150; e->vx = 2;  e->vy = -2; e->color = 11; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 200; e->y = 100; e->vx = -3; e->vy = -1; e->color = 6;  }
}

/* ISR/音/bcall 可視化HUD(上段, LMMV矩形) */
static void draw_hud(void) {
    u8 w = (u8)(snd_ticks & 0x7F);
    vdp_fill(0, 2, 128, 4, 4);
    if (w) vdp_fill(0, 2, w, 4, 15);
    vdp_fill(244, 2, 8, 8, snd_active ? 8 : 4);
    vdp_fill(232, 2, 8, 8, bank_ok ? 12 : 8);
}

u8 demo_update(void) {
    ent_update_all();
    ent_draw_all();

    demo_t++;
    if ((demo_t % 48) == 0)  sfx(0, SFX_SHOT);
    if ((demo_t % 96) == 0)  sfx(1, SFX_HIT);
    if ((demo_t % 192) == 0) sfx(2, SFX_BOOM);

    draw_hud();
    return SCENE_NONE;
}
