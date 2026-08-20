# 海イントロ敵機の検証: タイトル→SPACE→ステージ1開始→海フェーズで降下する敵機を数枚撮る。
set b [file dirname [info script]]
set SCENE 0xC400
proc rd {a} { debug read memory $a }
proc shot {f} { global b; screenshot -raw -prefix {} [file join $b .. build $f] }
proc pressSpace {} { keymatrixdown 8 0x01; after time 0.2 { keymatrixup 8 0x01 } }
proc waitStage {} { global SCENE
    if {[rd $SCENE] == 2} {
        after time 16 { shot fgt_a.png }
        after time 24 { shot fgt_b.png }
        after time 32 { shot fgt_c.png }
        after time 40 { shot fgt_d.png ; exit }
    } else { after time 0.2 waitStage }
}
after time 7.0 { shot fgt_title.png; pressSpace; after time 1.0 waitStage }
after time 90 "exit"
