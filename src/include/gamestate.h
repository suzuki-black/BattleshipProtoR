/* gamestate.h — ゲーム全体で共有する状態(常駐)。config(バンク)が設定し、gameplay が読む。 */
#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "types.h"

extern u8  g_difficulty;   /* 0=EASY / 1=NORMAL / 2=HARD */
extern u8  g_lives_idx;    /* 残機テーブル添字(0=2 / 1=3 / 2=5) */
extern u16 g_score;        /* スコア(撃破で加算) */
extern u8  g_lives;        /* 現在の残機(面開始で g_lives_idx から設定) */

#endif /* GAMESTATE_H */
