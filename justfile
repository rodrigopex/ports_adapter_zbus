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

gen_service_files service_name:
    @echo "Regenerating service files to build directory..."
    @test -d build/modules/{{service_name}}_service || (echo "Error: build/modules/{{service_name}}_service not found. Run 'just b' first." && exit 1)
    python3 zephlets/shared/codegen/generate_service.py \
    --proto zephlets/{{service_name}}/{{service_name}}_service.proto \
    --output-dir build/modules/{{service_name}}_service \
    --service-name {{service_name}}_service \
    --module-dir {{service_name}} \
    --no-generate-impl

# Create new service (interactive)
new_service_interactive:
    copier copy --UNSAFE \
    zephlets/shared/codegen/zephyr_service_template \
    zephlets/

# Create new service with name
new_service service_name:
    copier copy --UNSAFE --force \
    --data service_name="{{service_name}}" \
    --data include_basic_commands=true \
    --data service_description="{{service_name}} Service Module" \
    --data author_name="" \
    zephlets/shared/codegen/zephyr_service_template \
    zephlets/

# Update service from template
update_service service_name:
    @cd zephlets/{{service_name}} && copier update --UNSAFE

# Create new adapter (interactive)
new_adapter_interactive:
    python3 zephlets/shared/codegen/generate_adapter.py \
    --services-path zephlets \
    --output-dir adapters \
    --interactive

# Create new adapter with origin and destination
new_adapter origin dest:
    python3 zephlets/shared/codegen/generate_adapter.py \
    --services-path zephlets \
    --output-dir adapters \
    --origin {{origin}} \
    --destination {{dest}}
