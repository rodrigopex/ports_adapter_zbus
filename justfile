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

gen_zephlet_files zephlet_name:
    @echo "Regenerating zephlet files to build directory..."
    @test -d build/modules/{{ zephlet_name }}_zephlet || (echo "Error: build/modules/{{ zephlet_name }}_zephlet not found. Run 'just b' first." && exit 1)
    python3 zephlets/shared/codegen/generate_zephlet.py \
    --proto zephlets/{{ zephlet_name }}/{{ zephlet_name }}_zephlet.proto \
    --output-dir build/modules/{{ zephlet_name }}_zephlet \
    --zephlet-name {{ zephlet_name }}_zephlet \
    --module-dir {{ zephlet_name }} \
    --no-generate-impl

# Create new zephlet (interactive)
new_zephlet_interactive:
    copier copy --UNSAFE \
    zephlets/shared/codegen/zephyr_zephlet_template \
    zephlets/

# Create new zephlet with name
new_zephlet zephlet_name:
    copier copy --UNSAFE --force \
    --data zephlet_name="{{ zephlet_name }}" \
    --data include_basic_commands=true \
    --data zephlet_description="{{ zephlet_name }} Zephlet Module" \
    --data author_name="" \
    zephlets/shared/codegen/zephyr_zephlet_template \
    zephlets/

# Update zephlet from template
update_zephlet zephlet_name:
    @cd zephlets/{{ zephlet_name }} && copier update --UNSAFE

# Create new adapter (interactive)
new_adapter_interactive:
    python3 zephlets/shared/codegen/generate_adapter.py \
    --zephlets-path zephlets \
    --output-dir adapters \
    --interactive

# Create new adapter with origin and destination
new_adapter origin dest:
    python3 zephlets/shared/codegen/generate_adapter.py \
    --zephlets-path zephlets \
    --output-dir adapters \
    --origin {{ origin }} \
    --destination {{ dest }}

# Test commands

test_build_dir := './build_test'

# Run all tests (unit + integration)
test:
    @echo "{{ pre }} test: running all tests"
    west twister --testsuite-root zephlets --inline-logs -p qemu_x86_64
    west twister --testsuite-root tests --inline-logs -p qemu_x86_64

# Run only unit tests (from all zephlets)
test_unit:
    @echo "{{ pre }} test_unit: running unit tests"
    west twister --testsuite-root zephlets --inline-logs

# Run only integration tests
test_integration:
    @echo "{{ pre }} test_integration: running integration tests"
    west twister --testsuite-root tests/integration --inline-logs

# Run specific zephlet's tests
test_zephlet zephlet_name:
    @echo "{{ pre }} test_zephlet: {{ zephlet_name }}"
    west twister --testsuite-root zephlets/{{ zephlet_name }}/tests --inline-logs -v

# Run specific test by path
test_one test_path:
    @echo "{{ pre }} test_one: {{ test_path }}"
    west twister --testsuite-root {{ test_path }} --inline-logs -v

# Build and run single test manually (Linux only - macOS users should use test_zephlet)
test_build test_path:
    @echo "{{ pre }} test_build: {{ test_path }}"
    west build -b native_sim {{ test_path }} -d {{ test_build_dir }}
    {{ test_build_dir }}/zephyr/zephyr.exe

# Clean test artifacts
test_clean:
    @echo "{{ pre }} test_clean"
    command rm -rf {{ test_build_dir }} twister-out

# Coverage report (requires gcovr)
test_coverage:
    @echo "{{ pre }} test_coverage"
    west twister --testsuite-root zephlets --coverage --coverage-tool gcovr
    west twister --testsuite-root tests --coverage --coverage-tool gcovr
