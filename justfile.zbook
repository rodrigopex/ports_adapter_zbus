alias c := clean
alias b := build
alias f := flash

openocd_bin := "~/.local/bin/usr/local/bin/openocd"
openodc_dir := "~/.local/openocd"

clean:
    rip build

build:
    west build -b zbook/rp2350b/m33

build-wifi:
    west build -b zbook/rp2350b/m33 --shield zbook_wifi

flash:
    west flash --openocd {{ openocd_bin }} --openocd-search {{ openodc_dir }}
