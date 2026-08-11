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

/* ===== スプライト(V9938 mode2, 16x16) =====
   SCREEN5 の高位VRAMにテーブルを置く: 属性0xF780 / 色0xF580 / パターン0xF800。
   色はmode2では行ごと(色表16B/枚)。単色運用は vdp_sprite_color で全16行を塗る。 */
void vdp_sprite_init(void);                          /* 16x16化＋テーブル基底設定＋全消し */
void vdp_sprite_pattern(u8 patnum, const u8 *d32);   /* 16x16=32B をパターン patnum へ(patnumは4の倍数) */
void vdp_sprite_color(u8 slot, u8 color);            /* slot の色表16行を単色 color に */
void vdp_sprite_pos(u8 slot, u8 x, u8 y, u8 patnum); /* slot の属性(Y=y-1,X,pattern)を更新 */
void vdp_sprite_hide_from(u8 slot);                  /* slot に停止マーカ(Y=208)=以降非表示 */

#endif /* VDP_H */
