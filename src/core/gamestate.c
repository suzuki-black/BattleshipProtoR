/* gamestate.c — 共有ゲーム状態の実体(常駐)。既定は NORMAL / 残機3。 */
#include "gamestate.h"

u8 g_difficulty = 1;   /* NORMAL */
u8 g_lives_idx  = 1;   /* 3機     */
