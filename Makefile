# ============================================================================
#  BattleshipProtoR — turboR専用エンジン ビルド定義(唯一の真実)
#  多モジュール分割: 巨大 main.c を作らない。常駐は小関数の集合体として個別 .c に。
# ============================================================================
include config.mk

ifeq ($(PROFILE),release)
  OPT = $(OPT_RELEASE)
else
  OPT = $(OPT_DEBUG)
endif

BUILD  = build
SRC    = src
CORE   = $(SRC)/core
SCENES = $(SRC)/scenes
INC    = -I$(SRC)/include

# ── ヘッダ依存(重要): 共有ヘッダ(特に構造体を
#    定義する entity.h 等)を変更したら全 .c を必ず再コンパイルする。これを怠ると
#    「新旧で構造体レイアウトが食い違うオブジェクトが混在→メモリ破損」という
#    stale-object バグを踏む(実際に踏んだ)。小規模なので全再コンパイルで十分。
HDRS := $(wildcard $(SRC)/include/*.h) config.mk

# ── 常駐(bank0-2, <=24KB)にリンクするソース。crt0 は先頭に別途リンク。
#    ここへ足すたびに常駐サイズが増える。冷たいものは足さず bcall バンクへ回すこと。
RESIDENT_RELS = \
  $(BUILD)/sys.rel \
  $(BUILD)/vdp.rel \
  $(BUILD)/bank.rel \
  $(BUILD)/input.rel \
  $(BUILD)/sound.rel \
  $(BUILD)/sprites.rel \
  $(BUILD)/gamestate.rel \
  $(BUILD)/entity.rel \
  $(BUILD)/player.rel \
  $(BUILD)/fire.rel \
  $(BUILD)/ops.rel \
  $(BUILD)/scroll.rel \
  $(BUILD)/scene.rel \
  $(BUILD)/scene_boot.rel \
  $(BUILD)/scene_stage.rel \
  $(BUILD)/scene_intro.rel \
  $(BUILD)/scene_boss.rel \
  $(BUILD)/main.rel

# ── 追加バンク(冷たいコード/データ)。--bank N file の形で rompack へ渡す。
#    冷たいコードは「単独コンパイル → --code-loc 0xA000 でリンク → rompack が当該バンクへ格納」。
#    被呼コードは 0xA000 が単一エントリで自己完結(ARCHITECTURE §2)。
ROMPACK_BANKS = --bank 4 $(BUILD)/bank_demo.ihx \
                --bank 5 $(BUILD)/scene_title.ihx \
                --bank 6 $(BUILD)/scene_config.ihx \
                --bank 7 $(BUILD)/scene_ending.ihx

.PHONY: all rom clean run
all: rom
rom: GAME.ROM

$(BUILD):
	mkdir -p $(BUILD)

# C ソースは core/ と scenes/ から探す(basename は一意に保つ)
vpath %.c $(CORE) $(SCENES)

$(BUILD)/%.rel: %.c $(HDRS) | $(BUILD)
	sdcc -m$(TARGET) -c $(OPT) $(INC) $< -o $@

$(BUILD)/crt0rom.rel: $(SRC)/crt0rom.s | $(BUILD)
	sdasz80 -o $@ $<

# 冷たいコード(バンク)を 0xA000 単独リンク。data-loc はバンク関数用の RAM 退避域(0xE900)。
$(BUILD)/bank_demo.ihx: $(SRC)/banked/bank_demo.c $(HDRS) | $(BUILD)
	sdcc -m$(TARGET) -c $(OPT) $(INC) $< -o $(BUILD)/bank_demo.rel
	sdcc -m$(TARGET) --no-std-crt0 --code-loc 0xA000 --data-loc 0xE900 $(BUILD)/bank_demo.rel -o $@

# 常駐イメージのリンク(crt0 が先頭 = _HEADER/_CODE 起点)。rom.noi に常駐シンボル番地が出る。
$(BUILD)/rom.ihx: $(BUILD)/crt0rom.rel $(RESIDENT_RELS)
	sdcc -m$(TARGET) --no-std-crt0 --code-loc $(CODELOC) --data-loc $(DATALOC) \
	     $(BUILD)/crt0rom.rel $(RESIDENT_RELS) -o $@

# ── バンクシーン(冷たいシーン)ビルド(2パス) ──
# 1) 常駐 rom.ihx → rom.noi から常駐シンボル絶対番地を .s に落とす(バンク側が常駐関数を呼ぶため)
$(BUILD)/resident_syms.rel: $(BUILD)/rom.ihx tools/gen_symdefs.mjs
	node tools/gen_symdefs.mjs $(BUILD)/rom.noi $(BUILD)/resident_syms.s
	sdasz80 -o $@ $(BUILD)/resident_syms.s
# バンク先頭スタブ(0xA000 に jp _banked_entry を確定)
$(BUILD)/bankhead.rel: $(SRC)/banked/bankhead.s | $(BUILD)
	sdasz80 -o $@ $<
# 2) バンクシーン汎用ルール(scene_<name>.c → bank .ihx)。追加は ROMPACK_BANKS に1行。
#    bankhead + scene_<name> + resident_syms を 0xA000 リンク。data-loc は各シーン共用の退避域。
$(BUILD)/scene_%.ihx: $(SCENES)/scene_%.c $(HDRS) $(BUILD)/bankhead.rel $(BUILD)/resident_syms.rel
	sdcc -m$(TARGET) -c $(OPT) $(INC) $< -o $(BUILD)/scene_$*.rel
	sdcc -m$(TARGET) --no-std-crt0 --code-loc 0xA000 --data-loc 0xE000 \
	     $(BUILD)/bankhead.rel $(BUILD)/scene_$*.rel $(BUILD)/resident_syms.rel -o $@

BANK_IHX = $(BUILD)/bank_demo.ihx $(BUILD)/scene_title.ihx \
           $(BUILD)/scene_config.ihx $(BUILD)/scene_ending.ihx

GAME.ROM: $(BUILD)/rom.ihx $(BANK_IHX)
	node tools/rompack.mjs --code $(BUILD)/rom.ihx --out $@ $(ROMPACK_BANKS)

# openMSX で起動 → 数秒後にスクショ → 終了(headless 検証)
run: GAME.ROM
	openmsx -machine $(MACHINE) -carta GAME.ROM -romtype ASCII8 -script tools/test_boot.tcl

MACHINE ?= C-BIOS_MSX2+_JP

clean:
	rm -rf $(BUILD) GAME.ROM
