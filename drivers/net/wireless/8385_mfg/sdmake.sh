mount /dev/sda1 /mnt/usb
cp /mnt/usb/2bulverde/sdio8385p.h if/if_sdio/
make clean;make;make build
cp ../bin_sd8385/*.o /tftpboot/bulverte/root

