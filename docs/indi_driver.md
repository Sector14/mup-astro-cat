# About

The MUP Astro CAT INDI lib driver exposes the Moonlite focuser controller
as a standard INDI focuser.

It supports the standard relative and absolute position setting, timer based
motion, variable speed from 1 to 250 steps per second and runtime configurable
travel limits as well as controller fault status reporting.

# Travel limits

By default the limits are set to between 0 and 7000 steps. At runtime this can
be configured to any value between 0 and 65000. The upper limit should be set
according to your drawtube size based on 6135 steps per inch.

The focuser should be fully racked in prior to connection. This will ensure
the 0 position is correct.

If you have any accessories (a lodestar guide camera in my case) that prevent
fully racking in, first set the focuser to the default 0 position, then set
the abs position to step the focuser out far enough to attach the accessory.

Determine the minimum position you can now move the focuser in whilst leaving a
little wiggle room and set this as the min travel position in the OPTIONS tab.

In my case, I have a travel range of 0..7000 but with the lodestar attached 
I set the minimum to 1000 and prior to focusing my scope I set the moonlite
focuser to 4000, the midpoint of my available focus range 1000..7000. Coarse
focusing can now occur before moving onto the automated focusing routines.

The driver will never exceed the min/max travel limits whether you're moving
via setting absolute position, a relative move or a timer based moved. In 
addition, once supported, adjustments due to temperature correction will also
not allow limits to be exceeded.

# Faults

Should the FAULT indicator turn red, the DRV8805 has signaled a fault. This
may be due to overheating or other conditions (see datasheet). The indicator
will turn green once the fault has cleared.
