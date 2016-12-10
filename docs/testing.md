# Manually Testing

## Autostar

Serial Port for autostar uses

  * Baud: 9600
  * Data: 8
  * Stop: 1
  * Parity: None
  * Order: LSB

Testing can be done via minicom

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

Using "gpio" on the command line of the Raspberry Pi from the wiringPi
repo

  git clone git://git.drogon.net/wiringPi
  cd wiringPi
  ./build


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

## DS18B20

The EEPROM should ensure the w1-gpio module is loaded and the
device should show up on the w1 bus:-

    cat /sys/bus/w1/devices/w1_bus_master1/w1_master_slave_count 
    1

    cat /sys/bus/w1/devices/w1_bus_master1/w1_master_slaves 
    28-000002e13c1e

To read the temperature, use the device id returned above

    cat /sys/bus/w1/devices/28-000002e13c1e/w1_slave 
    43 01 4b 46 7f ff 0d 10 bd : crc=bd YES
    43 01 4b 46 7f ff 0d 10 bd t=20187

The current temperature "t" is 20.187 C.
