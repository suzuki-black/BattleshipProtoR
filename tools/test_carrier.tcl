# 2面 空母の艦載機射出(ET_PURSUER)検証。stage_sel=1(空母)＋無敵(死んで海に戻らない)＋throttle off。
set b [file dirname [info script]]
proc shot {f} { global b; screenshot -raw -prefix {} [file join $b .. build $f] }
set throttle off
after time 6.5 { poke 0xCCD0 1; poke 0xCCD2 1 }   ;# g_stage_sel=Carrier, g_invinc=1
after time 7.0 { keymatrixdown 8 0x01; after time 0.2 { keymatrixup 8 0x01 } }
after time 110 { shot cv_a.png }
after time 125 { shot cv_b.png }
after time 140 { shot cv_c.png }
after time 155 { shot cv_d.png; exit }
