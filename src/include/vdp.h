/* vdp.h — VDP(V9958)アクセスの常駐API。
   実装(vdp.c)は前作 BattleshipProto で実機確定した MSXgl 流イディオム
   (0x99 の2バイト書込を各自 di/ei で原子化)を踏襲。
   ※ホットパス(毎フレーム描画)から呼ぶ想定。常駐(bank0-2)に置く。 */
#ifndef VDP_H
#define VDP_H

#include "types.h"

/* VDP レジスタ R#r へ v を書く(di/ei 原子化)。 */
void vdp_wreg(u8 r, u8 v);

/* パレット registers: 色 idx を (r,g,b) 各0-7 に設定。 */
void vdp_set_pal(u8 idx, u8 r, u8 g, u8 b);

/* VRAM 書込アドレスを a に設定(以降 vdp_data() で連続書込)。 */
void vdp_write_addr(u16 a);

/* VRAM データポートへ1バイト(vdp_write_addr 後に使う)。 */
void vdp_data(u8 v);

/* SCREEN5(GRAPHIC4, 256x212 16色)へ切替(BIOS CHGMOD)。 */
void vdp_screen5(void);

/* VDPコマンド完了待ち(CE ポーリング)。 */
void vdp_cmd_wait(void);

/* 矩形塗り(HMMV): (dx,dy) から (nx,ny) を色 color で塗る。 */
void vdp_fill(u16 dx, u16 dy, u16 nx, u16 ny, u8 color);

/* 表示同期: VBLANK(JIFFY 更新)を1回待つ。 */
void vdp_wait_frame(void);

#endif /* VDP_H */
