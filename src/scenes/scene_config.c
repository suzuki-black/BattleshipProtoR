/* scene_config.c — ★設定メニュー(冷たいシーン=bank6)。バンク側から常駐 vdp_text 等を呼ぶ。
   UP/DOWN でカーソル移動、L/R で値変更、SPACE で START→ゲーム開始(SC_STAGE)。
   選択は常駐 g_difficulty/g_lives_idx に書き、gameplay が参照する。 */
#include "vdp.h"
#include "input.h"
#include "scene.h"
#include "gamestate.h"

static u8 cur;   /* 0=難易度 / 1=残機 / 2=START。RAM(data-loc)。init で初期化。 */

static const char *const diffs[3]  = { "EASY  ", "NORMAL", "HARD  " };
static const char *const livess[3] = { "2", "3", "5" };

static void row(u8 y, u8 idx, const char *label, const char *val) {
    vdp_text(48, y, (cur == idx) ? 11 : 14, 1, (cur == idx) ? ">" : " ");
    vdp_text(64, y, (cur == idx) ? 15 : 14, 1, label);
    if (val) vdp_text(176, y, 15, 1, val);
}

static void draw(void) {
    vdp_fill(0, 0, 256, 212, 1);
    vdp_text(88, 24, 15, 1, "- CONFIG -");
    row(72,  0, "DIFFICULTY", diffs[g_difficulty]);
    row(96,  1, "LIVES",      livess[g_lives_idx]);
    row(132, 2, "START GAME", (const char *)0);
    vdp_text(24, 190, 14, 1, "UP/DN:SEL  L/R:CHG  SPACE:OK");
}

static void config_init(void) {
    vdp_set_display_page(0);
    cur = 0;
    draw();
}

static u8 config_update(void) {
    u8 e = g_input_edge;
    if (e & INP_DOWN) { if (cur < 2) cur++; }
    if (e & INP_UP)   { if (cur > 0) cur--; }
    if (cur == 0) {
        if ((e & INP_RIGHT) && g_difficulty < 2) g_difficulty++;
        if ((e & INP_LEFT)  && g_difficulty > 0) g_difficulty--;
    } else if (cur == 1) {
        if ((e & INP_RIGHT) && g_lives_idx < 2) g_lives_idx++;
        if ((e & INP_LEFT)  && g_lives_idx > 0) g_lives_idx--;
    }
    if ((e & INP_TRIG) && cur == 2) return SC_STAGE;   /* START でステージ開始 */
    if (e) draw();                                     /* 入力があった時だけ再描画 */
    return SCENE_NONE;
}

void banked_entry(void) {
    if (g_scene_phase == 0) config_init();
    else g_scene_ret = config_update();
}
