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

Before building, please refer to "Pre-Stretch Distros" if you are using Jessie
or an earlier release.

It is useful to install indi/indi-3rdparty and mupastrocat into their own
directories in /usr/local/stow/ and then use GNU stow to setup symlinks in
/usr/local This approach allows you to easily switch between and test
different versions as well as making uninstallation a breeze.

## Wiring PI

GPIO pin control is done via the wiringPi library. You will need version 2.29
or greater installed either from the raspbian reposistory or built yourself via
the wiring pi git repository.

    git clone git://git.drogon.net/wiringPi
    cd wiringPi
    ./build

NOTE: The author of wiringPI has stopped releasing project updates. There is a
github mirror of the repo which may be of use until the project can migrate
to a suitable replacement.

## INDI

I assume you have already installed INDI from your repository or manually
built from source along with the relevant 3rd party indi drivers. Refer to
http://indilib.org for instructions on building/installing INDI.

Building the MUP Astro CAT driver follows a similar process.

  cd indi_driver/
  mkdir build; cd build
  ccmake ..

Configure install prefix /usr/local/stow/mup-astro-cat

  make

Then as root "make install" and from the stow directory "stow mup-astro-cat"

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

    dtc -@ -I dts -O dtb -o mup_astro_cat.dtb mup_astro_cat.dts
    eepmake eeprom_settings.txt mup_astro_cat.eep mup_astro_cat.dtb
    eepflash -w -f=mup_astro_cat.eep -t=24c32

Don't forget to remove the write protect jumper prior to flashing.

If all goes well, reboot the pi and look in /proc/device-tree/hat/

It's recommended to erase the eeprom prior to flashing the eep which can
be done by generating a 4096 byte empty eep file and flashing.

    dd if=/dev/zero of=empty.eep bs=1024 count=4

## Further Information

If you're curious about how overlays work documentation can be found at:

  * http://www.raspberrypi.org/documentation/configuration/device-tree.md
  * https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README
  * https://www.kernel.org/doc/Documentation/devicetree/usage-model.txt

Note: the overlay is based on pi3-\*-bt-overlay.dts and w1-gpio-overlay.dts from

   https://github.com/raspberrypi/linux/tree/rpi-4.4.y/arch/arm/boot/dts/overlays

You can optionally keep bluetooth enabled by basing off the pi3-miniuart-bt-overlay instead.
It will be using the software uart and requires a fixed core clock. See overlays
readme for more information in the raspberrypi repo.


## Pre-Stretch Distros

Due to a breaking upstream device tree change between Jessie and Stretch, the
mup_astro_cat.dts device tree file has switched to using "serial" rather than "uart"
for node names.

As a result of this change, whilst the 1-Wire configuration will still occur, remapping
of the UART to ALT0 and the two GPIO pins will no longer occur when running the latest
eeprom image on Jessie or earlier releases.

If you need to remain on Jessie or earlier, either use an eeprom built from v1.0
(commit c7784853ece16dbd0aeedfa1a500301adfb5b7bd) or patch mup_astro_cat.dst to
use the old "uart" rather than "serial" naming.

To patch, change fragment3 from

```
    serial0 = "/soc/serial@7e201000";
	serial1 = "/soc/serial@7e215040";
```

back to

```
    serial0 = "/soc/uart@7e201000";
    serial1 = "/soc/uart@7e215040";

```
