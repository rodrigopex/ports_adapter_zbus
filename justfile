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
    west twister --testsuite-root src -vvv --inline-logs -p mps2/an385 -O {{ twister_out_dir }}

# Run only unit tests (from all zephlets)
test_unit:
    @echo "{{ pre }} test_unit: running unit tests"
    west twister --testsuite-root src --inline-logs -O {{ twister_out_dir }}

# Run only integration tests
test_integration:
    @echo "{{ pre }} test_integration: running integration tests"
    west twister --testsuite-root tests/integration --inline-logs -O {{ twister_out_dir }}

# Run specific zephlet's tests
test_zephlet zephlet_name:
    @echo "{{ pre }} test_zephlet: {{ zephlet_name }}"
    west twister --testsuite-root src/{{ zephlet_name }}/tests --inline-logs -v -O {{ twister_out_dir }}

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
    west twister --testsuite-root src --coverage --coverage-tool gcovr -O {{ twister_out_dir }}
    west twister --testsuite-root tests --coverage --coverage-tool gcovr -O {{ twister_out_dir }}

# Footprint analysis on nrf52833dk
nrf_build_dir := './build_nrf'

size_nrf:
    @echo "{{ pre }} size_nrf: building for nrf52833dk + zephlet footprint report"
    west build -d {{ nrf_build_dir }} -b nrf52833dk/nrf52833 .
    west build -d {{ nrf_build_dir }} -t rom_report > {{ nrf_build_dir }}/rom_report.txt
    west build -d {{ nrf_build_dir }} -t ram_report > {{ nrf_build_dir }}/ram_report.txt
    python scripts/size_report.py {{ nrf_build_dir }} > {{ nrf_build_dir }}/zephlet_size_report.md
    @echo "  rom_report:    {{ nrf_build_dir }}/rom_report.txt"
    @echo "  ram_report:    {{ nrf_build_dir }}/ram_report.txt"
    @echo "  zephlet table: {{ nrf_build_dir }}/zephlet_size_report.md"
    @cat {{ nrf_build_dir }}/zephlet_size_report.md

# Marginal cost: rebuild without one zephlet, diff zephlet_size_report.md
size_nrf_minus zephlet:
    @echo "{{ pre }} size_nrf_minus: rebuilding nrf52833dk with CONFIG_ZEPHLET_{{ zephlet }}=n"
    west build -d {{ nrf_build_dir }}_minus -b nrf52833dk/nrf52833 . -- -DCONFIG_ZEPHLET_{{ zephlet }}=n
    python scripts/size_report.py {{ nrf_build_dir }}_minus > {{ nrf_build_dir }}_minus/zephlet_size_report.md
    @echo "Diff vs full build:"
    @diff {{ nrf_build_dir }}/zephlet_size_report.md {{ nrf_build_dir }}_minus/zephlet_size_report.md || true

# Latency benchmark on nrf52833dk
bench_build_dir := './build_bench'

bench:
    @echo "{{ pre }} bench: building latency harness for nrf52833dk"
    west build -d {{ bench_build_dir }} -b nrf52833dk/nrf52833 tests/bench

bench_flash:
    @echo "{{ pre }} bench_flash: flashing bench harness"
    west flash -d {{ bench_build_dir }}

bench_clean:
    rm -rf {{ bench_build_dir }}
