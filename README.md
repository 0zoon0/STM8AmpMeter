# STM8AmpMeter
Turn 3 digits STM8 voltmeter into amp meter

I've got a 3 digits voltmeter based on STM8 processor and a 7-segment LED common anode indicator. It used a resistor divider and was showing 1 for 0.075mV reading on pin 2 of STM8. I managed to turn it into an amp meter, by shorting the top resistor in the resistor divider and using a 0.075mOhm shunt to measure voltage drop and getting amp readings, until something bad happened and pin 2 died.

I have removed pin 2 and made a link to pin 3. I had to re-write the firmware to use the new pin instead of the dead one. The old firmware was lost as it was locked. This code is based on https://eddy-em.livejournal.com/80558.html project, that used almost the same voltmeter, with different wirings and turned it into thermometer.

The code is a ST Visual Develop project.
