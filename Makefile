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

# ── 常駐(bank0-2, <=24KB)にリンクするソース。crt0 は先頭に別途リンク。
#    ここへ足すたびに常駐サイズが増える。冷たいものは足さず bcall バンクへ回すこと。
RESIDENT_RELS = \
  $(BUILD)/sys.rel \
  $(BUILD)/vdp.rel \
  $(BUILD)/bank.rel \
  $(BUILD)/input.rel \
  $(BUILD)/sound.rel \
  $(BUILD)/entity.rel \
  $(BUILD)/scene.rel \
  $(BUILD)/scene_boot.rel \
  $(BUILD)/scene_demo.rel \
  $(BUILD)/main.rel

# ── 追加バンク(冷たいコード/データ)。--bank N file の形で rompack へ渡す。
#    冷たいコードは「単独コンパイル → --code-loc 0xA000 でリンク → rompack が当該バンクへ格納」。
#    被呼コードは 0xA000 が単一エントリで自己完結(ARCHITECTURE §2)。
ROMPACK_BANKS = --bank 4 $(BUILD)/bank_demo.ihx

.PHONY: all rom clean run
all: rom
rom: GAME.ROM

$(BUILD):
	mkdir -p $(BUILD)

# C ソースは core/ と scenes/ から探す(basename は一意に保つ)
vpath %.c $(CORE) $(SCENES)

$(BUILD)/%.rel: %.c | $(BUILD)
	sdcc -m$(TARGET) -c $(OPT) $(INC) $< -o $@

$(BUILD)/crt0rom.rel: $(SRC)/crt0rom.s | $(BUILD)
	sdasz80 -o $@ $<

# 冷たいコード(バンク)を 0xA000 単独リンク。data-loc はバンク関数用の RAM 退避域(0xE900)。
$(BUILD)/bank_demo.ihx: $(SRC)/banked/bank_demo.c | $(BUILD)
	sdcc -m$(TARGET) -c $(OPT) $(INC) $< -o $(BUILD)/bank_demo.rel
	sdcc -m$(TARGET) --no-std-crt0 --code-loc 0xA000 --data-loc 0xE900 $(BUILD)/bank_demo.rel -o $@

# 常駐イメージのリンク(crt0 が先頭 = _HEADER/_CODE 起点)
$(BUILD)/rom.ihx: $(BUILD)/crt0rom.rel $(RESIDENT_RELS)
	sdcc -m$(TARGET) --no-std-crt0 --code-loc $(CODELOC) --data-loc $(DATALOC) \
	     $(BUILD)/crt0rom.rel $(RESIDENT_RELS) -o $@

GAME.ROM: $(BUILD)/rom.ihx $(BUILD)/bank_demo.ihx
	node tools/rompack.mjs --code $(BUILD)/rom.ihx --out $@ $(ROMPACK_BANKS)

# openMSX で起動 → 数秒後にスクショ → 終了(headless 検証)
run: GAME.ROM
	openmsx -machine $(MACHINE) -carta GAME.ROM -romtype ASCII8 -script tools/test_boot.tcl

MACHINE ?= C-BIOS_MSX2+_JP

clean:
	rm -rf $(BUILD) GAME.ROM
