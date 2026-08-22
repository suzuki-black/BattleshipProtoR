/* scene_ending.c — ★エンディング(冷たいシーン=bank7)。静かなED曲(track2)＋
   なめらかな縦スクロールのスタッフロール(旧版 run_ending 移植)。
   技法: 全クレジット行を一度だけオフスクリーンのバッファB(VRAM Y=528)へ描画し、
   スクロール中は「下端に入る行だけ」1発のLMMM(vdp_copy)で page0 のリング(0..255)へ流し、
   R#23(縦スクロール)を毎フレーム更新する。全画面コピーは重いのでこの1行ストリームで軽量化。
   ※banked scene: 常駐 vdp_* を注入番地で呼ぶ。データ窓(bank_data)は触らない。 */
#include "vdp.h"
#include "input.h"
#include "scene.h"

#define END_B    528   /* バッファB基準VRAM Y(艦バッファと同域=ED時は空き) */
#define END_SPD  4     /* 4フレームで1px=約15px/s(ゆっくり) */
#define END_BG   1     /* 背景=濃紺 */
#define END_TX   15    /* 文字=白 */

/* クレジット行(旧版 ENDALL 準拠, 描画可能文字 [A-Z0-9 -] のみ)。""=空行(間隔)。 */
static const char *const credits[] = {
    "THE ZERO HAS SUNK",
    "EVERY BATTLESHIP",
    "",
    "PEACE RETURNS - BUT",
    "ONLY FOR A MOMENT",
    "",
    "WHAT DID THEY SAVE",
    "AND WHAT WAS LOST",
    "",
    "TIME MARCHES ON",
    "CRUEL AND UNRELENTING",
    "",
    "",
    "STAFF",
    "",
    "CONCEPT - SUZUKI-BLACK",
    "",
    "GRAPHICS - CLAUDE CODE",
    "",
    "SOUND - CLAUDE CODE",
    "",
    "PROGRAM - CLAUDE CODE",
    "",
    "TITLE - MICROSOFT COPILOT",
};
#define NROWS ((u8)(sizeof(credits) / sizeof(credits[0])))

/* 中央寄せX(8px/char)。 */
static u8 center_x(const char *s) {
    u8 n = 0; while (s[n]) n++;
    return (u8)((256 - (u16)n * 8) / 2);
}

/* 全行をバッファB(528..)へ一度だけ描く。空行はBG(初期クリア済み)のまま。 */
static void prerender(void) {
    u8 i;
    vdp_fill(0, END_B, 256, (u16)NROWS * 16, END_BG);   /* B のリールをクリア */
    for (i = 0; i < NROWS; i++) {
        const char *s = credits[i];
        if (s[0]) {
            vdp_fill(0, 224, 256, 16, END_BG);           /* スクラッチ行(page0の非可視Y=224) */
            vdp_text(center_x(s), 224, END_TX, END_BG, s);
            vdp_copy(0, 224, 0, (u16)(END_B + (u16)i * 16), 256, 16);   /* B の i 行へ */
        }
    }
    vdp_fill(0, 224, 256, 16, END_BG);                   /* スクラッチ後始末 */
}

/* エンディング本体(ブロッキング)。スクロール→THE END→タイトルへ。 */
static u8 run_ending(void) {
    s16 cam, stopcam;
    s16 botrow = -1;
    u8  sc = 0;
    u16 f;

    vdp_set_vscroll(0);
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 256, END_BG);      /* page0 リングをクリア */
    prerender();

    stopcam = (s16)((NROWS - 1) * 16 + 24);
    cam = -212;
    while (cam < stopcam) {
        s16 needbot = (s16)((cam + 224) >> 4);      /* 下端に入るべき行 */
        while (botrow < needbot) {
            botrow++;
            if (botrow >= 0 && botrow < (s16)NROWS)
                vdp_copy(0, (u16)(END_B + (u16)botrow * 16), 0, (u16)(((u16)botrow * 16) & 0xFF), 256, 16);
            else
                vdp_fill(0, (u16)(((u16)botrow * 16) & 0xFF), 256, 16, END_BG);
        }
        vdp_wait_frame();
        vdp_set_vscroll((u8)(cam & 0xFF));          /* VBLANK直後にR#23=無 tearing */
        if (++sc >= END_SPD) { sc = 0; cam++; }
        input_poll();
        if (g_input_edge & INP_TRIG) break;         /* SPACEで飛ばせる */
    }

    /* THE END(静止)→ 保持 → タイトル */
    vdp_set_vscroll(0);
    vdp_fill(0, 0, 256, 256, END_BG);
    vdp_text_s((u8)(128 - 7 * 8), 92, END_TX, END_BG, 2, "THE END");   /* 7字×16=112, 中央 */
    for (f = 0; f < 600; f++) {                      /* 約10秒(トリガで即) */
        input_poll();
        if (g_input_edge & INP_TRIG) break;
        vdp_wait_frame();
    }
    return SC_TITLE;
}

void banked_entry(void) {
    if (g_scene_phase == 0) { vdp_set_display_page(0); }   /* init: 表示ページだけ確定 */
    else g_scene_ret = run_ending();                       /* update: 本体を一気に実行しタイトルへ */
}
