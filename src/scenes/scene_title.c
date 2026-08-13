/* scene_title.c — ★冷たいシーンを「バンク化」した最初の実例(bank5)。
   常駐(bank0-2, 24KB)を食わずに ROM バンクへ置き、bcall で実行する。
   規律:
     - リンクは 0xA000(bankhead が先頭 jp _banked_entry を確定)。self-contained。
     - 常駐関数/global(vdp_fill/run_ops/g_input_edge/g_scene_* 等)は resident_syms(絶対番地)で解決 →
       ヘッダを include して普通に呼べる。ただし**データ窓(bank_data)は使わない**(自分の bank が窓に居るため)。
     - 自前の const(title_ops)は自バンク(0xA000+)に載り、窓が自分の間だけ読める(実行中は常にそう)。
   banked_entry が g_scene_phase(0=init/1=update)で分岐し、update 結果を g_scene_ret に書く。 */
#include "vdp.h"
#include "ops.h"
#include "input.h"
#include "scene.h"

/* タイトル絵(データ駆動): 中央の戦艦シルエット＋下部スタートバー。 */
static const u8 title_ops[] = {
    OPS_RECT, 100,  40, 56, 120, 14,   /* 船体(灰)   */
    OPS_RECT, 108,  56, 40,  26,  7,   /* 艦橋(シアン) */
    OPS_RECT, 116,  44, 24,   8,  8,   /* 艦首砲(赤) */
    OPS_RECT, 116, 150, 24,   8,  8,   /* 艦尾砲(赤) */
    OPS_RECT,  80, 182, 96,  10, 15,   /* スタートバー(白) */
    OPS_END
};

static u16 tt;   /* 経過フレーム(アトラクト自動遷移用)。RAM(data-loc)。init で0クリア。 */

static void title_init(void) {
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 212, 1);   /* 黒背景 */
    run_ops(0, 0, title_ops);
    vdp_text(72, 16, 15, 1, "BATTLESHIP PROTO R");
    vdp_text(88, 196, 14, 1, "PUSH SPACE");
    tt = 0;
}

static u8 title_update(void) {
    tt++;
    if ((g_input_edge & INP_TRIG) || tt > 600) return SC_CONFIG;   /* トリガ or 10秒で設定へ */
    return SCENE_NONE;
}

/* バンク単一エントリ(bankhead の jp 先)。g_scene_phase で init/update を分岐。 */
void banked_entry(void) {
    if (g_scene_phase == 0) title_init();
    else g_scene_ret = title_update();
}
