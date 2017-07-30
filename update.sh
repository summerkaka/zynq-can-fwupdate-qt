mount /dev/mmcblk0p1 /mnt
cd /mnt
./fwupdate -d can0 -f nucleo-app.hex
