set b [file dirname [info script]]
proc rd {a} { debug read memory $a }
proc shot {} { global b; screenshot -raw -prefix {} [file join $b .. build title_a.png]; after time 0.3 "exit" }
proc onTitle {} { after time 0.6 shot }
proc wait {} { if {[rd 0xC1DC] == 1} { onTitle } else { after time 0.2 wait } }
after time 5 wait
after time 25 "exit"
