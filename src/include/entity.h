/* entity.h — 汎用エンティティ・プール(常駐/ホット)。
   自機/敵機/弾/砲/エフェクトを1つの固定長プール＋type別 behavior で回す骨格。
   空戦の敵機も戦艦の砲も同じ枠で扱う(HANDOFF §4-3)。将来 run_ops/run_fire を behavior に接続。 */
#ifndef ENTITY_H
#define ENTITY_H

#include "types.h"

#define ENT_MAX 16   /* turboR前提で余裕。弾幕化時に拡張 */

/* 種別 = behavior テーブルの添字。追加時は entity.c の behaviors[] と対で更新。 */
enum {
    ET_NONE = 0,
    ET_BOUNCER,   /* 骨格デモ用: 画面端で反射する矩形 */
    ET_COUNT
};

typedef struct Entity {
    u8  active;
    u8  type;
    s16 x, y;     /* 位置(px, 左上) */
    s16 vx, vy;   /* 速度(px/frame) */
    u8  w, h;     /* 当たり/反射に使うサイズ(スプライトは16x16固定) */
    u8  color;    /* スプライト色(0-15) */
    u8  pat;      /* スプライトパターン番号(4の倍数) */
} Entity;

void    ent_reset(void);        /* プール全消去 */
Entity *ent_spawn(u8 type);     /* 空きを1つ確保(既定値で初期化)。無ければ NULL */
void    ent_update_all(void);   /* 全 active の behavior update を回す */
void    ent_draw_all(void);     /* 消去→描画の2パス(相互消去を防ぐ) */

#endif /* ENTITY_H */
