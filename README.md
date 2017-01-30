# MUP Astro CAT

The MUP Astro CAT is an auto GPIO configuring add-on board for the
Raspberry PI 3 which provides additional hardware interfaces to 
communicate and control a Meade Autostar, Moonlite focuser and
temperature sensors.

![MUP Astro CAT](https://bitbucket.org/BWGaryP/mup-astro-cat/raw/master/docs/mup_astro_cat_on_pi.jpg)

This repository contains the firmware and driver project files
needed to build and use the MUP Astro CAT.

Kicad hardware schematic and pcb files are available at:

   https://bitbucket.org/BWGaryP/mup-astro-cat-hardware/

Please refer to the additional documentation in the docs/ directory for
information on build/installation instructions and testing notes.

CAT refers to the Schmidt-Cassagrain telescope (CAT for short) this board will
be attached to. After naming it CAT I considered the recursive "CAP" which
would stand for "CAP Attached on Pi" since this project doesn't quite meet
the "Hat" spec, a cap is not a hat! but that means renaming things so CAT it is.

# Directories

  * docs        - project documentation
  * indi_driver - libindi drivers (GPL3)
  * eeprom      - eeprom configuration and overlay source (GPL2)

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

# Hardware Compatability

Version 1.x of the firmware/software is compatible with any revision of
Version 1 of the hardware. Breaking changes will result in a major version
bump.

# Copyright

Copyright Gary Preston 2016.

