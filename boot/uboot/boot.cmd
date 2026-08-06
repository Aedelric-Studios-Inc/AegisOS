# AegisOS U-Boot boot script
# Place in FAT partition as /boot/boot.scr (compiled with mkimage)

setenv bootargs "console=ttyAMA0,115200 root=/dev/mmcblk0p2 rw rootwait quiet"
setenv kernel_addr 0x40080000
setenv dtb_addr    0x48000000

fatload mmc 0:1 ${kernel_addr} /boot/aegisos.bin
fatload mmc 0:1 ${dtb_addr}   /boot/aegisos.dtb

booti ${kernel_addr} - ${dtb_addr}
