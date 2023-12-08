# basic-enthalpymeter-arduino
NOTE: a lot to be verified in this instructions!
This is provided "as is" and the author is not responsible!

## Pins:
Button: pin 2  
LED: pin 4  
Internal Sensor: pin A0  
External Sensor: pin A1  

## General instructions:
The sensors are NTC 10k connected to the ground, in series with a resistor to the VCC, 
and the mid point connects to the analog pin  
Use 1k pull down for the button  
Connect the LED GND to the GND and the VCC to a resistor that goes to the pin 4 
(suggested resistor of 1k for a red LED)  
Default amount of hot water for calibration: 100 grams  
The LED turns on to indicate less than 1 ºC oscilation in the last 2 seconds, 
than you can press the button to the next step  
Ensure to always use the same total amount of water used in calibration for the 
measurement process  
  
## When the Arduino start running, it requires calibration:  
1: put the water at normal temperature in the enthalpymeter recipient  
2: wait temp stabilize
3: press the button (it saves the initial temp a goes to the next step)  
4: with the external sensor, measure the hot water (in another recipient)  
5: when its stable, press the button to save this temp  
6: add the hot water in the enthalpymeter recipient  
7: when the temp is stable, press the button to save the final temp  
It will return the calibrated thermal capacity (refering to the entire amount of water 
plus the enthalpymeter) in the serial  

## After calibration, you can repeat the measurement any amount of times:  
1: press the button to start
2: when the temp is stable in the enthalpymeter, press the button to save the initial temp  
3: make whatever is the experiment  
4: when temp is stable, press the button to set it as the final temp  
This will return throught the serial the amount of heat changed within the enthalpymeter  
