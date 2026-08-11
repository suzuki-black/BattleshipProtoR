/* scene.c — シーンFSM ディスパッチャ(常駐)。
   registry を1か所に集約。update の戻り値でシーン遷移。巨大 main() を作らない。 */
#include "scene.h"
#include "input.h"
#include "vdp.h"

/* --- 各シーンの実体(それぞれ別ファイル)。追加時はここへ extern を足す --- */
extern void boot_init(void);
extern u8   boot_update(void);
extern void intro_init(void);
extern u8   intro_update(void);
extern void boss_init(void);
extern u8   boss_update(void);

/* シーンID順に登録。SC_COUNT と個数を一致させること。 */
static const Scene registry[SC_COUNT] = {
    /* SC_BOOT  */ { boot_init,  boot_update,  0 },
    /* SC_INTRO */ { intro_init, intro_update, 0 },
    /* SC_BOSS  */ { boss_init,  boss_update,  0 },
};

void scene_run(u8 cur) {
    u8 next;
    registry[cur].init();
    for (;;) {
        input_poll();
        next = registry[cur].update();
        if (next != SCENE_NONE && next != cur) {
            cur = next;
            registry[cur].init();
        }
        vdp_wait_frame();
    }
}
