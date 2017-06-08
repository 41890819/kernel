#!/bin/bash

make zImage -j16
cp arch/mips/boot/compressed/zImage ../device/ingenic/$1/kernel
cd ..
make bootimage -j16
