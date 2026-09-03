/* scene_ending.c — ★エンディング(冷たいシーン=bank7)。静かなED曲(track2)＋
   なめらかな縦スクロールのスタッフロール(旧版 run_ending 移植)。
   技法: page0(0..255)を256pxリングにし、R#23(縦スクロール)を毎フレーム更新。スクロールで下端
   (可視域直下の非可視帯)に入ってくる行だけを、その場で vdp_text 直接描画する(1行ストリーム)。
   ★旧実装は全行をバッファBへ事前描画していたが B=最大31行の固定上限があった。直接描画に変えて
     行数上限を撤廃(=物語/クレジットの分量に融通が利く)。描画は非可視帯で行うので tear しない。
   ※banked scene: 常駐 vdp_* を注入番地で呼ぶ。データ窓(bank_data)は触らない。 */
#include "vdp.h"
#include "input.h"
#include "scene.h"

#define END_SPD  4     /* 4フレームで1px=約15px/s(ゆっくり) */
#define END_BG   1     /* 背景=濃紺 */
#define END_TX   15    /* 文字=白 */

/* クレジット行(旧版 ENDALL 準拠, 描画可能文字 [A-Z0-9 -] のみ)。""=空行(間隔)。 */
static const char *const credits[] = {
    /* ★エンディングストーリー: READMEあらすじ(走れメロス冒頭のオマージュ)への返歌=結末のオマージュ。
       暖かくユーモラスに締める。会話は追加した '"' / ',' グリフを使用。 */
    "IN HIS LONE ZERO",
    "THE SAMURAI HAD SUNK",
    "EVERY LAST BATTLESHIP.",
    "",
    "HIS GREAT WORK DONE,",
    "HE CAME HOME TO HIS VILLAGE",
    "STILL IN THE CLOTHES",
    "THE FIERCE BATTLE HAD TORN.",
    "",
    "THE VILLAGERS CROWDED ROUND,",
    "AND WITH ONE VOICE",
    "THEY PRAISED HIS DEED.",
    "",
    "THEN A YOUNG GIRL HELD OUT",
    "A CRIMSON CLOAK TO HIM.",
    "THE SAMURAI STOOD AT A LOSS.",
    "",
    "A GOOD FRIEND OF THE VILLAGE,",
    "QUICK TO CATCH ON,",
    "KINDLY TOLD HIM,",
    "",
    "\"WHY, YOU ARE ALL BUT NAKED.",
    "HURRY, PUT ON THAT CLOAK.",
    "THIS DEAR GIRL CANNOT BEAR",
    "TO HAVE ALL SEE YOU BARE.\"",
    "",
    "THE SAMURAI BLUSHED",
    "A DEEP, DEEP RED.",
    "",
    "",
    "STAFF",
    "",
    "ORIGINAL PLAN",           /* 原案 */
    "SUZUKI-BLACK",
    "",
    "ORIGINAL CONCEPT",
    "SUZUKI-BLACK",
    "",
    "DIRECTION",
    "SUZUKI-BLACK",
    "",
    "PROGRAM",
    "CLAUDE CODE",
    "",
    "GRAPHICS",
    "CLAUDE CODE",
    "",
    "SOUND",
    "CLAUDE CODE",
    "",
    "TITLE ILLUSTRATION",
    "MS COPILOT",
    "",
    "CO-TITLE ILLUSTRATION",   /* 共同=CO-(映画クレジット流) */
    "CLAUDE CODE",
};
#define NROWS ((u8)(sizeof(credits) / sizeof(credits[0])))

/* 中央寄せX(8px/char)。 */
static u8 center_x(const char *s) {
    u8 n = 0; while (s[n]) n++;
    return (u8)((256 - (u16)n * 8) / 2);
}

/* エンディング本体(ブロッキング)。スクロール→THE END→タイトルへ。
   ★行を事前にバッファBへ描く方式(B=最大31行の固定上限)をやめ、スクロールで下端に入る行だけを
     その場で直接描画する(=クレジット行数の上限が消える=融通が利く)。描画位置は可視域(212px)の
     直下の非可視帯なので、スクロールで上がってくる頃には描き終わっている=tearしない。 */
static u8 run_ending(void) {
    s16 cam, stopcam;
    s16 botrow = -1;
    u8  sc = 0;
    u16 f;

    /* ★_bcall は banked 実行中ずっと di だが、本体は vdp_wait_frame(JIFFYを割込みで更新)で
       毎フレーム待つ=di のままだと JIFFY が進まず無限ループ(フリーズ)。BGM再生ISRは曲を
       RAM(bgm_ram)から読み 0xA000窓に触れないので、ここで ei しても bank7 窓は壊れない。 */
    __asm ei __endasm;
    vdp_set_vscroll(0);
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 256, END_BG);      /* page0 リングをクリア */

    stopcam = (s16)((NROWS - 1) * 16 + 24);
    cam = -212;
    while (cam < stopcam) {
        /* ★重い vdp_text 描画は「camを進めないフレーム」に、1フレーム1行だけ先読みで行う。
           理由: vdp_text は1文字ずつピクセル描画で重く、cam更新(=R#23が変わる=スクロールが動く)
           フレームで描くと1フレーム溢れて約1秒毎(1行=64フレーム)にスクロールがカクつく。camを
           進めないフレーム(R#23が前と同値)なら描画で溢れても見た目は停止しない=カクつきが消える。
           非可視帯(可視域直下)へ2行先まで先読みするので、上がってくる頃には描き終わっている。 */
        if (sc != 0) {   /* cam更新はsc:3→0の遷移だけ=sc!=0のフレームはR#23不変(スクロール静止)フレーム */
            /* ★先読みは+1行まで(リング=16行、可視≒13.25行なので余白は約2.75行=44px。基準+13px＋16px=29px<44pxで
               「上端に残る可視行」と衝突しない。+2行=45pxにすると一番上の可視行を上書きして順序が乱れる)。 */
            s16 want = (s16)((cam + 224) >> 4) + 1;   /* 可視域直下＋1行先まで(idleフレームの1フレーム遅延を吸収) */
            if (botrow < want) {
                botrow++;
                { u16 ry = (u16)(((u16)botrow * 16) & 0xFF);   /* リング(page0)の該当16px行 */
                  vdp_fill(0, ry, 256, 16, END_BG);            /* 行をBGでクリア(中央寄せの左右余白も) */
                  if (botrow >= 0 && botrow < (s16)NROWS && credits[botrow][0])
                      vdp_text(center_x(credits[botrow]), (u8)ry, END_TX, END_BG, credits[botrow]); }
            }
        }
        vdp_wait_frame();
        vdp_set_vscroll((u8)(cam & 0xFF));          /* VBLANK直後にR#23=無 tearing */
        if (++sc >= END_SPD) { sc = 0; cam++; }     /* END_SPDフレームで1px前進 */
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
