alias b := build
alias bb := rebuild
alias c := clean
alias r := run
alias ds := debugserver_qemu
alias da := attach

flags := "-DCONFIG_QEMU_ICOUNT=n"

build_dir := './build'
c_build_dir := "-d " + build_dir

board := "mps2/an385"
c_board := "-b " + board

dir := '.'

target_serial_port := '/dev/ttyACM0'

pre := '\n~~> just'

default:
    @just --list

minimal:
    just c b r

# Clean build directory
clean:
    #!/usr/bin/env perl
    if(-d "{{build_dir}}") {
        print("{{pre}} clean: removing build folder");
        system("rm -rf {{ build_dir }}");
    } else {
        print("{{pre}} clean: build folder not found.");
    }

# Rebuild the project
rebuild:
    @echo "{{pre}} rebuild: rebuilding project"
    west build

build:
    @echo "{{pre}} build: building project"
    west build {{ c_build_dir }} {{ c_board }} {{ dir }} -- {{ flags }}

run:
    @echo "{{pre}} run: running project"
    west build -t run

# Open the menuconfig of the project using variables: build_dir
config:
    west build -d {{ build_dir }} -t menuconfig

debugserver_qemu:
    west build -t debugserver_qemu

attach:
    $ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb -x {{dir}}/gdbinit {{ build_dir }}/zephyr/zephyr.elf

kb arg:
    @fend @no_trailing_newline "{{arg}} << 10" | wl-copy
