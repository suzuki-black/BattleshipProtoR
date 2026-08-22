/* hud.c — スプライトHUD実装。数字パターンは自前8x8フォント('0'..'9')を
   16x16 スプライトの左上 8x8 へ写して生成(vdp_text と同じ vdp_glyph 経由=書体を統一)。
   V9938 sprite mode2 は 1走査線 8枚まで表示できるので、上端に数字を6枚並べても欠けない。 */
#include "hud.h"
#include "vdp.h"
#include "sprites.h"
#include "entity.h"   /* g_spr_base(エンティティ描画の開始スロット) */

void hud_init(void) {
    u8 pat[32];
    u8 d, r;
    for (d = 0; d < 10; d++) {
        const u8 *g = vdp_glyph((u8)('0' + d));   /* 自前フォントの数字グリフ */
        for (r = 0; r < 8;  r++) pat[r] = g[r];   /* 左列 rows0-7 = 8x8 グリフ */
        for (r = 8; r < 32; r++) pat[r] = 0;      /* 左列下半分＋右列は空 */
        vdp_sprite_pattern(SPR_DIGIT0 + d * 4, pat);
    }
    /* 色は固定(位置だけ毎フレーム更新): スコア=白 / 残機アイコン=零戦の緑 / 残機数=黄 */
    for (d = 0; d < 5; d++) vdp_sprite_color(d, 15);
    vdp_sprite_color(5, 3);    /* 残機アイコン=緑(零戦シルエット) */
    vdp_sprite_color(6, 11);   /* 残機数=黄 */
    g_spr_base = HUD_SLOTS;   /* 以降エンティティは slot7 から詰める */
}

/* スコア5桁ゼロ詰め(slot0-4)＋残機=零戦アイコン(slot5)＋予備機数1桁(slot6, 右上)。
   HUDは低slot=高優先なので、8枚/走査線を超えても敵機(slot7+)が先に間引かれHUDは残る。 */
void hud_draw(u16 score, u8 lives) {
    static const u16 place[5] = { 10000, 1000, 100, 10, 1 };
    u8 i;
    for (i = 0; i < 5; i++) {
        u8 dg = (u8)((score / place[i]) % 10);
        vdp_sprite_pos(i, (u8)(8 + i * 8), 2, (u8)(SPR_DIGIT0 + dg * 4));
    }
    vdp_sprite_pos(5, 212, 1, SPR_ZERO);                                   /* 残機=零戦シルエット */
    vdp_sprite_pos(6, 234, 2, (u8)(SPR_DIGIT0 + (lives % 10) * 4));        /* 予備機数(9頭打ち) */
}
