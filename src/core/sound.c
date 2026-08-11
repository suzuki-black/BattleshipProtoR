/* sound.c — PSG効果音ドライバ＋H.TIMI 60Hz割込み(常駐)。
   前作 BattleshipProto(実機確定)の sfx エンジン/ISR イディオムを移植・整理。 */
#include "sound.h"

/* ---- PSG ポートI/O(規約非依存にファイルスコープ変数経由) ----
   0xA0=レジスタ選択, 0xA1=データ。PSGはVDPと独立ポートなので di 不要。 */
static u8 g_pr, g_pv;
static void psg(u8 r, u8 v) {
    g_pr = r; g_pv = v;
    __asm
        ld   a, (_g_pr)
        out  (0xA0), a
        ld   a, (_g_pv)
        out  (0xA1), a
    __endasm;
}

/* ---- SFX 状態 ---- */
static const u8 sfxDur[SFX_COUNT] = { 0, 8, 4, 28 };   /* NONE/SHOT/HIT/BOOM の持続フレーム */
static u8 sfxType[SND_CH];
static u8 sfxTimer[SND_CH];

volatile u16 snd_ticks;
volatile u8  snd_active;

/* 効果音トリガ。type/timer の2バイト更新中に ISR が割り込むと中途半端を読むので di/ei 原子化。
   破壊音(BOOM)再生中は命中音(HIT)で上書きしない(鳴り切らせる)。 */
void sfx(u8 ch, u8 type) {
    __asm di __endasm;
    if (!(type == SFX_HIT && sfxType[ch] == SFX_BOOM && sfxTimer[ch] != 0)) {
        sfxType[ch] = type;
        sfxTimer[ch] = sfxDur[type];
    }
    __asm ei __endasm;
}

/* 毎フレーム更新(ISRから呼ぶ=外部リンケージ)。各chの残り時間に応じ周波数/音量を更新(簡易エンベロープ)。 */
void sfx_update(void) {
    u8 ch, cnt = 0;
    for (ch = 0; ch < SND_CH; ch++) {
        u8 t, rem;
        if (sfxTimer[ch] == 0) continue;
        sfxTimer[ch]--;
        t = sfxType[ch];
        rem = sfxTimer[ch];
        if (t == SFX_SHOT) {                       /* 高→低の下降レーザー */
            u16 p = 40 + (u16)(7 - rem) * 62;
            psg(0, p & 0xFF); psg(1, (p >> 8) & 0x0F);
            psg(8, (rem >= 2) ? 13 : (rem * 6));
        } else if (t == SFX_HIT) {                 /* 短いノイズ "コッ" */
            psg(6, 15); psg(10, rem * 3);
        } else if (t == SFX_BOOM) {                /* 長い "ズガーン" */
            u8 np = (u8)(3 + (27 - rem));          /* noise周期 3(鋭)→30(深) */
            if (np > 31) np = 31;
            psg(6, np);
            psg(10, (rem >= 20) ? 15 : (u8)((rem * 15) / 20));
        }
        if (sfxTimer[ch] == 0) psg((u8)(8 + ch), 0);   /* 終了で該当chを無音 */
        else cnt++;
    }
    snd_active = cnt;
}

/* H.TIMI から呼ばれる ISR。割込み文脈なので使用レジスタを全退避(__naked で自前 ret)。
   snd_ticks を進め、sfx_update を回す(将来 bgm_update もここへ)。 */
void snd_isr(void) __naked {
    __asm
        push af
        push bc
        push de
        push hl
        push ix
        push iy
        ld   hl, (_snd_ticks)
        inc  hl
        ld   (_snd_ticks), hl
        call _sfx_update
        pop  iy
        pop  ix
        pop  hl
        pop  de
        pop  bc
        pop  af
        ret
    __endasm;
}

static void psg_init(void) {
    psg(7, 0x9C);                  /* mixer: tone A,B on / noise C on / tone C off */
    psg(6, 16);                    /* noise period 既定 */
    psg(8, 0); psg(9, 0); psg(10, 0);
    sfxType[0] = sfxType[1] = sfxType[2] = 0;
    sfxTimer[0] = sfxTimer[1] = sfxTimer[2] = 0;
    snd_ticks = 0;
    snd_active = 0;
}

/* H.TIMI(0xFD9F, 5バイトフック)へ JP snd_isr を仕込む。BIOSの垂直割込みが毎回CALLしてくる。 */
static void install_isr(void) {
    __asm
        di
        ld   a, #0xC3            ; JP opcode
        ld   (0xFD9F), a
        ld   hl, #_snd_isr
        ld   (0xFDA0), hl
        ei
    __endasm;
}

void sound_init(void) {
    psg_init();
    install_isr();
}
