# dr55sync
This project is creating DR-55 MIDI sync board.

BOSS DR-55, Dr. Rhythm, is a programable rhythm machine manuafactured by Roland in 1979. This projects brings MIDI conenectibity to DR-55.
This project includes schematics and sketch for Arduino.

Watch demo movie at YouTube https://www.youtube.com/watch?v=whlHrn3-OU4

+++ Features

+ MIDI clock slave sync  
DR-55 sync to the MIDI clock from external rhythm machine or sequencer.

+ Manual play by MIDI  
You can play DR-55 manualy from external keyboard or sequencer by MIDI note message. Hi-hat supports variation of open/close/pedal.

+ Velocity controls Accent  
When the velocity value is larger than threshold, accent signal turn on.  Threshold value is defined in source code.

+ Omni Mode Switch  
Receive MIDI channel is 10 in default. While the switch sets on, every midi channel is recognized. Receive channel is specified in the source code.

+++ MIDI Implementation
<pre>
Receive Messages

Channel Messages

Note Message     9x nn vv    (Hex)
  x … midi channel     
      1-16 : When the Omni switch is on
      10   : When the Omni switch is off 
  nn … note number 
      48 = BD
      50, 52 = SD
      51 = RS
      54 = Close HH
      56 = Pedal HH
      58 = Open HH
  vv …velocity
      0 : no sound
      1 - 89 : sound without accent
      90 - 127 : sound with accent
      Threshold is specified in source code.

System Real Time Messages

MIDI Clock        F8  (Hex)
Start             FA  (Hex)
Stop              FC  (Hex)

</pre>
+++ Hardware

+ Using Arduino Pro Mini compatible board (5V 16MHz)

+ Power is provided from DR-55.

+ The board is connected to DR-55 with 10 core flat cable. Board side of the cable is connector. DR-55 side of cable should be soldered to the DR-55 main board directory. No additional parts or special modification is required in DR-55 side.

+ Switch 1 of the DIP SW is a program switch of Arduino. Switch 2 - 4 can be used any setting.

// End of file
