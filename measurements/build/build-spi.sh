#!/bin/bash

git clone https://github.com/unikraft/app-helloworld && cd app-helloworld

mkdir workdir && git clone -b RELEASE-0.16.3 https://github.com/unikraft/unikraft.git workdir/unikraft

git clone https://github.com/Mihnea0Firoiu/unikraft-rpi.git -b gpio_native workdir/unikraft/plat/raspi

mkdir workdir/unikraft/include/uk/intctlr/
cp workdir/unikraft/plat/raspi/include/uk/intctlr/limits.h workdir/unikraft/include/uk/intctlr/limits.h
cp workdir/unikraft/plat/raspi/include/uk/spi.h workdir/unikraft/include/uk/spi.h

# Pull the common lcpu headers so #include <uk/plat/common/lcpu.h> works:
mkdir -p workdir/unikraft/include/uk/plat/common
cp -r workdir/unikraft/plat/common/include/uk/plat/common/*.h \
      workdir/unikraft/include/uk/plat/common/

cd workdir/unikraft/plat && echo '$(eval $(call import_lib,$(UK_PLAT_BASE)/raspi))' >> Makefile.uk && cd ../../..

cd workdir
git clone https://github.com/unikraft/lib-lwip.git libs/lwip && cd libs/lwip && git checkout e20459c47a6b5ab16967c15b220686c5be50d4d7 && cd ../..
git clone https://github.com/unikraft/lib-musl libs/musl && cd libs/musl && git checkout fd1abc9257b40a74c69b3a40467a453bf8892439 && cd ../..
cd ..

mkdir rootfs
touch rootfs/test.txt

cd rootfs && find -depth -print | tac | bsdcpio -o --format newc > ../initrd.cpio && cd ..

cp ../spi-config .config
cp ../../spi/main.c ./main.c

echo 'CONFIG_UK_APP="'$(pwd)'"' >> .config
echo 'CONFIG_UK_BASE="'$(pwd)/workdir/unikraft'"' >> .config
echo 'CONFIG_LIBVFSCORE_AUTOMOUNT_EINITRD_PATH="'$(pwd)/initrd.cpio'"' >> .config

sed -i '4s|.*|LIBS := $(UK_LIBS)/musl:$(UK_LIBS)/lwip|' Makefile

make -j16 all

cp ./workdir/build/kernel8.img ../kernel8.img
