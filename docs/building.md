# Building

The focuser and temperature sensor parts of the MUP Astro CAT are controlled 
via a custom Indi driver. In order to build this you will also need wiringPi 
installed (version 2.29 or greater) and indi-dev.

The CAT also contains an EEPROM for auto configuration of the GPIO pins
in the same way a regular Raspberry PI hat works. Please refer to the EEPROM
Programming section below for instructions on compiling the device tree and
flashing to the EEPROM on the CAT.

After building the drivers and programming the EEPROM, refer to installation.md
for additional PI configuration instructions.


## Wiring PI

GPIO pin control is done via the wiringPi library. You will need version 2.29
or greater installed either from the raspbian reposistory or built yourself via
the wiring pi git repository.

    git clone git://git.drogon.net/wiringPi
    cd wiringPi
    ./build


## INDI

I assume you have already installed INDI from your repository or manually
built from source along with the relevant 3rd party indi drivers. Refer to 
http://indilib.org for instructions on building/installing INDI.

Building the MUP Astro CAT driver follows a similar process.

... TODO ...


## EEPROM Programming

When building this board yourself, the EEPROM will need flashing with a suitable
device tree. 

In order to build and flash the eeprom, the PI will need a checkout of the
hats.git repository which contains the eepromutils:

    git clone https://github.com/raspberrypi/hats.git
    cd hats/eepromutils
    make clean && make

Also install the device-tree-compiler

    apt-get install device-tree-compiler

The eeprom format is described in the hats/eeprom-format.md however for
the MUP Astro Hat purposes, you just need to make the eep file using the
configured eeprom_settings.txt and include a couple of extra overlays
to configure 1-wire and disable the bluetooth module in order to free
up the hardware uart for autostar usage.
    
    dtc -@ -I dts -O dtb -o mup_astro_cat.dtb mup-astro-cat.dts
    eepmake eeprom_settings.txt mup_astro_cat.eep mup_astro_cat.dtb
    eepflash -w -f=mup_astro_cat.eep -t=24c32

Don't forget to remove the write protect jumper. 

If all goes well, reboot the pi and look in /proc/device-tree/hat/

It's recommended to erase the eeprom prior to flashing the eep which can
be done by generating a 4096 byte empty eep file and flashing.

    dd if=/dev/zero of=empty.eep bs=1024 count=4

If you're curious about how overlays work documentation can be found at:

  * http://www.raspberrypi.org/documentation/configuration/device-tree.md
  * https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README
  
Note: the overlay is based on pi3-\*-bt-overlay.dts and w1-gpio-overlay.dts from 

   https://github.com/raspberrypi/linux/tree/rpi-4.4.y/arch/arm/boot/dts/overlays

You can optionally keep bluetooth enabled by basing off the pi3-miniuart-bt-overlay instead.
It will be using the software uart and requires a fixed core clock. See overlays
readme for more information in the raspberrypi repo.


