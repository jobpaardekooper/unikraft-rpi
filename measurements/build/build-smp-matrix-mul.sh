#!/bin/bash

git clone https://github.com/unikraft/app-helloworld && cd app-helloworld

mkdir workdir && git clone -b RELEASE-0.16.3 https://github.com/unikraft/unikraft.git workdir/unikraft

git clone https://github.com/jobpaardekooper/unikraft-rpi.git workdir/unikraft/plat/raspi

mkdir workdir/unikraft/include/uk/intctlr/
mv workdir/unikraft/plat/raspi/include/uk/intctlr/limits.h workdir/unikraft/include/uk/intctlr/limits.h

# Pull the common lcpu headers so #include <uk/plat/common/lcpu.h> works:
mkdir -p workdir/unikraft/include/uk/plat/common
cp -r workdir/unikraft/plat/common/include/uk/plat/common/*.h \
      workdir/unikraft/include/uk/plat/common/

cd workdir/unikraft/plat && echo '$(eval $(call import_lib,$(UK_PLAT_BASE)/raspi))' >> Makefile.uk && cd ../../..

cd workdir
git clone https://github.com/unikraft/lib-musl libs/musl && cd libs/musl && git checkout fd1abc9257b40a74c69b3a40467a453bf8892439 && cd ../..
cd ..

mkdir rootfs
touch rootfs/test.txt

cd rootfs && find -depth -print | tac | bsdcpio -o --format newc > ../initrd.cpio && cd ..

cp ../smp/smp-matrix-mul-config .config
cp ../../smp/matrix-multiplication/main.c ./main.c

sed -i '4s/.*/LIBS := $(UK_LIBS)\/musl/' Makefile

echo 'CONFIG_UK_APP="'$(pwd)'"' >> .config
echo 'CONFIG_UK_BASE="'$(pwd)/workdir/unikraft'"' >> .config
echo 'CONFIG_LIBVFSCORE_AUTOMOUNT_EINITRD_PATH="'$(pwd)/initrd.cpio'"' >> .config

make -j16 all

cp ./workdir/build/kernel8.img ../kernel8.img

