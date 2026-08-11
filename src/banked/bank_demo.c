/* banked/bank_demo.c — 実バンクコール(bcall)の実証モジュール(冷たいコードの雛形)。
   規律(ARCHITECTURE §2 バンクコード3原則):
     - 0xA000 にリンク(単一エントリ=このファイルの先頭関数が 0xA000 に来る)
     - 常駐関数を呼ばない / データ窓(0xA000)を読まない / 自己完結
   ビルド: 単独コンパイル → --code-loc 0xA000 でリンク → rompack が bank4(ROM 0x8000)へ格納。
   実行: 常駐が bcall_to(4) → crt0 の _bcall が 0xA000 窓へ bank4 を差替え → call 0xA000(=この関数)。
   proof: RAM 0xE000 に 0x5A を書く(常駐が読んで検証)。将来この枠へ冷たいシーン描画等を載せる。 */
void bank_entry(void) {
    *(volatile unsigned char *)0xE000 = 0x5A;
}
