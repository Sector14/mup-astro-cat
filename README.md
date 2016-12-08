# MUP Astro Hat

The "MUP Astro Hat" is licensed under the CERN Open Hardware License v1.2 
and based upon the MIT licensed RPi_Hat template by xesscorp. Software
and Firmware are licensed under the GPL3.

# Directories

  * control_hat - Kicad project incl schematic/pcb layout for the MUP Astro Hat.
  * docs - hardware/software project documentation and 3rd party datasheets
  * gerber - Exported gerber files for re-ordering PCBs
  * software - libindi drivers
  * firmware - eeprom overlay source

# Intended Purpose

Use a raspberry pi and additional interface board to control the 
following devices:

  * SXVR-H9
  * LodeStar
  * Filter Wheel
  * LX90 Handset
  * Moonlite Focuser
  * Illuminated Reticle (v2?)
  * Temp Sensor

The SXVR-H9, LodeStar (excl ST4 guide port) and Filter Wheel simply 
connect as USB devices to the Pi. 

The LX90 Handset uses a RS232 connection via RJ22 to the PI.

Moonlite focuser controlled via GPIO to a unipolar stepper
motor controller that will handle signal timing. Ensures any 
jitter/pauses from pi due to non realtime OS won't impact
motor control.

ST4 port from APM is not implemented, it might be added in a future
revision via i2c bus however as the LX90 supports pulse commands
over serial, it may not be needed.

# Library Setup

In addition to the libraries, modules and models that ship with Kicad,
this project also uses the MUP Kicad Library for additional parts and
an optional 3rd party 3D model library.

## Eeschema

If you receive a "The following libraries were not found:" message when
opening the schematic then you do not have the MUPS Kicad library available.
Kicad will use a cached copy of the library so this step is optional, but 
if you wish to make changes you should clone the repo:

  https://bitbucket.org/BWGaryP/mup-kicad-library

and add its location "Preferences/Component Libraries" as a user defined 
search path in the "parts library editor".  Then close and re-open Eeschema
and all the schematic symbols should now be found.

## Footprints

The above repo also contains footprint files for PCBNew in the modules/
directory and 3d models (where applicable). PCBNew will need each modules/\*.pretty
directory adding to the global library, or if you prefer, the project specific 
libraries.

To do so, open PCBNew, "Preferences/Footprints Libraries Manager", then 
"Append with Wizard" and select each ".pretty" directory and click Next, then
select whether to add the footprint to the global or project library.

Note: Some footprints are unchanged Kicad footprints that have only been
saved to the MUPS library in order to associate a 3d model with the footprint.
If the official Kicad library ever adds the missing models, the parts will
likely be removed from this repo.

## 3d Models

Some parts include a reference to models from the GPL'd 3d library by "kcswalter".
These are optional models and not included in the MUPS Kicad library.

If you checkout the walter repo from

  git://smisioto.eu/kicad_libs.git 

You can have Kicad use these models during 3d preview mode
by adding a new env variable in PCBNew "Preferences/Configure Paths" 
called WALTER_KI3DMOD and with the path set to the 
<walter repo checkout path>/modules/packages3d/ directory. 

Any footprints in the MUPS library that has the 3d model path prefixed 
with {WALTER_KI3DMOD}/walter/ will then use the appropriate 3D model.
The same will apply to {MUPS_KI3DMOD}/mups/ although at this time I've
yet to create any custom 3D models and find it unlikely I will

# Pinouts

## Pi Hat RS232

Looking into port, locking tab on bottom, left to right:-

  1. GND
  2. NC
  3. Tx
  4. Rx

## Autostar RS232 Port

Looking into port with locking tab on bottom, left to right:- 

  * GND
  * NC
  * Tx
  * Rx

Note: Tx and Rx should cross over in cable so Tx on PC is Rx on Autostar.

## Moonlite Focuser

    Model: LSG35012F76P
    Voltage: 12v
    Resistance/Phase: 65.0 Ohm
    Inductance H/Phase: 27.0
    Hold Current: 0.369A
    Total Input Watt: 4.5
    Min Holding Torque: 420 gf-cm (6.3 oz/in)
    Pull out Torque @ 175p/s: 151 gf-cm (2.1 oz-in)
    Detent Torque: 130 gf-cm (1.8 oz-in)
    Rotor inertia: 6.8 gf-cm^2 (0.372 oz-in^2)
    Weight: 82g (2.9oz)
    Length: 22mm (0.866in)

|Part No.     | Similar | Step Angle Degrees | Steps Per Revolution | Reduction | Nominal Voltage VDC | Min Holding Torque (oz-in)| Pull-out Torque @175p/s (oz-in) | Detent Torque (oz-in) | Rotor Inertia (oz-in) | Weight (oz) | Length (in) |
|LSG35012F76P |3004-003 |0.100 |3600 |75 |12 |150 |101 |150.0 |0.0372 |8.6 |1.55

Looking into 6p6c port on Pi Hat, locking tab on bottom, left to right:-

  1. 12v
  2. Out1
  3. Out2
  4. Out3
  5. Out4
  6. No connect

The moonlite focuser uses a DB9 connector with the following pin mapping:-

  [TODO]
 
# PI Setup

To disable built in serial console, edit /boot/cmdline.txt and
remove the console=... entry. Or use raspi-config utility and 
select disable serial console.

add to boot/config.txt enable_uart=1 and dtoverlay=pi3-disable-bt

This will ensure the bluetooth module does not get the real HW uart
and maps it back to the gpio pins we need. Without this the PI exposes
a miniuart.

Serial Port for autostar uses
  * Baud: 9600
  * Data: 8
  * Stop: 1
  * Parity: None
  * Order: LSB

You also need to disable the modem which tries to use uart0

    systemctl disable hciuart

The hat should configure overlay, but cannot disable the modem.

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


# Manually Testing

## Autostar

    minicom -D /dev/ttyAMA0 -b 9600 -8 -o

Set HW and SW flow control off:

    ctrl-A Z
    o
    Serial port
    F
    G

Tx and Rx on head pin or the Autostar connector should now
be decodable on a scope. Or you can sort circuit Tx and Rx
on the RS232 auto star connector and should see any keypress
echo'd back.

## Focuser

Using "gpio" on the command line of the Raspberry Pi.

Write Pins:

    Pin#  Func    BCM  GPIO
    ------------------------
    40  - /Enable 21   29
    38  - Reset   20   28  
    37  - SM1     26   25
    36  - SM0     16   27
    35  - DIR     19   24
    33  - STEP    13   23


Read Pins:

    Pin# Func     BCM  GPIO
    -----------------------
    32 - /HOME    12   26
    31 - /FAULT   6    22

When using "gpio" you can specify GPIO (or BCM with -g switch) 
pin numbers. Use "gpio readall" for a mapping table showing
physical pin#, BCM and GPIO numbers.

./setup_drv8805.sh

```bash
#!/bin/sh

# header pin num. 
nENABLE=29
RESET=28
SM1=25
SM0=27
DIR=24
STEP=23

gpio mode ${nENABLE} out
gpio mode ${RESET} out
gpio mode ${SM1} out
gpio mode ${SM0} out
gpio mode ${DIR} out
gpio mode ${STEP} out

# Enter Reset
gpio write ${RESET} 1

# Set enabled, full step and CCW dir
gpio write ${nENABLE} 0
gpio write ${SM1} 0
gpio write ${SM0} 0
gpio write ${DIR} 0
gpio write ${STEP} 0

# Exit reset
gpio write ${nENABLE} 0

gpio readall
```

Optionally change DIR or step type (SM0 & 1) by writing to pin DIR=24, SM0=27 and SM1=25 


./step_drv8805.sh

```bash
#!/bin/sh

# header pin num. 
STEP=23
nHOME=26

echo Stepping once

gpio write ${STEP} 1
sleep 0.25
gpio write ${STEP} 0

echo /HOME is: 
gpio read ${nHOME}
```

# Gerber Export

For Ordering PCBs from dirtypcbs.com

For "plotting":-

  * "Gerber" format is selected.
  * Select Layers F.Cu, B.Cu, F.SilkS, B.Mask, F.Mask, Edge.Cuts
  * Select Options:
   * Plot footprint values
   * Plot footprint references
   * Exclude PCB edge layer from other layers
  * Select Gerber Options:
   * User Protel filename extensions

For Drill file:

  * Units Inches
  * Zeros Format, Suppress leading zeros
  * Drill Map File Format (not used)
  * Drill File Options:
   * Minimal header
   * Merge PTH and NPTH holes into one file
  * Drill origin, Absolute

After export, rename the edge cuts "gm1" extension to "gml" prior
to uploading zip to dirtypcbs.com.

# Future Revision

Selecting surface 1206 style footprint for C8, the 100uF bypass capacitor 
for the DRV8805 was a little costly due to needing higher than 12V. An 
SMD electrolytic would prove significantly cheaper £0.27 vs £2.46
and with a greater voltage margin and not require that much additional 
space for a radial footprint. Lapse in judgement to be corrected on a 
future revision.

The power supply could be reduced in size with a more carefully
selected inductor. Having never laid out a SMPS however, I decided
to stick to the recommended layout and inductors based on the
TI regulator used. Further experimentation will be done prior to
revision 2 along with a change to SMD electrolytics and diodes.

A surface mounting socket to mate the hat to the pi would be a better
choice as it allows an extended pin leg header to be mounted to the
pi and then the hat mounted on top. This works out cheaper than the
elevated sockets.

Signal lines all need test point locations making available.

Switch 4p4c and 6p6c connector layout to socket mounting 
to allow panel mount option or in the case of a direct wiring, a
smaller connector area.

USB ideally should also be panel mount but desoldering the PI's
dual sockets is something I'd prefer to avoid.

ST3232 switch to soic version.

Add a via behind the regulator to connect ground planes for better
thermal relief.

# Reference

  * http://www.beckwithelectronics.com/HURST/LSG35012F76P.htm
  * http://www.weasner.com/etx/techtips/2007/aux-port.html
  * http://www.weasner.com/etx/archive/mar12/autostar.html
  * https://github.com/raspberrypi/hats
  * https://github.com/raspberrypi/hats/blob/master/designguide.md
  * https://github.com/xesscorp/RPi_Hat_Template
  * http://www.briandorey.com/post/Raspberry-Pi-3-UART-Boot-Overlay-Part-Two

# Copyright

Copyright Gary Preston 2016.

This documentation describes Open Hardware and is licensed under the
CERN OHL v. 1.2.

You may redistribute and modify this documentation under the terms of the
CERN OHL v.1.2. (http://ohwr.org/cernohl). This documentation is distributed
WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF
MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A
PARTICULAR PURPOSE. Please see the CERN OHL v.1.2 for applicable
conditions


