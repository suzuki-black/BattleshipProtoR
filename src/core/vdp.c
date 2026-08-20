/* vdp.c — VDP(V9958)アクセスの常駐実装。
   前作 BattleshipProto(実機確定)の MSXgl 流イディオムを踏襲:
     0x99 への2バイト書込は割込みで壊れるので各自 di/ei で原子化する。 */
#include "vdp.h"
#include "msx.h"
#include "bank.h"    /* vdp_blit_bank_vram が窓めくりに使う(bank_data/bank_restore) */

__sfr __at(0x98) VDP_DAT;    /* VRAM データ                          */
__sfr __at(0x99) VDP_CTRL;   /* アドレス/レジスタ                    */
__sfr __at(0x9A) VDP_PAL;    /* パレットデータ                       */
__sfr __at(0x9B) VDP_IDAT;   /* 間接レジスタ(R#17 オートインクリメント) */

void vdp_wreg(u8 r, u8 v) {
    __asm di __endasm;
    VDP_CTRL = v;
    VDP_CTRL = 0x80 | r;
    __asm ei __endasm;
}

void vdp_set_pal(u8 idx, u8 r, u8 g, u8 b) {
    __asm di __endasm;
    VDP_CTRL = idx;
    VDP_CTRL = 0x80 | 16;         /* R#16 = パレットポインタ */
    VDP_PAL  = (r << 4) | b;
    VDP_PAL  = g;
    __asm ei __endasm;
}

/* R#14(アドレス上位)〜アドレス下位/上位ラッチ確立までを1つの di/ei に閉じて原子化。 */
void vdp_write_addr(u16 a) {
    __asm di __endasm;
    VDP_CTRL = (a >> 14) & 7;   VDP_CTRL = 0x80 | 14;
    VDP_CTRL = a & 0xFF;        VDP_CTRL = ((a >> 8) & 0x3F) | 0x40;
    __asm ei __endasm;
}

void vdp_data(u8 v) {
    VDP_DAT = v;
}

/* SCREEN5(GRAPHIC4)へ。BIOS ワーク(SCRMOD)を 5 にして CHGMOD。
   ★R#25 を明示的に 0 へ: BIOS CHGMOD は V9958 拡張レジスタ R#25 を管理しないため、
     SCREEN12(R#25 YJK=1)からの復帰で YJK ビットが残り、GRAPHIC4 の表示が壊れる(海が白化)。
     ここで必ず 0 に落として YJK/横スクロール拡張の残留を断つ。 */
void vdp_screen5(void) {
    __asm
        ld   a, #5
        ld   (0xFCAF), a       ; SCRMOD_W
        call 0x005F            ; CHGMOD
    __endasm;
    vdp_wreg(25, 0x00);        /* YJK/YAE/SP2/MSK を全クリア(SCREEN12 残留対策) */
}

/* ゲーム標準パレット(SCREEN5)。CHGMOD は既定パレットに戻すので、SCREEN5 へ入る度に張り直す。
   既定パレット + 4色の上書き(黒/海の青/赤/白)+ ボーダー青。爆発色(11,10,6,14等)は既定を使う。 */
/* ★旧版 BattleshipProto の艦/海パレットを全面採用(艦レンダラが要求する色ランプに一致させる)。
   1/2=海(明暗の青), 7=暗海=艦影(SHADOWC), 6=木甲板, 9=オリーブ, 12=橙, 4/5=灰, 13=ほぼ黒,
   14=淡灰, 15=白, 3=暗緑, 8=緑, 10=明緑, 11=赤。 */
void vdp_palette_game(void) {
    vdp_set_pal(0, 0, 0, 0);
    vdp_set_pal(1, 1, 4, 5);    /* 海(中) */
    vdp_set_pal(2, 2, 5, 6);    /* 海(明) */
    vdp_set_pal(7, 0, 1, 3);    /* 暗海 / 艦のドロップシャドウ(SHADOWC) */
    vdp_set_pal(6, 6, 5, 3);    /* 木甲板 */
    vdp_set_pal(9, 3, 3, 1);    /* オリーブ(縦通材/舷側陰) */
    vdp_set_pal(12, 7, 4, 0);   /* 橙(甲板斑点/発砲) */
    vdp_set_pal(4, 3, 3, 3);    /* 中灰(上部構造/ドーム) */
    vdp_set_pal(5, 2, 2, 2);    /* 灰(砲塔ベース/ドーム) */
    vdp_set_pal(14, 4, 4, 5);   /* 淡灰(ハイライト) */
    vdp_set_pal(13, 1, 1, 1);   /* ほぼ黒(外周/影) */
    vdp_set_pal(3, 1, 3, 1);    /* 暗緑 */
    vdp_set_pal(8, 2, 5, 2);    /* 緑 */
    vdp_set_pal(10, 4, 6, 4);   /* 明緑 */
    vdp_set_pal(11, 7, 1, 1);   /* 赤 */
    vdp_set_pal(15, 7, 7, 7);   /* 白 */
    vdp_wreg(7, 0x00);          /* ボーダー=黒 */
}

/* SCREEN12(GRAPHIC7 + YJK 自然画, 256x212, 256B/line, 約19268色)へ。
   BIOS ワーク(SCRMOD)を 8 にして CHGMOD で GRAPHIC7 を立て、R#25 の YJK ビットを付ける。
   R#25 = 0x08: YJK=1(YJKデコード on), YAE=0(パレット併用なし)。前作(実機確定)と同一手順。 */
void vdp_screen12(void) {
    __asm
        ld   a, #8
        ld   (0xFCAF), a       ; SCRMOD_W = 8 (GRAPHIC7)
        call 0x005F            ; CHGMOD
    __endasm;
    vdp_wreg(25, 0x08);        /* R#25: YJK=1 → SCREEN12 */
}

/* bank から連続する生データ(total バイト)を現行スクリーンの VRAM 先頭(0)へ流し込む。
   YJKタイトル画(54272B, bank9..15)のロード用。
   ★V9938 の VRAMアドレスカウンタは14bit(=16KB)で、オートインクリメントの桁上げは R#14 へ
     伝播しない。ゆえに 16KB を跨がぬよう、8KB(=バンク境界)ごとに vdp_write_addr で貼り直す。
     8KB×連続バンクは常に 0/8192 始まりなので、各チャンク内で14bit桁上げは起きない。
   窓(0xA000)は bank ごとにめくる。ISR(音)は窓/VDPアドレスに触れないので di 不要
     (VDP_DAT=0x98 のオートインクリメント書込はフリップフロップを使わない)。 */
void vdp_blit_bank_vram(u8 first_bank, u16 total) {
    const volatile u8 *win = (const volatile u8 *)0xA000;
    u16 off = 0;
    u8 bank = first_bank;
    while (off < total) {
        u16 j, chunk = (u16)(total - off);
        if (chunk > 0x2000) chunk = 0x2000;    /* 8KB */
        bank_data(bank);
        vdp_write_addr(off);
        for (j = 0; j < chunk; j++) VDP_DAT = win[j];
        off = (u16)(off + chunk);
        bank++;
    }
    bank_restore();
}

/* VDPコマンド完了待ち。CE(S#2 bit0)=1 の間ループ。
   重要: S#2 選択中に VBLANK 割込みが入ると、MSX の ISR が S#0(割込フラグ)でなく S#2 を
   読んでフラグを消せず→割込が消えず永久再突入でハングする。よって各ポーリングを di で囲み、
   必ず S#0 へ戻してから ei する(S#2 選択窓を最小化しつつ割込は基本 on に保つ)。 */
void vdp_cmd_wait(void) {
    __asm
    00001$:
        di
        ld   a, #2
        out  (0x99), a
        ld   a, #0x8F         ; R#15 = 2 (S#2 選択)
        out  (0x99), a
        in   a, (0x99)
        rra                    ; CE -> Carry
        ld   a, #0
        out  (0x99), a
        ld   a, #0x8F          ; R#15 = 0 (S#0 へ戻す)
        out  (0x99), a
        ei
        jp   c, 00001$
    __endasm;
}

/* LMMV(論理矩形塗り): (dx,dy)から(nx,ny)pxを色 color で塗る。座標・色ともピクセル単位。
   ※HMMV(0xC0)はバイト単位のため G4(2px/byte)では縞になる。塗りは LMMV(0x80)を使う。
   R#36..R#46 を個別書込(前作の実績イディオム)。前コマンド完了を待ってから発行。 */
void vdp_fill(u16 dx, u16 dy, u16 nx, u16 ny, u8 color) {
    vdp_cmd_wait();
    vdp_wreg(36, dx & 0xFF);  vdp_wreg(37, (dx >> 8) & 0x01);  /* DX (9bit) */
    vdp_wreg(38, dy & 0xFF);  vdp_wreg(39, (dy >> 8) & 0x03);  /* DY (10bit) */
    vdp_wreg(40, nx & 0xFF);  vdp_wreg(41, (nx >> 8) & 0x01);  /* NX (9bit) */
    vdp_wreg(42, ny & 0xFF);  vdp_wreg(43, (ny >> 8) & 0x03);  /* NY (10bit) */
    vdp_wreg(44, color);                                       /* CLR (色0-15) */
    vdp_wreg(45, 0);                                           /* ARG (DIX=DIY=0) */
    vdp_wreg(46, 0x80);                                        /* CMD = LMMV(論理IMP) */
}

/* LMMM: VRAM→VRAM 論理コピー。R#32-45 を設定し R#46=0x90。前コマンド完了を待つ。 */
void vdp_copy(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny) {
    vdp_cmd_wait();
    vdp_wreg(32, sx & 0xFF);  vdp_wreg(33, (sx >> 8) & 0x01);   /* SX (9bit)  */
    vdp_wreg(34, sy & 0xFF);  vdp_wreg(35, (sy >> 8) & 0x03);   /* SY (10bit) */
    vdp_wreg(36, dx & 0xFF);  vdp_wreg(37, (dx >> 8) & 0x01);   /* DX (9bit)  */
    vdp_wreg(38, dy & 0xFF);  vdp_wreg(39, (dy >> 8) & 0x03);   /* DY (10bit) */
    vdp_wreg(40, nx & 0xFF);  vdp_wreg(41, (nx >> 8) & 0x01);   /* NX (9bit)  */
    vdp_wreg(42, ny & 0xFF);  vdp_wreg(43, (ny >> 8) & 0x03);   /* NY (10bit) */
    vdp_wreg(44, 0);          vdp_wreg(45, 0);                   /* CLR/ARG    */
    vdp_wreg(46, 0x90);                                          /* CMD = LMMM */
}

void vdp_wait_frame(void) {
    volatile u16 *j = (volatile u16 *)0xFC9E;   /* JIFFY */
    u16 t = *j;
    while (*j == t) { }
}

/* ===== 自前 8x8 フォント(旧版 fontset を移植) =====
   ★BIOS の CGTABL(0x0004)には依存しない: C-BIOS は CGTABL が不安定で、実機とも書体が食い違うため
   (旧版でも同理由で自前フォントにしていた)。A-Z＋0-9＋'-' の37字。未収録/空白は空白として描く。 */
static const u8 fontset[37][8] = {
    { 0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00 },  /* 0=A  */
    { 0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00 },  /* 1=B  */
    { 0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00 },  /* 2=C  */
    { 0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00 },  /* 3=E  */
    { 0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00 },  /* 4=G  */
    { 0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00 },  /* 5=H  */
    { 0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00 },  /* 6=I  */
    { 0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00 },  /* 7=K  */
    { 0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00 },  /* 8=L  */
    { 0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00 },  /* 9=M  */
    { 0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00 },  /* 10=O */
    { 0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00 },  /* 11=P */
    { 0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00 },  /* 12=R */
    { 0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00 },  /* 13=S */
    { 0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00 },  /* 14=T */
    { 0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00 },  /* 15=V */
    { 0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00 },  /* 16=- */
    { 0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00 },  /* 17=0 */
    { 0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00 },  /* 18=1 */
    { 0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00 },  /* 19=2 */
    { 0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00 },  /* 20=3 */
    { 0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00 },  /* 21=4 */
    { 0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00 },  /* 22=5 */
    { 0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00 },  /* 23=6 */
    { 0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00 },  /* 24=7 */
    { 0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00 },  /* 25=8 */
    { 0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00 },  /* 26=9 */
    { 0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00 },  /* 27=X */
    { 0x7C,0x66,0x66,0x66,0x66,0x66,0x7C,0x00 },  /* 28=D */
    { 0x63,0x73,0x7B,0x6F,0x67,0x63,0x63,0x00 },  /* 29=N */
    { 0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00 },  /* 30=U */
    { 0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00 },  /* 31=W */
    { 0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00 },  /* 32=Y */
    { 0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00 },  /* 33=F */
    { 0x1E,0x06,0x06,0x06,0x66,0x66,0x3C,0x00 },  /* 34=J */
    { 0x3C,0x66,0x66,0x66,0x66,0x6C,0x36,0x00 },  /* 35=Q */
    { 0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00 },  /* 36=Z */
};
/* A-Z(0..25) → fontset 添字。順不同(旧版の格納順)。 */
static const u8 az_idx[26] = {
    0,1,2,28,3,33,4,5,6,34,7,8,9,29,10,11,35,12,13,14,30,15,31,27,32,36
};
static s8 font_index(u8 c) {
    if (c >= '0' && c <= '9') return (s8)(17 + (c - '0'));
    if (c >= 'A' && c <= 'Z') return (s8)az_idx[c - 'A'];
    if (c == '-') return 16;
    return -1;   /* 空白/未収録は空白 */
}

/* 文字 c の 8x8 グリフ(8B)を返す。未収録/空白は空グリフ。HUD の数字スプライト等が自前フォントを使う用。 */
const u8 *vdp_glyph(u8 c) {
    static const u8 blank[8] = { 0,0,0,0,0,0,0,0 };
    s8 idx = font_index(c);
    return (idx >= 0) ? fontset[idx] : blank;
}

/* 文字列描画(自前フォント)。1文字=8行×8px=4byte(2px/byte)を SCREEN5 page0 へ直接。
   px 偶数前提。fg/bg で地色も塗る。未収録文字は空白セルとして地色で埋める。 */
void vdp_text(u8 px, u8 py, u8 fg, u8 bg, const char *s) {
    u8 c, r;
    vdp_cmd_wait();   /* 直前のLMMV(塗り)完了を待つ。走行中に直接VRAM書込すると衝突し文字が欠ける。 */
    while ((c = (u8)*s++) != 0) {
        s8 idx = font_index(c);
        const u8 *g = (idx >= 0) ? fontset[idx] : (const u8 *)0;
        for (r = 0; r < 8; r++) {
            u8 fb = g ? g[r] : 0;
            vdp_write_addr((u16)((u16)(py + r) * 128 + (px >> 1)));
            VDP_DAT = (u8)(((fb & 0x80) ? fg : bg) << 4 | ((fb & 0x40) ? fg : bg));
            VDP_DAT = (u8)(((fb & 0x20) ? fg : bg) << 4 | ((fb & 0x10) ? fg : bg));
            VDP_DAT = (u8)(((fb & 0x08) ? fg : bg) << 4 | ((fb & 0x04) ? fg : bg));
            VDP_DAT = (u8)(((fb & 0x02) ? fg : bg) << 4 | ((fb & 0x01) ? fg : bg));
        }
        px += 8;
    }
}

/* 拡大文字描画(自前フォント, scale 倍角)。各グリフ画素を scale×scale ブロックへ展開。
   1文字=8*scale px 幅 / 8*scale 行。px は偶数前提(2px/byte 境界)。見出し(STAGE/TARGET/艦名)用。 */
void vdp_text_s(u8 px, u8 py, u8 fg, u8 bg, u8 scale, const char *s) {
    u8 c;
    vdp_cmd_wait();
    while ((c = (u8)*s++) != 0) {
        s8 idx = font_index(c);
        const u8 *g = (idx >= 0) ? fontset[idx] : (const u8 *)0;
        u8 gr;
        for (gr = 0; gr < 8; gr++) {           /* グリフの各行 */
            u8 fb = g ? g[gr] : 0;
            u8 sv;
            for (sv = 0; sv < scale; sv++) {   /* 縦 scale 倍 */
                u8 bit, pending = 0, have = 0;
                vdp_write_addr((u16)((u16)(u8)(py + gr * scale + sv) * 128 + (px >> 1)));
                for (bit = 0; bit < 8; bit++) {            /* MSB→LSB の8画素 */
                    u8 col = (fb & (u8)(0x80 >> bit)) ? fg : bg;
                    u8 k;
                    for (k = 0; k < scale; k++) {          /* 横 scale 倍。2px=1byteに詰める */
                        if (!have) { pending = (u8)(col << 4); have = 1; }
                        else       { VDP_DAT = (u8)(pending | col); have = 0; }
                    }
                }
            }
        }
        px += (u8)(8 * scale);
    }
}

/* ===== スクロール =====
   R#23 縦スクロールは VRAM 全体(スプライト含む)を縦シフトする。g_vscroll に量を保持し、
   vdp_sprite_pos が Y へ加算 → 縦スクロール中もスプライトを画面固定に見せる。
   R#26/27 横スクロール(蛇行)はスプライトに影響しないため補正不要。 */
static u8 g_vscroll;   /* 現在の縦スクロール量(sprite_pos がYに加算) */

void vdp_set_vscroll(u8 v) {
    g_vscroll = v;
    vdp_wreg(23, v);
}

void vdp_set_hscroll(u8 coarse, u8 fine) {
    vdp_wreg(26, coarse & 0x3F);   /* 8px単位の粗スクロール */
    vdp_wreg(27, fine & 0x07);     /* 0-7 の微スクロール    */
}

/* SCREEN5 表示ページ。R#2 = 0x1F | (page<<5)。page0=0x1F(base 0x0000)/page1=0x3F(base 0x8000)。 */
void vdp_set_display_page(u8 page) {
    vdp_wreg(2, (u8)(0x1F | ((page & 3) << 5)));
}

/* ===== スプライト(mode2, 16x16) =====
   SCREEN5 の BIOS 既定テーブル配置を使う(C-BIOS/実機共通): 属性0x7600/色0x7400/パターン0x7800。
   R#5/6/11 は CHGMOD(5) が既定値に設定済みなので触らない(相対再配置は環境差で不確実)。 */
#define SPR_ATTR  0x7600   /* 属性表(4B/枚: Y,X,pattern,予約) */
#define SPR_COLOR 0x7400   /* 色表(16B/枚: 行ごとの色)         */
#define SPR_PAT   0x7800   /* パターン生成表(8B単位)           */

/* R#1 の size ビットを立て 16x16 に(mag=0)。RG1SAV(0xF3E0)経由で他ビット保持。 */
static void set_sprite16(void) {
    __asm
        di
        ld   a, (0xF3E0)
        or   #0x02          ; size=16x16
        and  #0xFE          ; mag=0
        ld   (0xF3E0), a
        out  (0x99), a
        ld   a, #0x81       ; R#1
        out  (0x99), a
        ei
    __endasm;
}

void vdp_sprite_init(void) {
    u8 i;
    set_sprite16();
    for (i = 0; i < 32; i++) {             /* 全スプライトを画面外へ */
        vdp_write_addr(SPR_ATTR + i * 4);
        VDP_DAT = 216;                     /* Y=216(画面下=不可視) */
    }
}

void vdp_sprite_pattern(u8 patnum, const u8 *d32) {
    u8 i;
    vdp_write_addr(SPR_PAT + (u16)patnum * 8);
    for (i = 0; i < 32; i++) VDP_DAT = d32[i];
}

void vdp_sprite_color(u8 slot, u8 color) {
    u8 i;
    vdp_write_addr(SPR_COLOR + (u16)slot * 16);
    for (i = 0; i < 16; i++) VDP_DAT = color;
}

void vdp_sprite_pos(u8 slot, u8 x, u8 y, u8 patnum) {
    vdp_write_addr(SPR_ATTR + (u16)slot * 4);
    /* 表示Y=属性Y+1 のため -1。縦スクロール量を足して画面固定に補正。 */
    VDP_DAT = (u8)(y + g_vscroll - 1);
    VDP_DAT = x;
    VDP_DAT = patnum;
    VDP_DAT = 0;
}

void vdp_sprite_hide_from(u8 slot) {
    vdp_write_addr(SPR_ATTR + (u16)slot * 4);
    VDP_DAT = 216;           /* Y=216(0xD8)=212ライン時の停止マーカ。以降のスプライトを非表示
                                (208=0xD0は192ライン用。212ラインでは終端にならず古い残像が残る) */
}
