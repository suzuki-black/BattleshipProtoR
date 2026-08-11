/* scene_demo.c — エンティティ骨格のデモシーン。
   複数の bouncer を別速度で放ち、毎フレーム update→draw。プール/behavior/FSM/描画の疎通確認。
   将来ここが空戦イントロ/戦艦ボスへ発展する(実体= 自機/敵機/弾/砲)。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"

void demo_init(void) {
    Entity *e;
    vdp_fill(0, 0, 256, 212, 4);   /* 背景を海青で全消し */
    ent_reset();

    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 20;  e->y = 20;  e->vx = 3;  e->vy = 2;  e->w = 12; e->h = 12; e->color = 15; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 120; e->y = 40;  e->vx = -2; e->vy = 3;  e->w = 10; e->h = 10; e->color = 8;  }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 80;  e->y = 150; e->vx = 2;  e->vy = -2; e->w = 16; e->h = 8;  e->color = 11; }
    e = ent_spawn(ET_BOUNCER); if (e) { e->x = 200; e->y = 100; e->vx = -3; e->vy = -1; e->w = 8;  e->h = 16; e->color = 6;  }
}

u8 demo_update(void) {
    ent_update_all();
    ent_draw_all();
    return SCENE_NONE;
}
