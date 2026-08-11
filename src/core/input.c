/* input.c — 入力の常駐実装。row8(カーソル/スペース)を直読みして押下ビットへ整形。
   前作 poll_stick を踏襲(bit4=L,5=U,6=D,7=R が押下で0。スペースは row8 bit0)。 */
#include "input.h"

u8 g_input;
u8 g_input_edge;

static u8 g_raw;   /* row8 の生値(負論理) */

static void read_row8(void) __naked {
    __asm
        di
        in   a, (0xAA)
        and  #0xF0
        or   #8              ; row 8 を選択
        out  (0xAA), a
        in   a, (0xA9)
        ei
        ld   (_g_raw), a
        ret
    __endasm;
}

void input_poll(void) {
    u8 prev = g_input;
    u8 cur = 0;
    read_row8();
    /* row8: bit7=R,6=D,5=U,4=L,0=SPACE (押下で0)。押下=1 のビットへ整形。 */
    if (!(g_raw & 0x80)) cur |= INP_RIGHT;
    if (!(g_raw & 0x40)) cur |= INP_DOWN;
    if (!(g_raw & 0x20)) cur |= INP_UP;
    if (!(g_raw & 0x10)) cur |= INP_LEFT;
    if (!(g_raw & 0x01)) cur |= INP_TRIG;
    g_input = cur;
    g_input_edge = (u8)(cur & ~prev);   /* 押した瞬間 */
}
