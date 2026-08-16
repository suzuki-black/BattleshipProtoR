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

/* 隠しコマンド(コナミ): 上上下下左右左右 B A。成立で設定メニュー(SC_CONFIG)を開く。
   B=INP_TRIGB(ジョイ トリガ2 / キーM), A=INP_TRIG(スペース / ジョイ トリガ1)。 */
static const u8 konami[10] = {
    INP_UP, INP_UP, INP_DOWN, INP_DOWN, INP_LEFT, INP_RIGHT, INP_LEFT, INP_RIGHT, INP_TRIGB, INP_TRIG
};
static u8 kidx;   /* コナミ入力の進捗。RAM(data-loc)。init で0。 */

static void title_init(void) {
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 212, 1);   /* 黒背景 */
    run_ops(0, 0, title_ops);
    vdp_text(72, 16, 15, 1, "BATTLESHIP PROTO R");
    vdp_text(88, 196, 14, 1, "PUSH SPACE");
    kidx = 0;
}

static u8 title_update(void) {
    u8 e = g_input_edge;
    if (e) {
        /* コナミ進捗: 期待キーが押下エッジに含まれれば前進、外れたらリセット
           (押したのが先頭キー=UP ならそこから再開)。成立でコンフィグへ。 */
        if (e & konami[kidx]) {
            if (++kidx >= 10) { kidx = 0; return SC_CONFIG; }
        } else {
            kidx = (e & INP_UP) ? 1 : 0;
        }
    }
    /* 通常トリガ(A)でゲーム開始。※コナミ成立時は上で return 済み(こちらへ来ない)。 */
    if (e & INP_TRIG) return SC_STAGE;
    return SCENE_NONE;
}

/* バンク単一エントリ(bankhead の jp 先)。g_scene_phase で init/update を分岐。 */
void banked_entry(void) {
    if (g_scene_phase == 0) title_init();
    else g_scene_ret = title_update();
}
