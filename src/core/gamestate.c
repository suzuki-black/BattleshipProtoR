/* gamestate.c — 共有ゲーム状態の実体(常駐)。既定は NORMAL / 残機3。 */
#include "gamestate.h"

u8  g_difficulty = 1;   /* NORMAL */
u8  g_lives_idx  = 1;   /* 3機     */
u8  g_durability = 3;   /* 耐久HP  */
u8  g_stage_sel  = 0;   /* 1面     */
u8  g_continue   = 1;   /* 継続ON  */
u8  g_invinc     = 0;   /* 無敵OFF */
u16 g_score;
u8  g_lives;
u8  g_php;
