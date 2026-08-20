/* ship.c — 戦艦の事前描画レンダラ(旧版 BattleshipProto の艦システムを忠実移植)。ship.h 参照。
   上面視の艦を バッファB(=SC_SHIPBUF_Y, page2/3)へ一度だけ描く。艦高 496px(ship-local y 0..495)。
   物理VRAM Y = B + shipY。中心 x=128。塗りは旧版の高速 lmmv(R#17 間接連番書込)を移植。
   色は旧版の艦用パレット(sea1/2, 甲板6, 影7=SHADOWC, 灰ランプ 13→5→4→14→15, オリーブ9, 橙12)。 */
#include "ship.h"
#include "scroll.h"  /* SC_SHIPBUF_Y = バッファB基準Y */
#include "vdp.h"     /* vdp_copy(海テンプレのタイル) */

__sfr __at(0x98) SH_DAT;     /* VRAM データ */
__sfr __at(0x99) SH_CTRL;    /* VDP アドレス/レジスタ */
__sfr __at(0x9B) SH_IDAT;    /* R#17 間接オートインクリメント */

#define B         SC_SHIPBUF_Y   /* 528。ship-local y に足して絶対VRAM Y にする(旧版 B) */
#define SHADOWC   7              /* 船体ドロップシャドウ(暗青) */
#define NAAG      23             /* 対空砲マウント数 */

static u8 g_hull;   /* hull_w プロファイル選択(HULL_BISMARCK/HULL_IOWA) */

/* ---- 高速VDPコマンド完了待ち(S#2を1回選び di 中でタイトにポーリング→S#0へ戻す)。
   艦描画は BGM停止中(カード)に一括で行うので長め di でも音に影響しない=vdp_cmd_wait より速い。 ---- */
static void swait(void) {
    __asm
        di
        ld   a, #2
        out  (0x99), a
        ld   a, #0x8F      ; R#15=2 (S#2 選択)
        out  (0x99), a
    00011$:
        in   a, (0x99)
        rra                ; CE -> Carry
        jr   c, 00011$
        ld   a, #0
        out  (0x99), a
        ld   a, #0x8F      ; R#15=0 (S#0 へ戻す)
        out  (0x99), a
        ei
    __endasm;
}

/* ---- 高速矩形塗り(LMMV, R#17間接)。dy は絶対VRAM Y(=B+y を呼び元で足す)。 ---- */
static void sfill(u16 dx, u16 dy, u16 nx, u16 ny, u8 c) {
    swait();
    __asm di __endasm;
    SH_CTRL = 36; SH_CTRL = 0x80 | 17;               /* R#17=36(以降 R#36.. 自動増) */
    SH_IDAT = (u8)(dx & 0xFF); SH_IDAT = (u8)(dx >> 8);   /* DX */
    SH_IDAT = (u8)(dy & 0xFF); SH_IDAT = (u8)(dy >> 8);   /* DY(絶対) */
    SH_IDAT = (u8)(nx & 0xFF); SH_IDAT = (u8)(nx >> 8);   /* NX */
    SH_IDAT = (u8)(ny & 0xFF); SH_IDAT = (u8)(ny >> 8);   /* NY */
    SH_IDAT = c; SH_IDAT = 0; SH_IDAT = 0x80;            /* CLR, ARG, CMD=LMMV(0x80) */
    __asm ei __endasm;
}

/* ---- 直接1画素(read-modify-write。コマンドエンジンを使わない=斑点で高速)。dy=絶対VRAM Y(17bit)。
   byte addr = dy*128 + x/2。R#14=(addr>>14)&7=(dy>>7)&7。低14bit=A0-7/A8-13。SCREEN5=2px/byte。 ---- */
static void vpset(u16 dy, u16 x, u8 c) {
    u8 r14  = (u8)((dy >> 7) & 7);
    u8 alo  = (u8)(((dy & 1) << 7) | (x >> 1));   /* A0-7: dy*128 の bit7=(dy&1)<<7, +x/2 */
    u8 amid = (u8)((dy >> 1) & 0x3F);             /* A8-13: (dy*128)>>8 = dy>>1 */
    u8 b;
    __asm di __endasm;
    SH_CTRL = r14;  SH_CTRL = 0x80 | 14;          /* R#14 = A14-A16 */
    SH_CTRL = alo;  SH_CTRL = amid;               /* 読み(bit6=0) */
    b = SH_DAT;
    if (x & 1) b = (u8)((b & 0xF0) | c); else b = (u8)((b & 0x0F) | (u8)(c << 4));
    SH_CTRL = r14;  SH_CTRL = 0x80 | 14;
    SH_CTRL = alo;  SH_CTRL = (u8)(amid | 0x40);  /* 書き(bit6=1) */
    SH_DAT = b;
    __asm ei __endasm;
}
static void spset(u16 x, u16 dy, u8 c) { vpset(dy, x, c); }

/* ---- 疑似乱数(旧版と同一LCG。斑点の再現性のため呼び順も一致させる) ---- */
static u16 rng = 12345;
static u8 rnd(void) { rng = (u16)(rng * 25173 + 13849); return (u8)(rng >> 8); }

/* ---- 整数sqrt(乗算なし。円の各行幅) ---- */
static u8 isqrt(u16 n) { u8 r = 0; u16 sq = 1; while (sq <= n) { r++; sq += (u16)(2 * r + 1); } return r; }

/* ---- 塗り円(cy はship-local。B は内部で足す) ---- */
static void disk(s16 cx, s16 cy, s16 rr, u8 c) {
    s16 dy, w;
    for (dy = -rr; dy <= rr; dy++) {
        w = (s16)isqrt((u16)(rr * rr - dy * dy)) * 2;
        if (w > 0) sfill((u16)(cx - w / 2), (u16)(B + cy + dy), (u16)w, 1, c);
    }
}

/* ---- 陰影ドーム(左上光源。外周暗→内側白の5枚) ---- */
static void dome(s16 cx, s16 cy, s16 rr) {
    s16 t;
    disk(cx, cy, rr, 13);
    disk(cx, cy, rr - 1, 5);
    disk(cx - 1, cy - 1, rr - 2, 4);
    disk(cx - 1, cy - 1, rr - 4, 14);
    t = rr - 6; if (t < 1) t = 1;
    disk(cx - 2, cy - 2, t, 15);
}

/* ---- 主砲塔(ベースリング＋ドーム。砲身は別スプライト) ---- */
static void mainGun(s16 cx, s16 cy, s16 rr) {
    disk(cx, cy, rr + 2, 13);
    disk(cx, cy, rr + 1, 5);
    dome(cx, cy, rr);
}
/* ---- 対空砲(暗ベース＋ドーム) ---- */
static void aaGun(s16 x, s16 cy, s16 rr) { disk(x, cy, rr + 1, 13); dome(x, cy, rr); }
/* ---- 影(暗diskを +4+6 ずらし) ---- */
static void ground(s16 cx, s16 cy, s16 rr) { disk(cx + 4, cy + 6, rr, 13); }

/* ---- 円筒陰影の砲身(縦L・幅w。中心白→端灰) ---- */
static void barrel(s16 cx, s16 y, s16 L, s16 w) {
    s16 i, dd, t; u8 c;
    for (i = 0; i < w; i++) {
        dd = i - (w - 1) / 2; if (dd < 0) dd = -dd;
        t = (s16)(dd * 100 / ((w + 1) / 2));
        c = (t < 30) ? 15 : (t < 60) ? 14 : (t < 85) ? 4 : 5;
        sfill((u16)(cx - w / 2 + i), (u16)(B + y), 1, (u16)L, c);
    }
}

/* ---- 金属斑点(2px格子, ~21%で 4/13 を点描) ---- */
static void metalNoise(s16 x, s16 y, s16 w, s16 h) {
    s16 nx, ny; u8 r;
    for (ny = y; ny < y + h; ny += 2)
        for (nx = x; nx < x + w; nx += 2) { r = rnd(); if (r < 55) spset((u16)nx, (u16)(B + ny), (u8)((r & 1) ? 4 : 13)); }
}

/* ---- 3D金属ボックス(左上光源。影→本体→斑点→ハイライト→影縁) ---- */
static void deckBox(s16 x, s16 y, s16 w, s16 h, u8 fill) {
    sfill((u16)(x + 3), (u16)(B + y + 4), (u16)w, (u16)h, 13);      /* 影(右下) */
    sfill((u16)x, (u16)(B + y), (u16)w, (u16)h, fill);             /* 本体 */
    metalNoise(x + 1, y + 1, w - 2, h - 2);                         /* 内部斑点 */
    sfill((u16)x, (u16)(B + y), (u16)w, 1, 15);                     /* 上縁=白 */
    sfill((u16)x, (u16)(B + y), 1, (u16)h, 14);                     /* 左縁=淡 */
    sfill((u16)x, (u16)(B + y + h - 1), (u16)w, 1, 13);             /* 下縁=暗 */
    sfill((u16)(x + w - 1), (u16)(B + y), 1, (u16)h, 13);           /* 右縁=暗 */
}

/* ---- 船体半幅プロファイル(y=ship-local)。整数除算は切り捨て(旧版と一致) ---- */
static s16 hull_w(s16 y) {
    if (g_hull == 1) {                                    /* フッド(細い艦体 半幅46/鋭い艦首) */
        if (y < 44)  return (s16)(5 + (41 * y) / 44);
        if (y < 436) return 46;
        if (y < 474) return (s16)(46 - (16 * (y - 436)) / 38);
        return (s16)(30 - (5 * (y - 474)) / 22);
    }
    if (g_hull == 2) {                                    /* 双子(ネルソン級, 半幅24/丸い艦首=2隻並べる) */
        if (y < 28)  return (s16)(6 + (18 * y) / 28);
        if (y < 440) return 24;
        if (y < 476) return (s16)(24 - (10 * (y - 440)) / 36);
        return (s16)(14 - (3 * (y - 476)) / 20);
    }
    if (g_hull == HULL_IOWA) {                            /* アイオワ */
        if (y < 50)  return (s16)(5 + (45 * y) / 50);
        if (y < 448) return 50;
        if (y < 486) return (s16)(50 - (20 * (y - 448)) / 38);
        return (s16)(30 - (4 * (y - 486)) / 9);
    }
    /* HULL_BISMARCK(既定) */
    if (y < 36)  return (s16)(9 + (45 * y) / 36);
    if (y < 400) return 54;
    if (y < 462) return (s16)(54 - (21 * (y - 400)) / 62);
    return (s16)(33 - (3 * (y - 462)) / 34);
}

/* ---- 空母の飛行甲板プロファイル(フラット幅広, 半幅58) ---- */
static s16 carrier_w(s16 y) {
    if (y < 28)  return (s16)(20 + (38 * y) / 28);
    if (y < 452) return 58;
    if (y < 490) return (s16)(58 - (26 * (y - 452)) / 38);
    return 32;
}

/* ---- 船体本体(走査線ごとの甲板＋縁陰影＋縦通材＋甲板斑点＋レール刻み＋ドロップシャドウ) ---- */
static void paint_hull_at(s16 cx) {
    s16 y, y2, h, w, i, ys, ws, nx, ny; u8 r;
    /* (1)ドロップシャドウ(下10右14, 色7)。ws(=hull_w(y-10))が同一の連続行を1矩形にまとめる(高速化)。 */
    y = 0;
    while (y < 496) {
        ys = y - 10; ws = (ys < 0) ? -1 : hull_w(ys);
        y2 = y + 1;
        while (y2 < 496) { s16 w2 = ((y2 - 10) < 0) ? -1 : hull_w(y2 - 10); if (w2 != ws) break; y2++; }
        if (ws >= 0) sfill((u16)(cx - ws + 14), (u16)(B + y), (u16)(ws * 2), (u16)(y2 - y), SHADOWC);
        y = y2;
    }
    /* (2)甲板本体＋左右縁の陰影。w 同一の連続行を1矩形(縦帯)にまとめる=同一画素のまま高速化。 */
    y = 0;
    while (y < 496) {
        w = hull_w(y);
        y2 = y + 1;
        while (y2 < 496 && hull_w(y2) == w) y2++;
        h = y2 - y;
        sfill((u16)(cx - w), (u16)(B + y), (u16)(w * 2), (u16)h, 6);
        sfill((u16)(cx - w), (u16)(B + y), 1, (u16)h, 14);
        sfill((u16)(cx - w + 1), (u16)(B + y), 2, (u16)h, 12);
        sfill((u16)(cx + w - 3), (u16)(B + y), 2, (u16)h, 9);
        sfill((u16)(cx + w - 1), (u16)(B + y), 1, (u16)h, 13);
        y = y2;
    }
    for (i = -8; i <= 8; i++) {                        /* (3)縦通材17本(x=cx+i*6, 色9) */
        w = i * 6; if (w < 0) w = -w;
        ys = -1; ws = 0;
        for (y = 6; y < 490; y++) if (hull_w(y) - 3 > w) { if (ys < 0) ys = y; ws = y; }
        if (ys >= 0) sfill((u16)(cx + i * 6), (u16)(B + ys), 1, (u16)(ws - ys + 1), 9);
    }
    for (ny = 8; ny < 488; ny += 2) {                 /* (4)甲板斑点(12橙/9) */
        w = hull_w(ny);
        if (w < 14) continue;
        for (nx = (s16)(cx - w + 4); nx < (s16)(cx + w - 4); nx += 2) {
            r = rnd();
            if (r < 80) spset((u16)nx, (u16)(B + ny), (u8)((r & 1) ? 12 : 9));
        }
    }
    for (y = 44; y < 466; y += 5) {                   /* (5)舷側レール刻み(暗13) */
        ws = hull_w(y);
        sfill((u16)(cx - ws + 3), (u16)(B + y), 2, 1, 13);
        sfill((u16)(cx + ws - 5), (u16)(B + y), 2, 1, 13);
    }
}

/* ---- 波切り艦首シェブロン(暗13の V が頂点から広がる) ---- */
static void draw_bow(s16 cx, u8 cnt, u16 yb) {
    u8 i;
    for (i = 0; i <= cnt; i++) {
        sfill((u16)(cx - i), (u16)(B + yb + i / 2), 1, 2, 13);
        sfill((u16)(cx + i), (u16)(B + yb + i / 2), 1, 2, 13);
    }
}

/* ---- 対空砲23基(艦種別配置 bb=0/cv=1/hd=2/nl=3)。前14=大径(gb/ab), 後9=小径(gs/as) ---- */
static const u8  aag_x_bb[NAAG] = { 108,148,88,168,88,168,88,168,88,168,88,168,88,168,110,146,110,146,110,146,118,138,128 };
static const u16 aag_y_bb[NAAG] = { 196,196,150,150,178,178,206,206,240,240,280,280,308,308,232,232,282,282,300,300,34,34,452 };
static const u8  aag_x_cv[NAAG] = { 74,182,74,182,74,182,74,182,74,182,74,182,74,182,74,182,144,144,128,100,156,128,172 };
static const u16 aag_y_cv[NAAG] = { 62,62,106,106,150,150,194,194,238,238,282,282,326,326,370,370,140,220,22,442,442,448,405 };
static const u8  aag_x_hd[NAAG] = { 100,156,100,156,100,156,100,156,100,156,100,156,100,156,128,118,138,118,138,128,118,138,128 };
static const u16 aag_y_hd[NAAG] = { 145,145,180,180,215,215,250,250,285,285,315,315,345,345,195,235,235,270,270,330,388,388,452 };
static const u8  aag_x_nl[NAAG] = { 62,90,62,90,62,90,76,166,194,166,194,166,194,180,76,76,62,90,180,180,166,194,180 };
static const u16 aag_y_nl[NAAG] = { 140,140,220,220,300,300,180,140,140,220,220,300,300,180,110,260,340,340,110,260,340,340,380 };
static void draw_aag(u8 tbl, u8 gb, u8 gs, u8 ab, u8 as) {
    const u8 *ax; const u16 *ay; u8 i;
    switch (tbl) {
        case 1:  ax = aag_x_cv; ay = aag_y_cv; break;
        case 2:  ax = aag_x_hd; ay = aag_y_hd; break;
        case 3:  ax = aag_x_nl; ay = aag_y_nl; break;
        default: ax = aag_x_bb; ay = aag_y_bb; break;
    }
    for (i = 0; i < NAAG; i++) {
        ground((s16)ax[i], (s16)ay[i], (i < 14) ? gb : gs);
        aaGun ((s16)ax[i], (s16)ay[i], (i < 14) ? ab : as);
    }
}

/* ---- 艦OPS(7B/レコード {op,x,ylo,yhi,p1,p2,p3}, op=0終端)を解釈 ---- */
static void run_ship_ops(const u8 *d) {
    for (;;) {
        u8 op = d[0], p1, p2, p3; s16 x; u16 y;
        if (op == SOP_END) break;
        x = (s16)d[1]; y = (u16)(d[2] | (d[3] << 8)); p1 = d[4]; p2 = d[5]; p3 = d[6]; d += 7;
        switch (op) {
            case SOP_GROUND:  ground(x, (s16)y, p1);                 break;
            case SOP_MAINGUN: mainGun(x, (s16)y, p1);                break;
            case SOP_DOME:    dome(x, (s16)y, p1);                   break;
            case SOP_DISK:    disk(x, (s16)y, p1, p2);               break;
            case SOP_DECKBOX: deckBox(x, (s16)y, p1, p2, p3);        break;
            case SOP_AAGUN:   aaGun(x, (s16)y, p1);                  break;
            case SOP_LMMV:    sfill((u16)x, (u16)(B + y), p1, p2, p3); break;  /* Yは絶対=B+y */
            case SOP_BARREL:  barrel(x, (s16)y, p1, p2);             break;
        }
    }
}

/* ---- 空母の飛行甲板(舷側影→木甲板+光/影縁→板目→甲板ノイズ→センターライン破線)。コード生成。 ---- */
static void carrier_deck(void) {
    s16 y, y2, w, i, nx, ny; u16 h; u8 r;
    y = 4;                                          /* 舷側影(±(w+2), SHADOWC)。同幅帯まとめ */
    while (y < 494) {
        w = carrier_w(y - 4);
        y2 = y + 1; while (y2 < 494 && carrier_w(y2 - 4) == w) y2++;
        sfill((u16)(128 - w - 2), (u16)(B + y), (u16)((w + 2) * 2), (u16)(y2 - y), SHADOWC);
        y = y2;
    }
    y = 0;                                          /* 飛行甲板(木6)+左舷光(14,15)/右舷影(13,9) */
    while (y < 496) {
        w = carrier_w(y);
        y2 = y + 1; while (y2 < 496 && carrier_w(y2) == w) y2++;
        h = (u16)(y2 - y);
        sfill((u16)(128 - w), (u16)(B + y), (u16)(w * 2), h, 6);
        sfill((u16)(128 - w), (u16)(B + y), 1, h, 14);
        sfill((u16)(128 - w + 1), (u16)(B + y), 1, h, 15);
        sfill((u16)(128 + w - 1), (u16)(B + y), 1, h, 13);
        sfill((u16)(128 + w - 2), (u16)(B + y), 1, h, 9);
        y = y2;
    }
    for (i = -3; i <= 3; i++) if (i) sfill((u16)(128 + i * 15), (u16)(B + 28), 1, 424, 9);   /* 板目 */
    for (ny = 8; ny < 488; ny += 2) {               /* 甲板ノイズ(9/14) */
        w = carrier_w(ny); if (w < 14) continue;
        for (nx = (s16)(128 - w + 3); nx < (s16)(128 + w - 3); nx += 2)
            { r = rnd(); if (r < 70) spset((u16)nx, (u16)(B + ny), (u8)((r & 1) ? 9 : 14)); }
    }
    for (y = 34; y < 452; y += 14) sfill(127, (u16)(B + y), 2, 8, 15);   /* センターライン破線(白) */
}

void ship_render(u8 kind, u8 hull, u8 bow_cnt, u16 bow_yb, u8 aag_tbl, const u8 *aagp, const u8 *ops, const u8 *ops2) {
    u8 r;
    g_hull = hull;
    rng = 12345;                                   /* 斑点を毎回同一に(決定的) */
    for (r = 0; r < SC_SHIP_ROWS; r++)             /* 海テンプレ(512)を31行タイルして下地に */
        vdp_copy(0, SC_SEATMPL_Y, 0, (u16)(B + (u16)r * 16), 256, 16);
    if (kind == 2) {                               /* 空母: 飛行甲板→carrier_ops→島の金属ノイズ→carrier2_ops */
        carrier_deck();
        run_ship_ops(ops);
        metalNoise(140, 150, 36, 42);
        run_ship_ops(ops2);
    } else if (kind == 1) {                        /* 双子: 2隻ぶんの船体+艦首、上構は1回のOPS(絶対X) */
        paint_hull_at(76);  draw_bow(76, bow_cnt, bow_yb);
        paint_hull_at(180); draw_bow(180, bow_cnt, bow_yb);
        run_ship_ops(ops);
    } else {                                       /* 単艦(BB/Iowa/Hood) */
        paint_hull_at(128); draw_bow(128, bow_cnt, bow_yb);
        run_ship_ops(ops);
    }
    draw_aag(aag_tbl, aagp[0], aagp[1], aagp[2], aagp[3]);   /* 対空砲23基(艦種別配置) */
}
