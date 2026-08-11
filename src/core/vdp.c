/* vdp.c — VDP(V9958)アクセスの常駐実装。
   前作 BattleshipProto(実機確定)の MSXgl 流イディオムを踏襲:
     0x99 への2バイト書込は割込みで壊れるので各自 di/ei で原子化する。 */
#include "vdp.h"
#include "msx.h"

__sfr __at(0x98) VDP_DAT;    /* VRAM データ                          */
__sfr __at(0x99) VDP_CTRL;   /* アドレス/レジスタ                    */
__sfr __at(0x9A) VDP_PAL;    /* パレットデータ                       */
__sfr __at(0x9B) VDP_IDAT;   /* 間接レジスタ(R#17 オートインクリメント) */

void vdp_wreg(u8 r, u8 v) {
    __asm di __endasm;
    VDP_CTRL = v;
    VDP_CTRL = 0x80 | r;
    __asm ei __endasm;
}

void vdp_set_pal(u8 idx, u8 r, u8 g, u8 b) {
    __asm di __endasm;
    VDP_CTRL = idx;
    VDP_CTRL = 0x80 | 16;         /* R#16 = パレットポインタ */
    VDP_PAL  = (r << 4) | b;
    VDP_PAL  = g;
    __asm ei __endasm;
}

/* R#14(アドレス上位)〜アドレス下位/上位ラッチ確立までを1つの di/ei に閉じて原子化。 */
void vdp_write_addr(u16 a) {
    __asm di __endasm;
    VDP_CTRL = (a >> 14) & 7;   VDP_CTRL = 0x80 | 14;
    VDP_CTRL = a & 0xFF;        VDP_CTRL = ((a >> 8) & 0x3F) | 0x40;
    __asm ei __endasm;
}

void vdp_data(u8 v) {
    VDP_DAT = v;
}

/* SCREEN5(GRAPHIC4)へ。BIOS ワーク(SCRMOD)を 5 にして CHGMOD。 */
void vdp_screen5(void) {
    __asm
        ld   a, #5
        ld   (0xFCAF), a       ; SCRMOD_W
        call 0x005F            ; CHGMOD
    __endasm;
}

/* VDPコマンド完了待ち。CE(S#2 bit0)=1 の間ループ。
   重要: S#2 選択中に VBLANK 割込みが入ると、MSX の ISR が S#0(割込フラグ)でなく S#2 を
   読んでフラグを消せず→割込が消えず永久再突入でハングする。よって各ポーリングを di で囲み、
   必ず S#0 へ戻してから ei する(S#2 選択窓を最小化しつつ割込は基本 on に保つ)。 */
void vdp_cmd_wait(void) {
    __asm
    00001$:
        di
        ld   a, #2
        out  (0x99), a
        ld   a, #0x8F         ; R#15 = 2 (S#2 選択)
        out  (0x99), a
        in   a, (0x99)
        rra                    ; CE -> Carry
        ld   a, #0
        out  (0x99), a
        ld   a, #0x8F          ; R#15 = 0 (S#0 へ戻す)
        out  (0x99), a
        ei
        jp   c, 00001$
    __endasm;
}

/* LMMV(論理矩形塗り): (dx,dy)から(nx,ny)pxを色 color で塗る。座標・色ともピクセル単位。
   ※HMMV(0xC0)はバイト単位のため G4(2px/byte)では縞になる。塗りは LMMV(0x80)を使う。
   R#36..R#46 を個別書込(前作の実績イディオム)。前コマンド完了を待ってから発行。 */
void vdp_fill(u16 dx, u16 dy, u16 nx, u16 ny, u8 color) {
    vdp_cmd_wait();
    vdp_wreg(36, dx & 0xFF);  vdp_wreg(37, (dx >> 8) & 0x01);  /* DX (9bit) */
    vdp_wreg(38, dy & 0xFF);  vdp_wreg(39, (dy >> 8) & 0x03);  /* DY (10bit) */
    vdp_wreg(40, nx & 0xFF);  vdp_wreg(41, (nx >> 8) & 0x01);  /* NX (9bit) */
    vdp_wreg(42, ny & 0xFF);  vdp_wreg(43, (ny >> 8) & 0x03);  /* NY (10bit) */
    vdp_wreg(44, color);                                       /* CLR (色0-15) */
    vdp_wreg(45, 0);                                           /* ARG (DIX=DIY=0) */
    vdp_wreg(46, 0x80);                                        /* CMD = LMMV(論理IMP) */
}

void vdp_wait_frame(void) {
    volatile u16 *j = (volatile u16 *)0xFC9E;   /* JIFFY */
    u16 t = *j;
    while (*j == t) { }
}
