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
    if(-d "{{ build_dir }}") {
        print("{{ pre }} clean: removing build folder");
        system("rm -rf {{ build_dir }}");
    } else {
        print("{{ pre }} clean: build folder not found.");
    }

# Rebuild the project
rebuild:
    @echo "{{ pre }} rebuild: rebuilding project"
    west build

build:
    @echo "{{ pre }} build: building project"
    west build {{ c_build_dir }} {{ c_board }} {{ dir }} -- {{ flags }}

run:
    @echo "{{ pre }} run: running project"
    west build -t run

# Open the menuconfig of the project using variables: build_dir
config:
    west build -d {{ build_dir }} -t menuconfig

debugserver_qemu:
    west build -t debugserver_qemu

attach:
    $ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb -x {{ dir }}/gdbinit {{ build_dir }}/zephyr/zephyr.elf

kb arg:
    @fend @no_trailing_newline "{{ arg }} << 10" | wl-copy

# Test commands

test_build_dir := './build_test'
twister_out_dir := '/tmp/twister-out'

# Run all tests (unit + integration)
test:
    @echo "{{ pre }} test: running all tests"
    west twister --testsuite-root src/zephlets -vvv --inline-logs -p mps2/an385 -O {{ twister_out_dir }}
    west twister --testsuite-root tests -vvv --inline-logs -p mps2/an385 -O {{ twister_out_dir }}

# Run only unit tests (from all zephlets)
test_unit:
    @echo "{{ pre }} test_unit: running unit tests"
    west twister --testsuite-root src/zephlets --inline-logs -O {{ twister_out_dir }}

# Run only integration tests
test_integration:
    @echo "{{ pre }} test_integration: running integration tests"
    west twister --testsuite-root tests/integration --inline-logs -O {{ twister_out_dir }}

# Run specific zephlet's tests
test_zephlet zephlet_name:
    @echo "{{ pre }} test_zephlet: {{ zephlet_name }}"
    west twister --testsuite-root src/zephlets/{{ zephlet_name }}/tests --inline-logs -v -O {{ twister_out_dir }}

# Run specific test by path
test_one test_path:
    @echo "{{ pre }} test_one: {{ test_path }}"
    west twister --testsuite-root {{ test_path }} --inline-logs -v -O {{ twister_out_dir }}

# Build and run single test manually (Linux only - macOS users should use test_zephlet)
test_build test_path:
    @echo "{{ pre }} test_build: {{ test_path }}"
    west build -b native_sim {{ test_path }} -d {{ test_build_dir }}
    {{ test_build_dir }}/zephyr/zephyr.exe

# Clean test artifacts
test_clean:
    @echo "{{ pre }} test_clean"
    command rm -rf {{ test_build_dir }} {{ twister_out_dir }}

# Coverage report (requires gcovr)
test_coverage:
    @echo "{{ pre }} test_coverage"
    west twister --testsuite-root src/zephlets --coverage --coverage-tool gcovr -O {{ twister_out_dir }}
    west twister --testsuite-root tests --coverage --coverage-tool gcovr -O {{ twister_out_dir }}
