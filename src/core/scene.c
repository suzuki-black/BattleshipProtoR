/* scene.c — シーンFSM ディスパッチャ(常駐)。
   registry を1か所に集約。update の戻り値でシーン遷移。巨大 main() を作らない。
   bank!=0 のシーンは「冷たいシーン」= 当該バンクの 0xA000 エントリを bcall で実行(常駐窓を食わない)。
   バンク側エントリは g_scene_phase(0=init/1=update)を見て分岐し、update 結果を g_scene_ret に書く。 */
#include "scene.h"
#include "input.h"
#include "vdp.h"
#include "bank.h"
#include "sound.h"

/* シーン入場時に鳴らす BGM トラック。BGM_KEEP=変えない(継続) / BGM_OFF=停止。
   ★bgm_play は data_read(窓差替え)を伴うので常駐(scene_run)から呼ぶ=ここが正しい場所。 */
#define BGM_KEEP 0xFF
#define BGM_OFF  0xFE
static const u8 scene_bgm[SC_COUNT] = {
    /* SC_TITLE  */ 0,         /* タイトル曲 */
    /* SC_CONFIG */ BGM_KEEP,  /* タイトル曲を継続 */
    /* SC_STAGE  */ BGM_OFF,   /* 入場時は無音: 開始カードでファンファーレのみ→stage_introがメインBGMを開始 */
    /* SC_ENDING */ 2,         /* 静かなED曲(track2) */
};
static void scene_bgm_enter(u8 cur) {
    u8 t = scene_bgm[cur];
    if (t == BGM_KEEP) return;
    if (t == BGM_OFF)  bgm_stop();
    else               bgm_play(t);
}

/* シーン入場時のビデオモード。SC_TITLE のみ SCREEN12(YJK自然画)、他は SCREEN5。
   タイトル画(54272B, bank9..15)の VRAM流し込みは窓差替えを伴うため常駐文脈でしか行えない
   (バンクシーン内からだと窓復元で自シーンを追い出す)。よって bcall より前のここで済ませる。
   モードが変わる時だけ CHGMOD する(g_vmode で追跡)。SCREEN5復帰時はパレットも張り直す。 */
#define TITLE_BANK    9
#define TITLE_YJK_LEN 54272
static u8 g_vmode;   /* 現在のスクリーンモード(5/12)。0=未設定で初回に必ず張る。 */
static void scene_video_enter(u8 cur) {
    u8 want = (cur == SC_TITLE) ? 12 : 5;
    if (want == g_vmode) return;
    g_vmode = want;
    if (want == 12) {
        vdp_screen12();
        vdp_blit_bank_vram(TITLE_BANK, TITLE_YJK_LEN);
    } else {
        vdp_screen5();
        vdp_palette_game();
    }
}

/* --- 常駐シーンの実体。バンクシーンは registry の init/update=0 で bank に番号を持つ --- */
extern void stage_init(void);
extern u8   stage_update(void);

/* シーンID順に登録。SC_COUNT と個数を一致させること。 */
static const Scene registry[SC_COUNT] = {
    /* SC_TITLE  */ { 0,          0,            5 },   /* 冷たいシーン: bank5(bcall)。起動シーン */
    /* SC_CONFIG */ { 0,          0,            6 },   /* 冷たいシーン: bank6        */
    /* SC_STAGE  */ { stage_init, stage_update, 0 },   /* ★連続縦スクロール面        */
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

#ifdef DEBUG_FPS
u8  g_fps;         /* 実FPS(JIFFY増分を校正して算出)。hud_drawが2桁表示 */
u16 g_frame;       /* ★JIFFY非依存: ループ毎+1。ストップウォッチ実測用(検証)。hud_drawが4桁表示 */
static u16 fps_n = 1;  /* JIFFYの1VBLANKあたり増分(実機turboRでは1でない疑い=起動時に実測校正) */
#endif
void scene_run(u8 cur) {
    g_scene = cur;
    scene_video_enter(cur);  /* ビデオモード確立(SC_TITLEはSCREEN12化＋YJK流し込み)を先に */
    scene_bgm_enter(cur);    /* 窓=bank3 の常駐文脈で(bcall前に)曲を差替える */
    call_scene(cur, 0);
#ifdef DEBUG_FPS
    /* ★JIFFY(0xFC9E)の1VBLANKあたり増分を実測校正。実機turboRでは+1でない疑いがあり、
       これを測らないとFPS換算(60=1秒)が狂う。ガード(g)付きでJIFFY停止時もハングしない。 */
    { volatile u16 *jf = (volatile u16 *)0xFC9E; u16 a, b, g;
      a = *jf; g = 0; while (*jf == a && ++g) { }   /* 変化境界へ整列 */
      a = *jf; g = 0; while (*jf == a && ++g) { }   /* ちょうど1VBLANK分 */
      b = *jf; fps_n = (u16)(b - a); if (fps_n == 0) fps_n = 1; }
#endif
    for (;;) {
        input_poll();
        g_scene_ret = SCENE_NONE;
        call_scene(cur, 1);
        if (g_scene_ret != SCENE_NONE && g_scene_ret != cur) {
            cur = g_scene_ret;
            g_scene = cur;
            scene_video_enter(cur);
            scene_bgm_enter(cur);
            call_scene(cur, 0);
        }
#ifdef DEBUG_FPS
        g_frame++;   /* ★JIFFY非依存の真フレーム数(ストップウォッチ検証用) */
        /* 実FPS: 校正済み fps_n を使い「JIFFYが 60*fps_n 進む=実時間1秒」あたりのループ数=FPS。 */
        { static u16 lastj; static u8 fc;
          u16 j = *(volatile u16 *)0xFC9E;
          fc++;
          if ((u16)(j - lastj) >= (u16)(60 * fps_n)) { g_fps = (fc > 99) ? 99 : fc; fc = 0; lastj = j; } }
#endif
        vdp_wait_frame();
    }
}
