#!/bin/bash
# build-i2c-shim.sh — build Unikraft image with BCM2835 GPIO + I2C FreeBSD shim
#
# Both LIBBCM2835_GPIO and LIBBCM2835_I2C are enabled.
# The I2C driver (bcm2835_bsc.c) is layered on top of the same KOBJ shim
# infrastructure as the GPIO driver.
#
# Run from inside the Docker build container (repo mounted at /workspace).

set -e

REPO=/workspace
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

rm -rf app-helloworld

git clone https://github.com/unikraft/app-helloworld && cd app-helloworld

mkdir workdir && git clone -b RELEASE-0.16.3 \
    https://github.com/unikraft/unikraft.git workdir/unikraft

git clone https://github.com/Mihnea0Firoiu/unikraft-rpi.git \
    -b gpio_native workdir/unikraft/plat/raspi

# Overlay local (uncommitted) shim files from the mounted repo.
RASPI_PLAT=workdir/unikraft/plat/raspi
cp -r ${REPO}/drivers/.   ${RASPI_PLAT}/drivers/
cp    ${REPO}/Config.uk   ${RASPI_PLAT}/Config.uk
cp    ${REPO}/Makefile.uk ${RASPI_PLAT}/Makefile.uk

mkdir -p workdir/unikraft/include/uk/intctlr/
cp ${RASPI_PLAT}/include/uk/intctlr/limits.h \
   workdir/unikraft/include/uk/intctlr/limits.h

# Public API headers
cp ${RASPI_PLAT}/drivers/include/uk/gpio.h \
   workdir/unikraft/include/uk/gpio.h
cp ${RASPI_PLAT}/drivers/include/uk/i2c.h \
   workdir/unikraft/include/uk/i2c.h

mkdir -p workdir/unikraft/include/uk/plat/common
cp -r workdir/unikraft/plat/common/include/uk/plat/common/*.h \
      workdir/unikraft/include/uk/plat/common/

cd workdir/unikraft/plat && \
    echo '$(eval $(call import_lib,$(UK_PLAT_BASE)/raspi))' >> Makefile.uk && \
    cd ../../..

# Copy unmodified FreeBSD drivers from the overlay (already in the repo).
# bcm2835_gpio.c and bcm2835_bsc.c are committed under drivers/freebsd/.
mkdir -p ${RASPI_PLAT}/drivers/freebsd

cd workdir
git clone https://github.com/unikraft/lib-lwip.git libs/lwip && \
    cd libs/lwip && git checkout e20459c47a6b5ab16967c15b220686c5be50d4d7 && cd ../..
git clone https://github.com/unikraft/lib-musl libs/musl && \
    cd libs/musl && git checkout fd1abc9257b40a74c69b3a40467a453bf8892439 && cd ../..
cd ..

mkdir -p rootfs && touch rootfs/test.txt
cd rootfs && find -depth -print | tac | bsdcpio -o --format newc > ../initrd.cpio && cd ..

cp ${SCRIPT_DIR}/i2c-shim-config .config
cp ${REPO}/measurements/i2c-shim/main.c ./main.c

echo 'CONFIG_UK_APP="'$(pwd)'"' >> .config
echo 'CONFIG_UK_BASE="'$(pwd)/workdir/unikraft'"' >> .config
echo 'CONFIG_LIBVFSCORE_AUTOMOUNT_EINITRD_PATH="'$(pwd)/initrd.cpio'"' >> .config

sed -i '4s|.*|LIBS := $(UK_LIBS)/musl:$(UK_LIBS)/lwip|' Makefile

make -j$(nproc) all

cp ./workdir/build/kernel8.img ${SCRIPT_DIR}/kernel8.img
echo "Build complete: ${SCRIPT_DIR}/kernel8.img"
