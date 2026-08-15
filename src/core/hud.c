/* hud.c — スプライトHUD実装。数字パターンは BIOS 8x8 フォント('0'..'9')を
   16x16 スプライトの左上 8x8 へ写して生成(vdp_text と同じ CGTABL 経由)。
   V9938 sprite mode2 は 1走査線 8枚まで表示できるので、上端に数字を6枚並べても欠けない。 */
#include "hud.h"
#include "vdp.h"
#include "sprites.h"
#include "entity.h"   /* g_spr_base(エンティティ描画の開始スロット) */

void hud_init(void) {
    const u8 *font = (const u8 *)(*(volatile u16 *)0x0004);   /* CGTABL → BIOS フォント */
    u8 pat[32];
    u8 d, r;
    for (d = 0; d < 10; d++) {
        const u8 *g = font + ((u16)('0' + d)) * 8;
        for (r = 0; r < 8;  r++) pat[r] = g[r];   /* 左列 rows0-7 = 8x8 グリフ */
        for (r = 8; r < 32; r++) pat[r] = 0;      /* 左列下半分＋右列は空 */
        vdp_sprite_pattern(SPR_DIGIT0 + d * 4, pat);
    }
    /* 色は固定(位置だけ毎フレーム更新): スコア=白 / 残機=黄 */
    for (d = 0; d < 5; d++) vdp_sprite_color(d, 15);
    vdp_sprite_color(5, 11);
    g_spr_base = HUD_SLOTS;   /* 以降エンティティは slot6 から詰める */
}

/* スコアを 5桁ゼロ詰め(slot0-4)、残機を1桁(slot5, 右上)で描画。 */
void hud_draw(u16 score, u8 lives) {
    static const u16 place[5] = { 10000, 1000, 100, 10, 1 };
    u8 i;
    for (i = 0; i < 5; i++) {
        u8 dg = (u8)((score / place[i]) % 10);
        vdp_sprite_pos(i, (u8)(8 + i * 8), 2, (u8)(SPR_DIGIT0 + dg * 4));
    }
    vdp_sprite_pos(5, 232, 2, (u8)(SPR_DIGIT0 + (lives % 10) * 4));
}
