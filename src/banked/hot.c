/* hot.c — RAM実行されるAAサブシステム(aa_update/aa_collide)。
   ★このファイルは常駐にリンクせず、hot_ram[] の実番地に単独 --code-loc してバンク格納する(hotcode.h)。
     R800のROMフェッチ律速を外すのが目的なので、必ずRAM上で実行されること。
   ★挙動は scene_stage.c の旧 aa_update/aa_collide と同一(AA走査は§4-2の窓化で可視区間のみに短縮)。
     プールは ent_pool()、AA状態は aa_hot.h の常駐シンボルを参照(RAM実行モジュールにDATAは持たせない)。
   ★当たり判定(ent_resolve_collisions)はRAM化しても実機で速度不変だったため entity.c(ROM)へ戻した。 */
#include "entity.h"
#include "sound.h"
#include "gamestate.h"
#include "scroll.h"       /* SC_SHIP_R0 */
#include "player.h"       /* g_player_x/y */
#include "fire.h"         /* emit/emit_burst/aim_dir */
#include "ship.h"         /* SHIP_NAAG, ship_aag_tables */
#include "aa_hot.h"       /* AA共有状態(cam/curstage/aa_*)＋rnd/burn_add */
#include "assets_data.h"  /* ship_aagtbl, STAGE_COUNT */

/* ===== 対空砲23基の発砲(RAM実行) =====
   ★挙動は scene_stage.c の旧 aa_update と完全同一。移設のみ(状態は aa_hot.h 経由で常駐を参照)。
     毎フレーム23基を走査するのでRAM実行の効き所。座標表はループ外で1回取得。
   ※窓化(可視区間のみ走査)も試したが実機で速度不変(残コストは走査でなく発砲emit側)だったため撤回。 */
static const u8 aafire_iv[STAGE_COUNT] = { 140, 132, 84, 64, 40 };   /* 面別発砲間隔(小=激しい) */
void hot_aa_update(void) {
    u8 tbl = ship_aagtbl[curstage], i, fired = 0, nv = 0;
    const u8 *aax; const u16 *aay;
    ship_aag_tables(tbl, &aax, &aay);   /* 座標表をループ外で1回取得=毎フレーム23回の関数呼び+switchを排除 */
    for (i = 0; i < SHIP_NAAG; i++) {
        s16 gx, sy, sx; u16 gy;
        if (aa_dead[i]) continue;                           /* 破壊済みは撃たない/当たらない */
        gx = (s16)aax[i]; gy = aay[i];                      /* 直接添字参照(ship_aag_pos関数呼びを回避) */
        sy = (s16)(SC_SHIP_R0 * 16 + (s16)gy) - (s16)cam;   /* 画面Y */
        if (sy < -8 || sy > 216) continue;                  /* 当たり範囲外=発砲も当たりも無い(艦が視界外) */
        sx = gx + g_meander;                                /* 画面X(蛇行に追従) */
        aa_vis_i[nv] = i; aa_vis_sx[nv] = sx; aa_vis_sy[nv] = sy; nv++;  /* 可視AAを記録(aa_collideが使う) */
        if (sy < 8 || sy > 200) continue;                   /* 発砲はより狭い帯でのみ(画面端の砲は撃たない) */
        if (aa_fire[i]) { aa_fire[i]--; continue; }
        if (fired >= 2) { aa_fire[i] = 1; continue; }       /* 同フレーム発砲上限=発砲波の平準化(発射レートは不変) */
        { /* 狙い±3ステップの散らし。rnd()&7 → -3..+4(≒±38°)。マスクのみ=除算を使わない。 */
          u8 dir = (u8)((aim_dir(sx, sy, (s16)g_player_x, (s16)g_player_y) + (rnd() & 7) + 29) & 31);
          if (i < 14) { emit_burst(sx, sy, dir, 2, 42); }    /* 大型=時限信管エアバースト(橙カプセル, fuze42) */
          else { emit(sx, sy, dir, 0, 2); }                  /* 小型=通常小弾(橙ペレット) */
        }
        { u16 iv = (u16)aafire_iv[curstage] + (u16)i * 6;    /* u16で計算し255クランプ(高i=最上部の砲ほど間隔長) */
          aa_fire[i] = diff_interval((iv > 255) ? 255 : (u8)iv); }
        fired++;
        sfx(1, SFX_EFIRE);
    }
    aa_nvis = nv;   /* このフレームの可視AA数(aa_collideが使う) */
}

/* 自機弾 × 対空砲(RAM実行)。★挙動は scene_stage.c の旧 aa_collide と完全同一。移設のみ。 */
void hot_aa_collide(void) {
    u8 v, k, nb = 0;
    Entity *bul[8];                     /* 画面内の自機弾を一度だけ収集(AA毎の全プール再走査=乗算を排す) */
    Entity *e = ent_pool();
    for (k = 0; k < ENT_MAX; k++, e++)
        if (e->active && e->type == ET_BULLET && e->team == TEAM_PLAYER && nb < 8) bul[nb++] = e;
    if (!nb || !aa_nvis) return;        /* 自機弾/可視AAが無ければ即終了 */
    {
    const u8 *aax; const u16 *aay;
    ship_aag_tables(ship_aagtbl[curstage], &aax, &aay);   /* 撃破時のburn_add用(gx/gyを命中時だけ再読込) */
    /* aa_updateが作った可視AAリスト(sx/sy計算済)だけを回す=23基走査→可視分へ短縮＋sx/sy再計算を排除。 */
    for (v = 0; v < aa_nvis; v++) {
        u8 i = aa_vis_i[v];
        s16 sx = aa_vis_sx[v], sy = aa_vis_sy[v];
        for (k = 0; k < nb; k++) {
            Entity *b = bul[k];
            if (!b->active) continue;
            { s16 dx = (s16)(b->x + 8) - sx, dy = (s16)(b->y + 8) - sy;
              if (dx < 0) dx = -dx; if (dy < 0) dy = -dy;
              if (dx < 10 && dy < 10) {
                  b->active = 0;
                  if (aa_hp[i]) aa_hp[i]--;
                  if (aa_hp[i] == 0) {
                      aa_dead[i] = 1; g_score += (i < 14) ? 30 : 20;
                      burn_add((s16)aax[i], (u16)(SC_SHIP_R0 * 16 + aay[i]), (u8)((i < 14) ? 1 : 2));  /* 炎上サイト登録 */
                      ent_spawn_explosion(sx, sy); sfx(2, SFX_BOOM); g_shake = 6;
                  } else { ent_spawn_spark(b->x, b->y); sfx(2, SFX_HIT); }
                  break;
              }
            }
        }
    }
    }
}
