/* scene.h — シーンFSM(汎用)。title/空戦イントロ/戦艦ボス/撃破/ending/gameover を
   すべて {init, update} の関数対で表現し、巨大 main() を作らない(SDCC破綻回避)。

   将来の拡張(冷たいシーンをバンク化):
     Scene に bank を持たせ、bank!=0 の場合はディスパッチャが g_bank=bank; bcall() で
     当該バンクの 0xA000 エントリを呼ぶ(そのエントリが init/update を g_scene_phase で分岐)。
     今は常駐シーン(bank=0)のみ実装。詳細は docs/ARCHITECTURE.md。 */
#ifndef SCENE_H
#define SCENE_H

#include "types.h"

#define SCENE_NONE 0xFF   /* update の戻り値: シーン継続(遷移なし) */

/* シーンID(登録順)。追加時はここと scene.c の registry を対で更新。 */
enum {
  SC_BOOT = 0,
  SC_INTRO,   /* 空戦イントロ(縦スクロールのみ) */
  SC_BOSS,    /* 戦艦ボス(蛇行スクロール)       */
  SC_COUNT
};

typedef struct {
  void (*init)(void);     /* 遷移してきた時に1回          */
  u8   (*update)(void);   /* 毎フレーム。次のシーンID or SCENE_NONE */
  u8   bank;              /* 0=常駐 / 非0=当該バンクで bcall(将来) */
} Scene;

/* start シーンから開始し、以後メインループを回す(ROM: 戻らない)。 */
void scene_run(u8 start);

#endif /* SCENE_H */
