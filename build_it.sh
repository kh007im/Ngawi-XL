#!/bin/bash
TOOLCHAIN="/home/MattKhojim/android/toolchain/linaro-4.7-2013.10/bin/arm-eabi"
MODULES_DIR="../modules"
KERNEL_DIR="/home/MattKhojim/android/ngawi-xl-k"
make ARCH=arm CROSS_COMPILE=$TOOLCHAIN- akp_sa77_defconfig
make ARCH=arm CROSS_COMPILE=$TOOLCHAIN- -j4
if [ -a $KERNEL_DIR/arch/arm/boot/zImage ];
then
echo "Copying modules"
find . -name '*.ko' -exec cp {} $MODULES_DIR/ \;
cd $MODULES_DIR
echo "Stripping modules for size"
$TOOLCHAIN-strip --strip-unneeded *.ko
cd $KERNEL_DIR
else
echo "Compilation failed! Fix the errors!"
fi
