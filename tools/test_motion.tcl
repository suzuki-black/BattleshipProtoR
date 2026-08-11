set b [file dirname [info script]]
after time 7  "screenshot -raw -prefix {} [file join $b .. build shot_a.png]"
after time 10 "screenshot -raw -prefix {} [file join $b .. build shot_b.png] ; exit"
