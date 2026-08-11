;; ============================================================================
;;  crt0rom.s — turboR専用エンジン(BattleshipProtoR)の ROM 起動コード
;;  32K ROM 起動("AB"ヘッダ@0x4000, BIOS が init を呼ぶ)＋ ASCII8 マッパー初期化
;;  ＋ 汎用バンクコール・トランポリン(_bcall)。
;;
;;  重要: 32K ROM は INIT 時にページ1(0x4000-0x7FFF)しか自スロットへ切替わらない。
;;  コード/定数が 0x8000 を超える(gsinit 等が page2 に居る)ので、page2 を自スロットへ
;;  ENASLT で有効化してから gsinit/_main を呼ぶ。出典: MSX2 Technical Handbook ch5。
;;
;;  リファレンス: 前作 BattleshipProto cport(code-banking ブランチ, 実機確定済み)。
;; ============================================================================
        .module crt0rom
        .globl  _main
        .globl  _g_bank
        .globl  l__INITIALIZER
        .globl  s__INITIALIZER
        .globl  s__INITIALIZED

        .area   _HEADER (ABS)
        .org    0x4000
        .db     0x41, 0x42      ; "AB"
        .dw     init            ; INIT エントリ
        .dw     0x0000          ; STATEMENT
        .dw     0x0000          ; DEVICE
        .dw     0x0000          ; TEXT
        .dw     0x0000          ; reserved
        .dw     0x0000
        .dw     0x0000

        .area   _CODE
init:
        ;; --- page2(0x8000-0xBFFF) を自カートリッジのスロットへ有効化 (32K ROM 必須) ---
        call    0x0138          ; RSLREG: A = 基本スロットレジスタ(ポート0xA8)
        rrca                    ; page1 のスロット番号(bit3-2)を bit1-0 へ
        rrca
        and     #0x03
        ld      c, a
        ld      b, #0
        ld      hl, #0xFCC1     ; EXPTBL
        add     hl, bc
        ld      c, a            ; C = 基本スロット番号
        ld      a, (hl)         ; 拡張スロットか?
        and     #0x80
        or      c               ; 拡張なら最上位ビットを立てる
        ld      c, a            ; C = スロットID(F00000PP)
        inc     hl
        inc     hl
        inc     hl
        inc     hl              ; HL -> SLTTBL[slot]
        ld      a, (hl)         ; 2次スロットレジスタ値
        and     #0x0C           ; page1/2 の2次スロットビット(bit3-2)
        or      c               ; F000SSPP を組み立て
        ld      h, #0x80        ; page2 指定
        call    0x0024          ; ENASLT (page2 を有効化)

        ;; --- ASCII8 マッパー初期化: 4窓を確定(bank0-2=コード固定, bank3=スワップ窓既定) ---
        ;;    書込アドレスでバンク選択。gsinit/main 呼出前に page2 側の窓を確定させる。
        ld      a, #0
        ld      (0x6000), a     ; 0x4000-0x5FFF = bank0
        ld      a, #1
        ld      (0x6800), a     ; 0x6000-0x7FFF = bank1
        ld      a, #2
        ld      (0x7000), a     ; 0x8000-0x9FFF = bank2
        ld      a, #3
        ld      (0x7800), a     ; 0xA000-0xBFFF = bank3 (スワップ窓の既定)

        ;; --- 以降 page2 のコード(gsinit/main)を安全に呼べる ---
        call    gsinit
        call    _main
loop:
        jr      loop            ; ROM: 戻らずループ

        ;; ------------------------------------------------------------------
        ;;  _bcall — 汎用バンクコール・トランポリン
        ;;  crt0内(リンク先頭=0x4000近傍=常駐<0xA000)に置くので、0xA000窓を差替えても
        ;;  このコード自身は生きている。C から: g_bank = <bank>; bcall();
        ;;  被呼バンクは 0xA000 を単一エントリとして自己完結(常駐関数もデータ窓も触らない)。
        ;;  切替中は di。終わったらスワップ窓を既定(bank3)へ復元。
        ;; ------------------------------------------------------------------
_bcall::
        ld      a, (_g_bank)    ; 呼ぶバンク番号(C側が設定)
        di
        ld      (0x7800), a     ; 0xA000-0xBFFF = そのバンク(被呼コード)
        call    0xA000          ; バンクコード実行(0xA000 がエントリ)
        ld      a, #3
        ld      (0x7800), a     ; スワップ窓を既定(bank3)へ復元
        ei
        ret

        .area   _INITIALIZER
        .area   _HOME
        .area   _GSINIT
gsinit:
        ld      bc, #l__INITIALIZER
        ld      a, b
        or      a, c
        jr      Z, gsinit_done
        ld      de, #s__INITIALIZED
        ld      hl, #s__INITIALIZER
        ldir
gsinit_done:
        .area   _GSFINAL
        ret

        .area   _DATA
        .area   _INITIALIZED
        .area   _BSS
        .area   _HEAP
