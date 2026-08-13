/* scene_ending.c — ★エンディング(冷たいシーン=bank7)。静かな一枚絵＋英文＋THE END。
   トリガ or 一定時間でタイトルへ戻る。将来ここへスタッフロール/静かなBGMを載せる。 */
#include "vdp.h"
#include "input.h"
#include "scene.h"

static u16 et;   /* 経過フレーム。RAM(data-loc)。init で0。 */

static void ending_init(void) {
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 212, 1);
    vdp_text(80,  40, 14, 1, "MISSION COMPLETE");
    vdp_text(32, 100, 15, 1, "THE SEA IS CALM AGAIN.");
    vdp_text(40, 120, 15, 1, "THE FLEET SAILS HOME.");
    vdp_text(104, 168, 8, 1, "THE END");
    et = 0;
}

static u8 ending_update(void) {
    et++;
    if ((g_input_edge & INP_TRIG) || et > 600) return SC_TITLE;   /* トリガ or 10秒でタイトルへ */
    return SCENE_NONE;
}

void banked_entry(void) {
    if (g_scene_phase == 0) ending_init();
    else g_scene_ret = ending_update();
}
