/* sys.c — システム起動(turboR 専用の初期化)。
   起動時に一度だけ R800(ROMモード)へ切替え、以降 Z80命令を高速実行する。
   MSX2+ 以下(version<3)では CHGCPU を呼ばず素通り(安全側)。 */
#include "sys.h"
#include "msx.h"

/* R800(ROMモード)へブースト。前作 boost_r800 を踏襲。 */
static void boost_r800(void) {
    __asm
        ld   a, (0x002D)     ; MSX_VER
        cp   #3
        jr   c, 00001$       ; version<3(turboR未満)なら何もしない
        ld   a, #0x81        ; CPU=R800(ROM) + 変更ビット
        call 0x0180          ; CHGCPU
    00001$:
    __endasm;
}

void sys_init(void) {
    boost_r800();
}
