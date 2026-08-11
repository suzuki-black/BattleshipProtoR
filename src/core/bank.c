/* bank.c — ASCII8 バンク切替の常駐実装。
   バンク選択は 0x7800 への書込のみ(他窓に書くと暴走)。
   g_bank / bcall のトランポリン本体は crt0rom.s(_bcall)側。 */
#include "bank.h"

u8 g_bank;   /* crt0 の _bcall が参照。bcall() 前に呼ぶバンク番号を入れる。 */

void bank_data(u8 n) {
    *(volatile u8 *)0x7800 = n;
}

void bank_restore(void) {
    *(volatile u8 *)0x7800 = BANK_DEFAULT;
}

void bcall_to(u8 bank) {
    g_bank = bank;
    bcall();
}
