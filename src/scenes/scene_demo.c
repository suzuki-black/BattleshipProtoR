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
#include "fire.h"
#include "ops.h"

#define BANK_PROOF (*(volatile u8 *)0xE000)

/* 16x16 中実ブロック(mode2: 前半16B=左列 rows0-15 / 後半16B=右列 rows0-15) */
static const u8 box16[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

/* 16x16 の中央 6x6 弾(cols5-10, rows5-10)。左列=cols5-7(0x07)/右列=cols8-10(0xE0) */
static const u8 bullet16[32] = {
    0x00,0x00,0x00,0x00,0x00,0x07,0x07,0x07,0x07,0x07,0x07,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0x00,0x00,0x00,0x00,0x00
};

/* 発砲スクリプト: 40フレーム毎に 8方向リング(kind0, 弾速3) */
static const u8 fd_ring[] = { 40, FIRE_RING, 8, 0, 3, FIRE_END };

/* データ駆動の艦体(run_ops): 船体(灰)＋艦橋(シアン)＋主砲(赤) */
static const u8 ship_ops[] = {
    OPS_RECT,  0, 10, 56, 12, 14,   /* 船体 */
    OPS_RECT, 10,  4, 14, 10,  7,   /* 艦橋 */
    OPS_RECT, 24,  0,  8,  8,  8,   /* 主砲 */
    OPS_END
};

static u16 demo_t;
static u8  bank_ok;

void demo_init(void) {
    Entity *e;
    vdp_fill(0, 0, 256, 212, 4);   /* 背景を海青で全消し */
    run_ops(100, 24, ship_ops);    /* データ駆動の艦体を背景へ描画 */
    ent_reset();
    demo_t = 0;

    /* スプライト初期化＋パターン投入(0=ブロック, 4=弾) */
    vdp_sprite_init();
    vdp_sprite_pattern(0, box16);
    vdp_sprite_pattern(4, bullet16);

    /* 実バンクコール実証 */
    BANK_PROOF = 0x00;
    bcall_to(4);
    bank_ok = (BANK_PROOF == 0x5A);

    /* 反射体×2 */
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 20;  e->y = 30;  e->vx = 3;  e->vy = 2;  e->color = 15; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 210; e->y = 160; e->vx = -2; e->vy = -3; e->color = 11; }

    /* 中央の射手(データ駆動 run_fire でリング弾) */
    e = ent_spawn(ET_SHOOTER); if (e) { e->x = 120; e->y = 96; e->color = 8; e->fire = fd_ring; e->ftimer = 20; }
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
