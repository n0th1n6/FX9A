# Host-driven 24LC16 read via the existing STM32 GPIOs. No flash or SRAM writes.
# Invoke from the repository root using OpenOCD with stlink.cfg/stm32f1x.cfg.
# GPIO wiring must match the checked-in FX9A source: PE2=WP, PE3=SCL, PE4=SDA.

proc ee_high {mask} { mww 0x40011810 $mask }
proc ee_low {mask} { mww 0x40011814 $mask }
proc ee_inputs {} { return [lindex [read_memory 0x40011808 32 1] 0] }
proc ee_start {} {
    ee_high 0x10
    ee_high 0x08
    if {([ee_inputs] & 0x18) != 0x18} { error "EEPROM bus is not released" }
    ee_low 0x10
    ee_low 0x08
}
proc ee_stop {} {
    ee_low 0x08
    ee_low 0x10
    ee_high 0x08
    ee_high 0x10
}
proc ee_tx {value} {
    for {set bit 7} {$bit >= 0} {incr bit -1} {
        if {$value & (1 << $bit)} { ee_high 0x10 } else { ee_low 0x10 }
        ee_high 0x08
        ee_low 0x08
    }
    ee_high 0x10
    ee_high 0x08
    set nack [expr {[ee_inputs] & 0x10}]
    ee_low 0x08
    if {$nack} { error "EEPROM did not acknowledge byte $value" }
}
proc ee_rx {last} {
    ee_high 0x10
    set value 0
    for {set bit 0} {$bit < 8} {incr bit} {
        ee_high 0x08
        set value [expr {($value << 1) | (([ee_inputs] >> 4) & 1)}]
        ee_low 0x08
    }
    if {$last} { ee_high 0x10 } else { ee_low 0x10 }
    ee_high 0x08
    ee_low 0x08
    ee_high 0x10
    return $value
}

init
set original_state [stm32f1x.cpu curstate]
echo "Original CPU state: $original_state"
set touched 0
set original_odr 0
set failure [catch {
    halt
    wait_halt 2000
    set clock [lindex [read_memory 0x40021018 32 1] 0]
    set crl [lindex [read_memory 0x40011800 32 1] 0]
    set original_odr [lindex [read_memory 0x4001180c 32 1] 0]
    echo [format "Original GPIOE CRL=0x%08x ODR=0x%08x" $crl $original_odr]
    if {($clock & 0x40) == 0} { error "GPIOE clock is disabled" }
    if {($crl & 0xfff00) != 0x66200} { error "Unexpected EEPROM GPIO configuration" }
    if {([ee_inputs] & 0x18) != 0x18} { error "EEPROM bus is busy; retry when idle" }
    # Assert write protection before generating any bus transitions.
    ee_high 0x04
    set touched 1
    if {([ee_inputs] & 0x04) == 0} { error "EEPROM write-protect pin did not go high" }
    for {set pass 1} {$pass <= 2} {incr pass} {
        set hex ""
        for {set bank 0} {$bank < 8} {incr bank} {
            ee_start
            # Address-only write phase sets the read pointer; no data is written.
            ee_tx [expr {0xa0 | ($bank << 1)}]
            ee_tx 0
            ee_start
            ee_tx [expr {0xa1 | ($bank << 1)}]
            for {set offset 0} {$offset < 256} {incr offset} {
                append hex [format %02x [ee_rx [expr {$offset == 255}]]]
            }
            ee_stop
            echo "EEPROM pass $pass: [expr {($bank + 1) * 256}]/2048 bytes"
        }
        # OpenOCD's Jim Tcl does not provide the Tcl binary command.
        # Preserve ASCII hex and convert to raw bytes on the host afterward.
        set path "backups/2026-11-18-stlink/eeprom/fx9a-eeprom-read${pass}.hex.txt"
        set out [open $path w]
        puts $out $hex
        close $out
        echo "Saved $path"
    }
} reason]

set cleanup_failure [catch {
    if {$touched} {
        ee_stop
        # Restore only the three EEPROM outputs. All other GPIOs are untouched.
        set saved [expr {$original_odr & 0x1c}]
        ee_high $saved
        ee_low [expr {0x1c & ~$saved}]
    }
    if {$original_state eq "running"} { resume }
} cleanup_reason]
if {$cleanup_failure} { echo "CLEANUP FAILED: $cleanup_reason" }
if {$failure} { echo "EEPROM READ FAILED: $reason" }
if {!$failure && !$cleanup_failure} { echo "EEPROM reads complete; original CPU state restored" }
shutdown
