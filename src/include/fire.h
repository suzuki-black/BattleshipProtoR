/* fire.h — 発砲プリミティブ emit と発砲スクリプト run_fire(データ駆動)。
   HANDOFF §4-2 の run_fire を骨格化。難易度メカ(予告/レイジ/抑え込み等)は将来ここへ足す。

   FireDesc(バイト列): { interval, [op, a, kind, spd] x N, 0 }
     interval : 発火間隔(フレーム)
     op       : 発砲オペコード(下記)
     a        : opの引数(FIXED=方向0-15 / RING=弾数)
     kind     : 弾種(色/パターンの選択に使用)
     spd      : 弾速(方向テーブル基準の 1/8 スケール)
     終端      : op=0 で1レコード終わり
   方向 dir(0-15)=16分割。0=上、時計回り。 */
#ifndef FIRE_H
#define FIRE_H

#include "types.h"
#include "entity.h"

/* 発砲オペコード */
#define FIRE_END   0   /* レコード終端 */
#define FIRE_FIXED 1   /* a=固定方向(0-15)へ1発 */
#define FIRE_RING  2   /* a=弾数の全方位リング */
/* 将来: FIRE_AIMED(自機狙い), FIRE_AIMOFF(狙い±オフセット) 等 */

/* 1発生成。dir(0-15)/spd から速度を決め ET_BULLET を1つ spawn。戻り値は実体(満杯なら NULL)。 */
Entity *emit(s16 x, s16 y, u8 dir, u8 kind, u8 spd);

/* 射手 e の FireDesc(e->fire)を進め、interval 毎に発砲オペを実行する。 */
void run_fire(Entity *e);

#endif /* FIRE_H */
