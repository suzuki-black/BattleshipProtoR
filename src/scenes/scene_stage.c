/* scene_stage.c — ★1本の連続縦スクロール面(海→戦艦 地続き。画面カット無し)。
   フェーズ0: 海=蛇行なしの直進スクロール(前進)。戦艦の船尾が見えたら交戦へ。
   フェーズ1: 戦艦=船首↔船尾の往復蛇行スクロール(縦の往復＋横揺れ weaveX)＝前作の戦艦戦。
   自機は下部固定(scroll側のY補正＋weaveXは自機に非適用)。砲塔/戦闘機/撃破は次段で統合。 */
#include "scene.h"
#include "vdp.h"
#include "entity.h"
#include "sprites.h"
#include "scroll.h"
#include "ship.h"          /* 旧版忠実の艦レンダラ */
#include "fire.h"
#include "sound.h"
#include "hud.h"
#include "gamestate.h"
#include "input.h"
#include "bank.h"          /* data_read(艦体OPSをバンク→RAM) */
#include "assets_data.h"   /* 自動生成: ship_ops_off/len, ship_hull/bowcnt/bowyb, SHIP_OPS_RAM_MAX, ASSET_BANK */

/* ★艦(上面視, 496px)は旧版 BattleshipProto の艦システムを忠実移植(ship.c)。海テンプレを下地に
   paint_hull＋波切り艦首＋主砲/艦橋/煙突OPS＋対空砲23基を バッファB(SC_SHIPBUF_Y=528)へ事前描画。
   前部Anton/Bruno(超越)＋後部Cäsar/Dora の4主砲は破壊可能スプライト(ET_TURRET)。全撃破でクリア。 */

/* 主砲の発砲: 50f毎に自機狙い4-way散弾(偶数=自機直線上に隙間)。
   ★suppress=24: 半径内(≒ゼロ距離)に自機が居ると発射スキップ=肉薄で撃たせない。難易度で半径増減。 */
static const u8 fd_gun[]  = { 50, 24, FIRE_AIMFAN, 4, 2, 3, FIRE_END };
/* 戦闘機の発砲: 45f毎に自機狙い＋散らし円錐(±3)。空中の的なので抑え込みは無し(suppress=0)。 */
static const u8 fd_faim[] = { 45,  0, FIRE_AIMED, 3, 1, 2, FIRE_END };

static u16 rng;
static u8  rnd(void) { rng = rng * 25173 + 13849; return (u8)(rng >> 8); }

static u8 curstage;   /* 現在の面(0..STAGE_COUNT-1)。stage_init が g_stage_sel から設定 */
/* 面ごとの艦名(結果画面用)。艦体OPS/長さは assets_data.h の ship_*_off/len[curstage]。 */
static const char *const stagename[STAGE_COUNT] = { "BISMARCK", "IOWA", "CARRIER", "HOOD", "TWINS" };

/* 主砲塔(破壊可能)を艦上の世界座標に配置。艦中心 x=120(sprite左上→中心128)。世界Y=艦頭(SC_SHIP_R0*16)+艦内y。
   hp=3: 抑え込み(肉薄で撃たせない)で安全に連射しないと落としにくい=「ゼロ距離抑え込み=最速撃破」を要求。 */
static void spawn_turret(u8 shipX, u16 shipY, u8 delay) {
    Entity *e = ent_spawn(ET_TURRET);
    if (e) {
        e->ax = (s16)shipX - 8;                /* 砲塔中心x→スプライト左上 */
        e->ay = (s16)(SC_SHIP_R0 * 16 + shipY);
        e->color = 5; e->pat = SPR_TURRET; e->hp = 3; e->fire = fd_gun; e->ftimer = delay;
    }
}
/* 各面の主砲4基の艦内(x,y)=OPSの主砲位置。順=BB/Iowa/Carrier/Hood/Twins。空母/双子はx左右に分かれる。 */
static const u8  gun_x[STAGE_COUNT][4] = {
    {128,128,128,128}, {128,128,128,128}, {95,161,95,161}, {128,128,128,128}, {76,76,180,180}
};
static const u16 gun_y[STAGE_COUNT][4] = {
    {64,104,322,362}, {72,108,330,372}, {100,100,300,300}, {70,108,372,410}, {80,360,80,360}
};

/* 戦艦を バッファB へ事前描画(旧版忠実)。海テンプレ→OPS(+ops2)をRAMへ読み ship_render(艦種別)。
   ★重い(数千VDP塗り)ので、Bに既に現在の艦が居るなら再生成しない(ミス再挑戦=即再開)。 */
static u8 ship_ram[SHIP_OPS_RAM_MAX];
static u8 ship_ram2[128];        /* ops2(空母のみ, max85) */
static s8 rendered_stage = -1;   /* バッファBに描画済みの面(-1=未) */
static void prerender_ship(void) {
    if (rendered_stage == (s8)curstage) return;   /* 既に描画済み=再生成不要 */
    scroll_build_sea();                        /* 海テンプレート(512) */
    data_read(ASSET_BANK, ship_ops_off[curstage],  ship_ram,  ship_ops_len[curstage]);
    data_read(ASSET_BANK, ship_ops2_off[curstage], ship_ram2, ship_ops2_len[curstage]);
    ship_render(ship_kind[curstage], ship_hull[curstage], ship_bowcnt[curstage], ship_bowyb[curstage],
                ship_aagtbl[curstage], ship_aagp[curstage], ship_ram, ship_ram2);
    rendered_stage = (s8)curstage;
}

#define WMAX 40   /* 横揺れ(weaveX)の振幅 */

static u16 cam;
static u8  phase;     /* 0=海(直進) / 1=戦艦(往復蛇行) */
static u8  sdiv, wtimer, ftick;
static s8  camdir;    /* 往復方向: -1=船首へ / +1=船尾へ */
static s16 weaveX;
static s8  wdir;
static u8  dmode;     /* 撃破演出モード(0=通常 / 1=炎上スペクタクル中) */
static u8  dtimer;    /* 撃破演出の残フレーム */
static u8  raided;    /* 1=戦艦出現時に空襲(戦闘機/敵弾)を一掃済み */

/* weaveX を横HWスクロール(R#26/27)へ。滑らかな左寄せは R#26=ceil(s/8)/R#27=(8-frac)。 */
static void apply_weave(void) {
    u8 s = (u8)((256 - (weaveX & 0xFF)) & 0xFF);
    u8 frac = (u8)(s & 7);
    vdp_set_hscroll((u8)(((s >> 3) + (frac ? 1 : 0)) & 0x1F), (u8)((8 - frac) & 7));
    g_meander = weaveX;   /* 砲塔スプライトの横追従(次段の砲塔統合で使用) */
}

/* 設定の残機初期値(config g_lives_idx→2/3/5)。 */
static u8 lives_init(void) {
    static const u8 t[3] = { 2, 3, 5 };
    return t[(g_lives_idx < 3) ? g_lives_idx : 1];
}

/* 1回の挑戦のレイアウトを「表示を切替えず」構築(艦はオフスクリーンのバッファBへ事前描画)。
   ★これを開始カード表示中に呼ぶことで、カードの裏でステージ準備が進む(旧版と同じ)。
   スコア/残機は触らない(それらは新規ゲーム=stage_init が初期化)。 */
static void stage_build(void) {
    Entity *e;
    prerender_ship();     /* 艦をバッファB(オフスクリーン)へ。stage_begin_display の前に必須 */
    vdp_sprite_init();
    sprites_load();
    hud_init();           /* 数字パターン投入＋HUDスロット確保(g_spr_base) */
    ent_reset();
    cam = SC_CAM_START; phase = 0; sdiv = 0; wtimer = 0; ftick = 0;
    weaveX = 0; wdir = 1; camdir = -1; g_meander = 0; rng = 0x1234;

    e = ent_spawn(ET_PLAYER);
    if (e) { e->x = 120; e->y = 176; e->color = 15; e->pat = SPR_BLOCK; }

    g_php = g_durability ? g_durability : 1;   /* 1機あたりの耐久HP(設定) */
    g_pinv = 0;
    g_miss = 0;
    dmode = 0; dtimer = 0; raided = 0;

    /* 破壊可能主砲塔(ビスマルク配置=前2/後2。海フェーズ中は画面外)。全撃破でクリア。
       艦内Yは ship_top/ship_bot のマウント位置と一致(前:72/108, 後:300/344)。 */
    g_gun_kills = 0;
    spawn_turret(gun_x[curstage][0], gun_y[curstage][0], 30);
    spawn_turret(gun_x[curstage][1], gun_y[curstage][1], 45);
    spawn_turret(gun_x[curstage][2], gun_y[curstage][2], 60);
    spawn_turret(gun_x[curstage][3], gun_y[curstage][3], 75);
}

/* 地形リングを表示(page1)＝ここでゲーム画面が現れる。scroll_init は prerender 済みが前提。 */
static void stage_begin_display(void) {
    scroll_init();
    sea_init(curstage);          /* 艦種別の海コラム帯を選択 */
    vdp_set_hscroll(0, 0);
}

/* ミス再挑戦: 開始カード/ファンファーレ無しで即再構築(BGMは鳴りっぱなしのまま面最初から)。 */
static void stage_setup(void) {
    stage_build();
    stage_begin_display();
}

/* 面別BGM(gen_assets のトラック順: 0=title/1=BBマーチ/2=ED/3=空母/4=フッド哀歌/5=双子/6=Iowa)。
   順=BB/Iowa/Carrier/Hood/Twins。 */
static const u8 stage_bgm[STAGE_COUNT] = { 1, 6, 3, 4, 5 };

/* ステージ開始シーケンス(旧版準拠)。各面の開始時(新規ゲーム/次面へ)にだけ実行=ミス再挑戦では出さない。
   手順: BGM停止(無音) → カード(STAGE n/TARGET/艦名/シルエット)を page0 に描く
        → ★カードの裏でステージ準備(stage_build=艦の事前描画・砲台配線をオフスクリーンで)
        → 開始ファンファーレ(BGM無音でこれだけ鳴る) → 余韻
        → メインBGM開始 → 地形を表示(stage_begin_display)＝ゲーム開始。
   艦名は可変長なので中央寄せ(8px/char)。 */
static void stage_intro(void) {
    const char *nm = stagename[curstage];
    u8 f, n = 0;
    char num[2];
    while (nm[n]) n++;                       /* 艦名の長さ(中央寄せ用) */

    bgm_stop();                              /* カード中は無音(タイトル/前面のBGMを止める) */
    vdp_set_vscroll(0);                      /* 縦スクロール解除(page0のズレ防止) */
    vdp_sprite_hide_from(0);                 /* スプライト全消し */
    vdp_set_display_page(0);
    vdp_fill(0, 0, 256, 212, 1);            /* 背景=海の濃紺(旧パレット色1=1,4,5)。装飾ラインは無し */

    /* 見出しは2倍角(旧版準拠)。STAGE n / - TARGET - / 艦名 を中央寄せで大きく。地色=背景(1)。 */
    num[0] = (char)('1' + curstage); num[1] = 0;
    vdp_text_s(72, 12, 15, 1, 2, "STAGE");         /* "STAGE"(5字×16=80) 72..152。白で視認性確保 */
    vdp_text_s(168, 12, 15, 1, 2, num);            /* n は空白1つ空けて 168 */
    vdp_text_s(48, 32, 11, 1, 2, "- TARGET -");    /* 10字×16=160 → x48 中央(赤) */

    /* 枠無し: 船影(実艦BG)は stage_build 後に背景へ直接コピーする(下地/白枠は描かない)。 */
    vdp_text_s((u8)(128 - n * 8), 156, 15, 1, 2, nm);   /* 艦名(2倍角・中央寄せ, 1字=16px) */

    stage_build();                          /* ★カードの裏でゲーム本体の艦をバッファBへ生成(重い) */
    vdp_copy(96, (u16)(SC_SHIPBUF_Y + 122), 96, 58, 64, 88);   /* 実艦Bの胴体中央を額縁へ(生成後に反映) */

    play_fanfare_open();                    /* 開始ファンファーレ(BGM無音でこれだけ鳴る) */
    for (f = 0; f < 40; f++) vdp_wait_frame();     /* 少し余韻(旧版と同じ40フレーム) */

    bgm_play(stage_bgm[curstage]);           /* 面別メインBGM開始 */
    stage_begin_display();                   /* 地形を表示=ゲーム開始 */
}

/* シーン入場(新規ゲーム): スコア/被弾/残機を初期化し、開始面(config選択)からレイアウト構築。 */
void stage_init(void) {
    g_score = 0;
    g_kills = 0; g_playerhit = 0;
    g_lives = lives_init();
    curstage = (g_stage_sel < STAGE_COUNT) ? g_stage_sel : 0;
    stage_intro();        /* 1面開始: カード＋ファンファーレ→準備→BGM→開始 */
}

/* スコアを5桁ゼロ詰め文字列へ(結果画面表示用)。 */
static char scorebuf[6];
static void fmt_score(u16 v) {
    u8 i;
    for (i = 5; i > 0; i--) { scorebuf[i - 1] = (char)('0' + (v % 10)); v /= 10; }
    scorebuf[5] = 0;
}

/* 撃破結果画面(page0)＋勝ちどきファンファーレ(前景・ブロッキング)。終わりにトリガ待ち。 */
static void results_and_fanfare(void) {
    u8 f;
    vdp_set_vscroll(0);                 /* 縦スクロール解除(page0テキストのズレ＋上端ゴミを防ぐ) */
    vdp_sprite_hide_from(0);            /* スプライト全消し(停止マーカを slot0 へ) */
    vdp_set_display_page(0);            /* 結果は非スクロールの page0 に描く */
    vdp_fill(0, 0, 256, 212, 1);        /* 黒地 */
    vdp_text(72,  60, 8,  1, "TARGET DESTROYED");
    vdp_text(96,  88, 15, 1, stagename[curstage]);
    fmt_score(g_score);
    vdp_text(72, 120, 15, 1, "SCORE");
    vdp_text(120, 120, 11, 1, scorebuf);
    play_fanfare();                     /* 勝ちどき(BGM停止・前景同期) */
    vdp_text(88, 168, 14, 1, "PUSH SPACE");
    for (f = 0; f < 180; f++) {         /* 約3秒 or トリガで次へ */
        input_poll();
        if (g_input_edge & INP_TRIG) break;
        vdp_wait_frame();
    }
}

/* 撃破演出(炎上スペクタクル): スクロール凍結・艦上へ爆発を降らせる＋轟音。尺が尽きたら結果へ。 */
static u8 defeat_update(void) {
    scroll_to(cam);                     /* 表示維持(cam凍結) */
    if ((dtimer & 3) == 0) ent_spawn_explosion((s16)(96 + (rnd() % 64)), (s16)(24 + (rnd() % 152)));
    if ((dtimer % 15) == 0) sfx(2, SFX_BOOM);
    ent_update_all();                   /* 爆発アニメを進める */
    ent_draw_all();
    if (dtimer) dtimer--;
    if (dtimer == 0) {
        results_and_fanfare();
        if (curstage + 1 < STAGE_COUNT) {   /* 次の面へ(スコア/残機は持ち越し) */
            curstage++;
            stage_intro();      /* 次面開始: カード＋ファンファーレ→準備→BGM→開始 */
            return SCENE_NONE;
        }
        return SC_ENDING;               /* 最終面クリア → エンディング */
    }
    return SCENE_NONE;
}

u8 stage_update(void) {
    if (dmode) return defeat_update();  /* 撃破演出中は専用処理 */

    if (phase == 0) {
        /* 海: 蛇行なしの直進。船尾が見えたら(=cam<=STERN)交戦フェーズへ地続きに移行 */
        if (++sdiv >= 2) { sdiv = 0; if (cam > SC_CAM_STERN) cam--; }
        scroll_to(cam);
        /* 空戦(イントロ)は「戦艦が未出現の開けた海」の間だけ。艦が入り始めたら空襲終了
           (でないと戦闘機が上端=艦の上に突然湧いてゴミに見える。HANDOFF §2: 空戦→戦艦)。 */
        if (cam > SC_CAM_SHIP && (++ftick % 40) == 0) {
            Entity *f = ent_spawn(ET_FIGHTER);
            if (f) {
                f->x = 24 + (rnd() % 200); f->y = -16;
                f->vx = (rnd() & 1) ? 1 : -1; f->vy = 2 + (rnd() % 2);
                f->color = 11; f->pat = SPR_FIGHTER;   /* 赤(旧パレット11) */
                if (rnd() & 1) { f->fire = fd_faim; f->ftimer = 20 + (rnd() % 30); }
            }
            sfx(1, SFX_HIT);
        }
        /* 戦艦が出現した瞬間に、残っている空襲(戦闘機/敵弾)を一掃(艦の手前に居残るゴミ防止)。 */
        if (cam <= SC_CAM_SHIP) {   /* 戦艦出現後は空襲を退かせる: 初回に戦闘機＋敵弾を一掃、以後は戦闘機のみ毎フレーム掃除 */
            if (!raided) { raided = 1; ent_clear_enemies(); sea_set_ship(curstage); } else ent_clear_fighters();
        }
        if (cam <= SC_CAM_STERN) { phase = 1; camdir = -1; }
    } else {
        /* 戦艦: 船首↔船尾の往復(縦) */
        if (++sdiv >= 2) {
            sdiv = 0;
            cam = (u16)((s16)cam + camdir);
            if (cam <= SC_CAM_BOW)   camdir = 1;
            if (cam >= SC_CAM_STERN) camdir = -1;
        }
        scroll_to(cam);
        /* 蛇行(横揺れ) */
        if (++wtimer >= 3) {
            wtimer = 0;
            weaveX += wdir;
            if (weaveX >= WMAX)  wdir = -1;
            if (weaveX <= -WMAX) wdir = 1;
        }
        apply_weave();
    }

    /* HUD は R#23(縦スクロール)設定直後・エンティティ描画より前に確定させる。
       画面最上部のHUDは最もラスタ競合しやすく、重い ent_draw_all の後に書くと
       ラスタが既に上端を通過→R#23とズレて1px上下振動する(旧版で残っていた不具合)。 */
    hud_draw(g_score, g_lives);

    ent_update_all();
    ent_resolve_collisions();
    ent_draw_all();
    sea_frame();   /* SEA13: 海コラムを1strip位相流し=水が艦に対して流れる擬似多重スクロール */

    /* 自機撃墜(ミス): 残機を1減らし、残っていれば面最初から全砲台復活でやり直し。
       尽きたら 継続ONでコンティニュー(残機を初期値へ戻して再挑戦=無限) / OFFでタイトルへ。 */
    if (g_miss) {
        g_miss = 0;
        if (g_lives) g_lives--;
        if (g_lives == 0) {
            if (g_continue) { g_lives = lives_init(); stage_setup(); }
            else return SC_TITLE;
        } else {
            stage_setup();
        }
        return SCENE_NONE;
    }
    /* 全砲台撃破でクリア → 撃破演出へ(炎上→撃破!!→スコア→ファンファーレ→エンディング) */
    if (phase == 1 && ent_count(ET_TURRET) == 0) {
        dmode = 1; dtimer = 150;   /* 約2.5秒の炎上スペクタクル */
        bgm_stop();
        return SCENE_NONE;
    }
    return SCENE_NONE;
}
