# doxeo-device
All doxeo-device to connect to doxeo

## Set Arduino Pro Mini to 1Mhz

* Copy the file ATmegaBOOT_168_atmega328_pro_1MHz.hex into AppData\Local\Arduino15\packages\arduino\hardware\avr\1.6.23\bootloaders\atmega
* Copy the lines below into : AppData\Local\Arduino15\packages\arduino\hardware\avr\1.6.23\boards.txt
```
## Arduino Pro or Pro Mini (1.8V, 1 MHz) w/ ATmega328
## --------------------------------------------------
pro.menu.cpu.1MHzatmega328=ATmega328 (1.8V, 1 MHz)

pro.menu.cpu.1MHzatmega328.upload.maximum_size=30720
pro.menu.cpu.1MHzatmega328.upload.maximum_data_size=2048
pro.menu.cpu.1MHzatmega328.upload.speed=9600

pro.menu.cpu.1MHzatmega328.bootloader.low_fuses=0x62
pro.menu.cpu.1MHzatmega328.bootloader.high_fuses=0xD4
pro.menu.cpu.1MHzatmega328.bootloader.extended_fuses=0xFE
pro.menu.cpu.1MHzatmega328.bootloader.file=atmega/ATmegaBOOT_168_atmega328_pro_1MHz.hex

pro.menu.cpu.1MHzatmega328.build.mcu=atmega328p
pro.menu.cpu.1MHzatmega328.build.f_cpu=1000000L
```
* Upload the ArduinoISP sketch on your Arduino UNO
* Hook up your pro mini like shown in the diagram
![ProMini](/Arduino/1mhz/pro_mini.png)
* Select "Arduino as ISP" in the Programmers menu
* Select "Arduino Pro or Pro Mini" and "ATmega328 (1.8V, 1 MHz)"
* Hit "Burn bootloader"
* More info [here](https://forum.pimatic.org/topic/383/tips-battery-powered-sensors)