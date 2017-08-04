/* 
  RD-55 MIDI sync version 2.1

created 10 March 2017
updated 12 April 2017
updated 29 July  2017
by Takao Watase

This is free software released into the public domain.

*/

// select circuit board version

//#define PCB_VERSION_1_0    // version 1.0
#define PCB_VERSION_2_1    // version 2.1

// constants
#define TPQN24  6     // number of F8 clock when TPQN=24
#define TPQN48  12    // number of F8 clock when TPQN=48
#define LED_ON HIGH
#define LED_OFF LOW

// Length of control pulse (msec)
#define DUR_BD 20     // BD gate time
#define DUR_SD 20     // SD gate time
#define DUR_RS 20     // RS gate time
#define DUR_AC 30     // gate time of Accent
#define DUR_HH_CLOSE 5   // gate time for close HH
#define DUR_HH_PEDAL 35  // gate time for pedal HH
#define DUR_HH_OPEN 150  // gate time for open HH
#define WIDTH_CLOCK 20   // gate time of tempo clock
#define WIDTH_LED 50     // gate time for led

#define THRES_AC    90   // threshold velocity for accent

#ifdef PCB_VERSION_2_1   // PCB Version 2.1
// I/O pins            pin# of Connector  -   DR-55 components
#define IO_HH 10    // 2                      S4 Common
#define IO_RS 11    // 3                      TC5501P  pin 12
#define IO_SD 12    // 4                      TC5501P  pin 14
#define IO_BD 13    // 5                      TC5501P  pin 16
#define CLOCK 14    // 6                      TC4011UB pin 13
#define ST 15       // 7                      TC4011UB pin 3
#define IO_AC 16    // 8                      TC5501P  pin 10
#define SW_OMNI  9  // Omni mode switch

// pin# of LEDs
#define LED_MIDI 4  // midi/clock indicator
#define LED_RUN  5  // light while running clock
#endif 

#ifdef PCB_VERSION_1_0   // PCB Version 1.0
// I/O pins            pin# of Connector  -   DR-55 components
#define IO_HH 10    // 2                      S4 Common
#define IO_RS 11    // 3                      TC5501P  pin 12
#define IO_SD 12    // 4                      TC5501P  pin 14
#define IO_BD 13    // 5                      TC5501P  pin 16
#define CLOCK 17    // 6                      TC4011UB pin 13
#define ST 16       // 7                      TC4011UB pin 3
#define IO_AC 15    // 8                      TC5501P  pin 10
#define SW_OMNI  9  // Omni mode switch

// pin# of LEDs
#define LED_MIDI 4  // midi/clock indicator
#define LED_RUN  5  // lit while running
#endif 

// midi status
#define MIDI_START    0xfa
#define MIDI_STOP     0xfc
#define MIDI_CLOCK    0xf8
#define MIDI_NOTE     0x90
#define MIDI_STATUS   0x80
#define MIDI_CH       10    // receive channel , 10 = rhythm ch.

#define isStatus(data) (data >= 0x80)   // check status byte
#define getMidiCh(ch) (ch - 1)          // midi channel start from 1, but actual value start from 0.

// variables
int cntClock = 0;     // count F8
int cntClkLed = 0;    // count clock led

bool isRun = false;   // true while running clock
bool reqRun = false;  // request to run
bool isMidiOmni = false;  // true while omni sw on

int tpqn = TPQN24;    // Ticks Per Quater Note means number of clock pulse for quarter note.

int fData;            // waiting data type
const int flag_st = 0;    // waiting 1st byte as status
const int flag_d1 = 1;    // waiting 2nd byte as data 1
const int flag_d2 = 2;    // waiting 3rd byte as data 2
byte key;             // note number
byte vel;             // velocity

// timers
// use Metro library
#include <Metro.h>

Metro tmLed = Metro(WIDTH_LED);       // midi/clock led indicator
Metro tmClock = Metro(WIDTH_CLOCK);   // clock pulse width
Metro tmBD = Metro(DUR_BD);           // duration BD
Metro tmSD = Metro(DUR_SD);           // duration SD
Metro tmRS = Metro(DUR_RS);           // duration RS
Metro tmAC = Metro(DUR_AC);           // duration AC
Metro tmHH = Metro(DUR_HH_CLOSE);     // duration HH

// decralation of subroutines
void handleStart();                     // event handler for midi start (FA)
void handleStop();                      // event handler for midi stop (FC)
void handleNoteOn(byte key, byte vel);  // event handler for midi note messages
void Sync();                            // process F8 clock
void StartClockPulse();                 // process to start clock pulse
void StartFirstClockPulse();            // process the first clock pulse
void EndClockPulse();                   // process to end clock pulse

// programs

/*
 * the setup routine runs once when you press reset: 
 */
void setup() {
  // initialize the digital pin as an output.
  Serial.begin(31250);                // rate for MIDI
  pinMode(LED_MIDI, OUTPUT);
  pinMode(LED_RUN, OUTPUT);
  pinMode(SW_OMNI, INPUT_PULLUP);     // use internal pull-up
  /*
  following pins should be set "INPUT", set signal line open, while not running.
  */
  pinMode(CLOCK, INPUT);
  pinMode(ST, INPUT);
  pinMode(IO_BD, INPUT);
  pinMode(IO_SD, INPUT);
  pinMode(IO_RS, INPUT);
  pinMode(IO_HH, INPUT);
  pinMode(IO_AC, INPUT);
  
  digitalWrite(IO_BD, LOW);
  digitalWrite(IO_SD, LOW);
  digitalWrite(IO_RS, LOW);
  digitalWrite(IO_HH, LOW);
  digitalWrite(IO_AC, LOW);
  digitalWrite(LED_MIDI, LED_OFF);
  digitalWrite(LED_RUN, LED_OFF);

  fData = flag_st;  // waiting status
  key = vel = 0;

  // sign for start up

  for ( int i = 0; i < 4; i++ ) {
    digitalWrite(LED_RUN, LED_ON);
    delay(250); 
    digitalWrite(LED_RUN, LED_OFF);
    delay(250); 
  }  
}

/* 
 * the loop routine runs over and over again forever: 
 */
void loop() {
  // check serial rx data
  if (Serial.available() <= 0) goto TIMER_PROC;

  // processing rx data
  byte data;
  data = Serial.read();

  if (isStatus(data)) {         // check status byte
    if (data == MIDI_START) {
      handleStart();            // Start clock
    }
    else if(data == MIDI_STOP) {
      handleStop();             // Stop clock
    }
    else if(data == MIDI_CLOCK) {
      Sync();                   // process F8 clock
    }
    else if((data & 0xf0) == MIDI_NOTE) {
                                // process note status message
      if (isMidiOmni ||  ((data & 0x0f) == getMidiCh(MIDI_CH))) {  // if not omni mode, check midi channel.
//      if ((data & 0x0f) == MIDI_CH) {  // if not omni mode, check midi channel.
        fData = flag_d1;        // if the channel is valid, next data is data 1
      } else {
        fData = flag_st;        // if the channel is invalid, ignore rx till coming status byte
      }
    }
    else if((data & 0xf0) != 0xf0) {
      // channel message other than note
      fData = flag_st;  // waiting next status
    }
  }
  else {
      // process data byte
      if (fData == flag_d1) {
          // data 1 is note number
          key = data;
          fData = flag_d2;    // next data shold be data 2.
      }
      else if (fData == flag_d2) {
          // data 2 is velocity
          vel = data;
          fData = flag_d1;    // waiting data 1 if running status was available
          if (vel > 0) handleNoteOn(key, vel);  // process note on
      }
  }

/*
 * Timer process
 */
TIMER_PROC:
  // Clock pulse end
  if (tmClock.check() == 1) {
    EndClockPulse();
  }

  // sound pulse end
  if (tmBD.check() == 1) {
    digitalWrite(IO_BD, LOW);   // set pulse low
    pinMode(IO_BD, INPUT);      // set line open
  }
  if (tmRS.check() == 1) {
    digitalWrite(IO_RS, LOW);
    pinMode(IO_RS, INPUT);
  }
  if (tmSD.check() == 1) {
    digitalWrite(IO_SD, LOW);
    pinMode(IO_SD, INPUT);
  }
  if (tmHH.check() == 1) {
    digitalWrite(IO_HH, LOW);
    pinMode(IO_HH, INPUT);
  }
  if (tmAC.check() == 1) {
    digitalWrite(IO_AC, LOW);
    pinMode(IO_AC, INPUT);
  }

  // read omni mode sw
  isMidiOmni = (digitalRead(SW_OMNI) == HIGH) ? false : true;

  // midi/clock led off
  if (tmLed.check() == 1) {
    digitalWrite(LED_MIDI, LED_OFF);
  }
}

/*
 * Process F8 clock
 */
void Sync() {
  if (reqRun) {                 // if the request flag was rised, 
    StartFirstClockPulse();     //   process to start clock
    cntClock = 0;               // reset clock count
    return;
  }

                                // generating tempo clock pulse
  if (++cntClock == tpqn) {     // increment clock count and check it.
    StartClockPulse();          // start clock pulse 
    cntClock = 0;               // reset clock count
  }
}

/*
 * Process to start clock pulse
 */
void StartClockPulse() {
                                // start clock pulse (set low)
  digitalWrite(CLOCK, LOW);
  tmClock.reset();
                                  // clock led on
  if (isRun &&                    //    while running and
      cntClkLed++ % 4 == 0) {     //    per 4 clocks.
    digitalWrite(LED_MIDI, LED_ON);
    tmLed.interval(WIDTH_LED);    // start led timer
    tmLed.reset();
  }
}

/*
 * Process the first clock pulse 
 */
void StartFirstClockPulse()
{
  isRun = true;               // set status running
  reqRun = false;             // reset request flag
  cntClkLed = 0;              // reset count clock led
  pinMode(ST, OUTPUT);        // set pin as "OUTPUT" to control status line.
  digitalWrite(ST, HIGH);     // set running.
  pinMode(CLOCK, OUTPUT);     // set pin as "OUTPUT" to control clock line.
  StartClockPulse();          // generate pulse on clock line.
}

/*
 * process to stop clock pulse
 */
void EndClockPulse() {
                                  // end clock pulse (set high)
  digitalWrite(CLOCK, HIGH);
}

/*
 * midi handler routine - Start
 */
void handleStart(void)
{
  reqRun = true;                  // set request flag
  digitalWrite(LED_RUN, LED_ON);  // led on
}

/*
 * midi handler routine - Stop
 */
void handleStop(void)
{
  digitalWrite(ST, LOW);                // Set status line low.
  isRun = false;                        // set status stop
  pinMode(CLOCK, INPUT);                // open clock line
  pinMode(ST, INPUT);                   // open status line
  digitalWrite(LED_RUN, LED_OFF);       // led off   
}

/*
 * midi handler routine - note message
 */
void handleNoteOn(byte key, byte vel)
{
    int pin = 0;        // pin#
    int dur = 0;        // duration (gate time)
    switch (key) {
        case 48:        // note# of BD
          pin = IO_BD;
          dur = DUR_BD;
          tmBD.reset();
          break;
        case 51:        // note# of RS
          pin = IO_RS;
          dur = DUR_RS;
          tmRS.reset();
          break;
        case 50:        // note# of SD1
        case 52:        // note# of SD2
          pin = IO_SD;
          dur = DUR_SD;
          tmSD.reset();
          break;
        case 54:        // note# of Close HH
          pin = IO_HH;
          dur = DUR_HH_CLOSE;
          tmHH.interval(dur);
          tmHH.reset();
          break;
        case 56:        // note# of Pedal HH
          pin = IO_HH;
          dur = DUR_HH_PEDAL;
          tmHH.interval(dur);
          tmHH.reset();
          break;
        case 58:        // note# of Open HH
          pin = IO_HH;
          dur = DUR_HH_OPEN;
          tmHH.interval(dur);
          tmHH.reset();
          break;
        default:
          return;
    }

    // start accent pulse
    //   process before start note pulse
    if (vel > THRES_AC) {
      pinMode(IO_AC, OUTPUT);         // set accent line ready
      digitalWrite(IO_AC, HIGH);      // start pulse
      tmAC.reset();                   // start timer
    }

    // start note pulse
    pinMode(pin, OUTPUT);             // set note line ready
    digitalWrite(pin, HIGH);          // start pulse
    digitalWrite(LED_MIDI, LED_ON);   // set led on
    tmLed.interval(dur);              // start led off timer
    tmLed.reset();
}

// End of file

