set b [file dirname [info script]]
after time 8  "screenshot -raw -prefix {} [file join $b .. build sc1.png]"
after time 12 "screenshot -raw -prefix {} [file join $b .. build sc2.png]"
after time 18 "screenshot -raw -prefix {} [file join $b .. build sc3.png]"
after time 22 "screenshot -raw -prefix {} [file join $b .. build sc4.png] ; exit"
