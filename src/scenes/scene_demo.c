/* scene_demo.c — エンティティ骨格＋音ドライバのデモシーン。
   複数の bouncer を毎フレーム update→draw。加えて定期的に SFX を鳴らし、
   H.TIMI ISR の稼働を画面で可視化する:
     ・上段の白バー幅 = snd_ticks & 0x7F  → ISRが毎フレーム進んでいる証跡(伸縮する)
     ・右上の四角     = 発音中=赤 / 無音=青 → ISRがエンベロープを減衰させている証跡(鳴らすと赤→自然に青へ) */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sound.h"
#include "bank.h"

#define BANK_PROOF (*(volatile u8 *)0xE000)   /* bank_demo が 0x5A を書く証跡 */

static u16 demo_t;
static u8  bank_ok;   /* 実バンクコール成功フラグ */

void demo_init(void) {
    Entity *e;
    vdp_fill(0, 0, 256, 212, 4);   /* 背景を海青で全消し */
    ent_reset();
    demo_t = 0;

    /* 実バンクコール実証: bank4 の bank_entry を呼び、RAM証跡(0xE000==0x5A)を確認 */
    BANK_PROOF = 0x00;
    bcall_to(4);
    bank_ok = (BANK_PROOF == 0x5A);

    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 20;  e->y = 30;  e->vx = 3;  e->vy = 2;  e->w = 12; e->h = 12; e->color = 15; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 120; e->y = 50;  e->vx = -2; e->vy = 3;  e->w = 10; e->h = 10; e->color = 8;  }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 80;  e->y = 150; e->vx = 2;  e->vy = -2; e->w = 16; e->h = 8;  e->color = 11; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 200; e->y = 100; e->vx = -3; e->vy = -1; e->w = 8;  e->h = 16; e->color = 6;  }
}

/* ISR可視化HUD(上段)。バー=tick、右上四角=発音中フラグ。 */
static void draw_hud(void) {
    u8 w = (u8)(snd_ticks & 0x7F);          /* 0..127 で伸縮 */
    vdp_fill(0, 2, 128, 4, 4);              /* バー領域を消去(青) */
    if (w) vdp_fill(0, 2, w, 4, 15);        /* tickバー(白) */
    vdp_fill(244, 2, 8, 8, snd_active ? 8 : 4);  /* 発音中=赤 / 無音=青 */
    vdp_fill(232, 2, 8, 8, bank_ok ? 12 : 8);    /* 実バンクコール: 成功=緑 / 失敗=赤 */
}

u8 demo_update(void) {
    ent_update_all();
    ent_draw_all();

    /* 定期的に効果音をトリガ(ISRが減衰・停止を担う) */
    demo_t++;
    if ((demo_t % 48) == 0)  sfx(0, SFX_SHOT);
    if ((demo_t % 96) == 0)  sfx(1, SFX_HIT);
    if ((demo_t % 192) == 0) sfx(2, SFX_BOOM);

    draw_hud();
    return SCENE_NONE;
}
