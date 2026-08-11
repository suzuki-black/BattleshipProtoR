/* input.h — 入力(カーソル/ジョイ方向・トリガ)の常駐API。
   前作の row8 直読みイディオムを踏襲。将来ジョイスティックポートへ拡張予定。 */
#ifndef INPUT_H
#define INPUT_H

#include "types.h"

/* 押下ビット(1=押下)。row8 の負論理を反転して提供する。 */
#define INP_RIGHT 0x01
#define INP_DOWN  0x02
#define INP_UP    0x04
#define INP_LEFT  0x08
#define INP_TRIG  0x10   /* スペース(トリガ) */

extern u8 g_input;       /* 今フレームの押下状態     */
extern u8 g_input_edge;  /* 今フレーム“押した瞬間”   */

/* 毎フレーム1回呼ぶ。g_input / g_input_edge を更新。 */
void input_poll(void);

#endif /* INPUT_H */
