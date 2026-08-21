# 海イントロ検証(アドレス非依存): タイトル→SPACE→(カード/ファンファーレ自動)→海フェーズを複数枚撮る。
set b [file dirname [info script]]
proc shot {f} { global b; screenshot -raw -prefix {} [file join $b .. build $f] }
proc pressSpace {} { keymatrixdown 8 0x01; after time 0.2 { keymatrixup 8 0x01 } }
after time 7.0  { shot fgt_title.png; pressSpace }
after time 30   { shot fgt_a.png }
after time 42   { shot fgt_b.png }
after time 54   { shot fgt_c.png }
after time 66   { shot fgt_d.png ; exit }
