set b [file dirname [info script]]
set SL [file join $b .. build boss_L.png]
set SR [file join $b .. build boss_R.png]
set LOG [open [file join $b .. build aim.log] w]
proc rd {a} { debug read memory $a }
proc lg {m} { global LOG; puts $LOG $m; flush $LOG }
proc shotR {} { global SR; lg "RIGHT px=[rd 0xC1DA] hit=[rd 0xC1D9] kills=[rd 0xC1D8]"; screenshot -raw -prefix {} $SR; keymatrixup 8 0x80; after time 0.2 "exit" }
proc goRight {} { after time 1.9 shotR; keymatrixdown 8 0x80 }
proc shotL {} { global SL; lg "LEFT  px=[rd 0xC1DA] hit=[rd 0xC1D9] kills=[rd 0xC1D8]"; screenshot -raw -prefix {} $SL; keymatrixup 8 0x10; after time 0.4 goRight }
proc onboss {} { lg "boss scene=[rd 0xC1DC] px0=[rd 0xC1DA]"; keymatrixdown 8 0x10; after time 1.9 shotL }
proc waitboss {} { if {[rd 0xC1DC] == 2} { onboss } else { after time 0.3 waitboss } }
after time 6 waitboss
after time 45 "exit"
