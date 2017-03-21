#!/bin/sh

echo "\n\033[46;30mMake firmware : LowPower Mode\033[0m\n"
make clean
make MK_FLAGS=-DENABLE_LP_MODE
cp firmware.hex ../voice_wakeup_firmware.hex

echo "\n\033[46;30mMake firmware : Normal Mode\033[0m\n"
make clean
make MK_FLAGS=
cp firmware.hex ../voice_wakeup_firmware_normal.hex

make clean