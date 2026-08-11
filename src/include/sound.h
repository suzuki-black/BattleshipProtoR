/* sound.h — PSG効果音ドライバ(常駐)。H.TIMI(60Hz垂直割込み)で毎フレーム更新し、
   ゲーム負荷に依らずテンポ/エンベロープ一定(フレームレート非依存)。
   ※効果音のトリガ(sfx)は type/timer を立てるだけ。実際の PSG 書込・減衰は ISR 側で行う
     (PSG は VDP と独立ポートなので ISR とメインの競合は type/timer の di/ei 原子化のみで足りる)。
   BGM(音符データ=ROMバンク)は実バンクコール(#2)導入後に追加する。 */
#ifndef SOUND_H
#define SOUND_H

#include "types.h"

/* 効果音の系統(添字は内部の持続長表と対) */
enum {
    SFX_NONE = 0,
    SFX_SHOT,   /* 自機弾: 高→低の下降レーザー "ぴゅん"   */
    SFX_HIT,    /* 命中(生存): 短いノイズ "コッ"          */
    SFX_BOOM,   /* 破壊: 長い "ズガーン"(鋭→深→減衰)      */
    SFX_COUNT
};

/* 3ch: 0=自機系(tone A) / 1=敵系(tone B) / 2=命中破壊(noise C) を想定 */
#define SND_CH 3

void sound_init(void);        /* PSG初期化 + H.TIMI ISR 設置。boot時1回 */
void sfx(u8 ch, u8 type);     /* ch(0..2) に効果音 type をトリガ(同chは後勝ち。破壊音は保護) */

/* ISR 稼働の観測点(検証・HUD用) */
extern volatile u16 snd_ticks;   /* ISRが毎フレーム ++(H.TIMI稼働の証跡) */
extern volatile u8  snd_active;  /* 現在発音中の ch 数(ISRが毎フレーム再計算) */

#endif /* SOUND_H */
