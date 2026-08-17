/* scene_ending.c — ★エンディング(冷たいシーン=bank7)。静かなED曲(track2, scene_bgm が再生)＋
   スタッフロール(クレジットのページ送り)。各ページを数秒表示→トリガ or 時間で次へ、末尾でタイトルへ。
   ※banked scene: 常駐 vdp_text/vdp_fill を注入番地で呼ぶ。データ窓(bank_data)は使わない。 */
#include "vdp.h"
#include "input.h"
#include "scene.h"

#define PAGE_HOLD 210   /* 各ページ表示フレーム(約3.5秒)。トリガで即次へ */
#define NPAGE 4

static u16 et;    /* 現ページの経過フレーム。RAM(data-loc)。init で0 */
static u8  page;  /* 現在のクレジットページ */

static void draw_page(u8 p) {
    vdp_fill(0, 0, 256, 212, 1);
    if (p == 0) {
        vdp_text(64,  48, 11, 1, "MISSION COMPLETE");
        vdp_text(24,  96, 15, 1, "THE SEA IS CALM AGAIN.");
        vdp_text(32, 116, 15, 1, "THE FLEET SAILS HOME.");
    } else if (p == 1) {
        vdp_text(112, 28, 11, 1, "STAFF");
        vdp_text(48,  68, 14, 1, "GAME DESIGN");
        vdp_text(64,  84, 15, 1, "SUZUKI");
        vdp_text(48, 108, 14, 1, "PROGRAM");
        vdp_text(64, 124, 15, 1, "SUZUKI");
        vdp_text(48, 148, 14, 1, "MUSIC");
        vdp_text(64, 164, 15, 1, "SUZUKI");
    } else if (p == 2) {
        vdp_text(80,  40, 11, 1, "SPECIAL THANKS");
        vdp_text(48,  92, 15, 1, "MSX COMMUNITY");
        vdp_text(40, 124, 15, 1, "AND YOU, PILOT.");
    } else {
        vdp_text(96, 92, 8, 1, "THE END");
    }
}

static void ending_init(void) {
    vdp_set_display_page(0);
    page = 0; et = 0;
    draw_page(0);
}

static u8 ending_update(void) {
    et++;
    if ((g_input_edge & INP_TRIG) || et > PAGE_HOLD) {
        et = 0;
        if (++page >= NPAGE) return SC_TITLE;   /* 末尾でタイトルへ */
        draw_page(page);
    }
    return SCENE_NONE;
}

void banked_entry(void) {
    if (g_scene_phase == 0) ending_init();
    else g_scene_ret = ending_update();
}
