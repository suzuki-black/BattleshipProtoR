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

/* ===== スプライト(mode2, 16x16) =====
   SCREEN5 の BIOS 既定テーブル配置を使う(C-BIOS/実機共通): 属性0x7600/色0x7400/パターン0x7800。
   R#5/6/11 は CHGMOD(5) が既定値に設定済みなので触らない(相対再配置は環境差で不確実)。 */
#define SPR_ATTR  0x7600   /* 属性表(4B/枚: Y,X,pattern,予約) */
#define SPR_COLOR 0x7400   /* 色表(16B/枚: 行ごとの色)         */
#define SPR_PAT   0x7800   /* パターン生成表(8B単位)           */

/* R#1 の size ビットを立て 16x16 に(mag=0)。RG1SAV(0xF3E0)経由で他ビット保持。 */
static void set_sprite16(void) {
    __asm
        di
        ld   a, (0xF3E0)
        or   #0x02          ; size=16x16
        and  #0xFE          ; mag=0
        ld   (0xF3E0), a
        out  (0x99), a
        ld   a, #0x81       ; R#1
        out  (0x99), a
        ei
    __endasm;
}

void vdp_sprite_init(void) {
    u8 i;
    set_sprite16();
    for (i = 0; i < 32; i++) {             /* 全スプライトを画面外へ */
        vdp_write_addr(SPR_ATTR + i * 4);
        VDP_DAT = 216;                     /* Y=216(画面下=不可視) */
    }
}

void vdp_sprite_pattern(u8 patnum, const u8 *d32) {
    u8 i;
    vdp_write_addr(SPR_PAT + (u16)patnum * 8);
    for (i = 0; i < 32; i++) VDP_DAT = d32[i];
}

void vdp_sprite_color(u8 slot, u8 color) {
    u8 i;
    vdp_write_addr(SPR_COLOR + (u16)slot * 16);
    for (i = 0; i < 16; i++) VDP_DAT = color;
}

void vdp_sprite_pos(u8 slot, u8 x, u8 y, u8 patnum) {
    vdp_write_addr(SPR_ATTR + (u16)slot * 4);
    VDP_DAT = (u8)(y - 1);   /* 表示Y=属性Y+1 のため -1 */
    VDP_DAT = x;
    VDP_DAT = patnum;
    VDP_DAT = 0;
}

void vdp_sprite_hide_from(u8 slot) {
    vdp_write_addr(SPR_ATTR + (u16)slot * 4);
    VDP_DAT = 208;           /* Y=208 = 以降のスプライト処理を停止 */
}
