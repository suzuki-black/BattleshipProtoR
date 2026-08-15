proc dump {t} {
    global OUT
    set s "t=$t"
    foreach r {0 1 2 3 8 9} {
        set v 0
        catch { set v [debug read "PSG regs" $r] }
        append s " R$r=$v"
    }
    puts $OUT $s ; flush $OUT
}
set OUT [open "build/bgm_psg.txt" w]
after time 8.0  "dump 8.0"
after time 8.2  "dump 8.2"
after time 8.4  "dump 8.4"
after time 8.7  "dump 8.7"
after time 9.5  "dump 9.5"
after time 10.5 "dump 10.5"
after time 11.5 "dump 11.5 ; close \$OUT ; exit"
