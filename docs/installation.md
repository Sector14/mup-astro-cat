# Installation

It is assumed you have already built the MUP Astro CAT indi driver and
installed it into the correct INDI directory. Please refer to building.md
if not.

## GPIO Access

The MUP Astro CAT INDI driver requires the WiringPi library installed. Version
2.29 or greater is required. If wiringPi is not available in your repository
refer to building.md for instructions on manually building/installing it.

WiringPi requires access to the gpio pins, the memory mapping of which is
exposed via /dev/gpiomem 

    adduser <youruser> gpio
  
gpiomem access may need an env var setting to enable wiringPi to use it, add
to .bashrc

    export WIRINGPI_GPIOMEM=1

## Serial

The built in serial console needs to be disabled to free up the uart for the
autostar connection. edit /boot/cmdline.txt and change the console=... entry
to console=tty1 and add enable_uart=1 to /boot/config.txt.

The hat eeprom will ensure bluetooth is disabled and uart remapped, but cannot 
disable the modem service which uses uart0. To disable it use:
    
    systemctl disable hciuart

After rebooting "dmesg | grep tty" should show ttyAMA0 is a PL0011 and /dev/serial0
should be linked to ttyAMA0.

## Wifi

in /etc/network/interface


    allow-hotplug wlan0
    iface wlan0 inet dhcp
       wpa-conf /etc/wpa_supplicant/wpa_supplicant.conf

In wpa_supplicant.conf

    network={
	  ssid="WIFI"
	  psk=ENCODED_PASS
	  key_mgmt=WPA-PSK
	  pairwise=CCMP	
    }

Pass encoded with

    wpa_passphrase SSID passphrase

