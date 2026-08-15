/* scene.c — シーンFSM ディスパッチャ(常駐)。
   registry を1か所に集約。update の戻り値でシーン遷移。巨大 main() を作らない。
   bank!=0 のシーンは「冷たいシーン」= 当該バンクの 0xA000 エントリを bcall で実行(常駐窓を食わない)。
   バンク側エントリは g_scene_phase(0=init/1=update)を見て分岐し、update 結果を g_scene_ret に書く。 */
#include "scene.h"
#include "input.h"
#include "vdp.h"
#include "bank.h"

/* --- 常駐シーンの実体。バンクシーンは registry の init/update=0 で bank に番号を持つ --- */
extern void boot_init(void);
extern u8   boot_update(void);
extern void intro_init(void);
extern u8   intro_update(void);
extern void boss_init(void);
extern u8   boss_update(void);
extern void stage_init(void);
extern u8   stage_update(void);

/* シーンID順に登録。SC_COUNT と個数を一致させること。 */
static const Scene registry[SC_COUNT] = {
    /* SC_BOOT   */ { boot_init,  boot_update,  0 },
    /* SC_TITLE  */ { 0,          0,            5 },   /* 冷たいシーン: bank5(bcall) */
    /* SC_CONFIG */ { 0,          0,            6 },   /* 冷たいシーン: bank6        */
    /* SC_STAGE  */ { stage_init, stage_update, 0 },   /* ★連続縦スクロール面        */
    /* SC_INTRO  */ { intro_init, intro_update, 0 },
    /* SC_BOSS   */ { boss_init,  boss_update,  0 },
    /* SC_ENDING */ { 0,          0,            7 },   /* 冷たいシーン: bank7        */
};

u8 g_scene;         /* 現在のシーンID(デバッグ/HUD/検証用に公開)   */
u8 g_scene_phase;   /* バンクシーンへの指示: 0=init / 1=update       */
u8 g_scene_ret;     /* バンク/常駐 update の戻り値(次シーンID)       */

/* シーン cur の phase(0=init,1=update)を実行。バンクシーンは bcall 経由。 */
static void call_scene(u8 cur, u8 phase) {
    const Scene *s = &registry[cur];
    if (s->bank == 0) {
        if (phase == 0) s->init();
        else g_scene_ret = s->update();
    } else {
        g_scene_phase = phase;
        bcall_to(s->bank);   /* バンク側 banked_entry が g_scene_phase を見て分岐 */
    }
}

void scene_run(u8 cur) {
    g_scene = cur;
    call_scene(cur, 0);
    for (;;) {
        input_poll();
        g_scene_ret = SCENE_NONE;
        call_scene(cur, 1);
        if (g_scene_ret != SCENE_NONE && g_scene_ret != cur) {
            cur = g_scene_ret;
            g_scene = cur;
            call_scene(cur, 0);
        }
        vdp_wait_frame();
    }
}
