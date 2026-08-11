/* scene_boot.c — 「Hello VDP」= 土台の疎通確認シーン。
   SCREEN5 に切替え、VDPコマンド(HMMV)で全画面塗り＋矩形2枚を描く。
   これが画面に出れば: crt0起動 / page2有効化 / ASCII8窓 / シーンFSM / VDPアクセス が
   すべて生きていることの実機証明になる。中身は将来 title シーンへ差し替える。 */
#include "scene.h"
#include "vdp.h"
#include "input.h"

static u8 boot_t;   /* 経過フレーム(遷移タイマ) */

void boot_init(void) {
    vdp_screen5();

    /* 見やすいパレットを明示設定(既定に依存しない) */
    vdp_set_pal(0, 0, 0, 0);   /* 背景=黒        */
    vdp_set_pal(4, 1, 2, 6);   /* 海っぽい青      */
    vdp_set_pal(8, 6, 1, 1);   /* 赤              */
    vdp_set_pal(15, 7, 7, 7);  /* 白              */
    vdp_wreg(7, 0x04);         /* ボーダー色=青(4) (SCREEN5: R#7 低位ニブル=ボーダー) */

    /* 全画面(256x212)を青、中央に白枠、その中に赤 = 3コマンドで疎通確認 */
    vdp_fill(0,   0,   256, 212, 4);
    vdp_fill(48,  56,  160, 100, 15);
    vdp_fill(56,  64,  144, 84,  8);

    boot_t = 0;
}

u8 boot_update(void) {
    /* 疎通画面を約1.5秒見せる or トリガで、エンティティ骨格デモ(SC_DEMO)へ遷移。 */
    boot_t++;
    if (boot_t > 90 || (g_input_edge & INP_TRIG)) return SC_DEMO;
    return SCENE_NONE;
}
