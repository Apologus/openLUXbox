#!/bin/sh
for gametemp in Gamecode/*
do
    gamedir=$(basename ${gametemp})
    gamename=${gamedir//_/ }
    gamename=${gamename//CanoLbalt/canoLbalt}
    arduino-cli compile -v -e -b esp8266:esp8266:nodemcu Gamecode/${gamedir}/
    mv "Gamecode/${gamedir}/build/esp8266.esp8266.nodemcu/${gamedir}.ino.bin" "docs/firmware/${gamename}/${gamename}.bin"
    arduino-cli compile -v -e -b esp32:esp32:mhetesp32minikit Gamecode/${gamedir}/
    mv "Gamecode/${gamedir}/build/esp32.esp32.mhetesp32minikit/${gamedir}.ino.bootloader.bin" "docs/firmware/${gamename}/${gamename}.ino.bootloader.bin"
    mv "Gamecode/${gamedir}/build/esp32.esp32.mhetesp32minikit/${gamedir}.ino.partitions.bin" "docs/firmware/${gamename}/${gamename}.ino.partitions.bin"
    mv "Gamecode/${gamedir}/build/esp32.esp32.mhetesp32minikit/boot_app0.bin" "docs/firmware/${gamename}/boot_app0.bin"
    mv "Gamecode/${gamedir}/build/esp32.esp32.mhetesp32minikit/${gamedir}.ino.bin" "docs/firmware/${gamename}/${gamename}_ESP32.bin"
    rm -r "Gamecode/${gamedir}/build"
done
