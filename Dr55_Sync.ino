#include <Btn.h>
#include <Metro.h>

/* RD-55 MIDI sync

Written by T.Watase
1 Jan. 2017

*/

#define TPQN24  6     // number of F8 clock when TPQN=24
#define TPQN48  12    // number of F8 clock when TPQN=48
#define LENPULSE 10   // length of gate pulse in msec

// I/O
#define IO_HH 10 // TC5501P  Base of Q12 茶
#define IO_RS 11 // TC5501P  pin 12
#define IO_SD 12 // TC5501P  pin 14
#define IO_BD 13 // TC5501P  pin 16
#define CLOCK 14 // TC4011UB pin 13
#define ST 15    // TC4011UB pin 3 青
#define IO_AC 16 // TC5501P  pin 10 緑
#define SW_OMNI  9  //  tact sw for midi ch setting

// LEDs
const int led_omni = 4;  // midi omni on, LOW active 
const int led_midi = 5; // midi/clock indicator, LOW active
const int led_run  = 6; // LOW active

// midi status byte
const byte midi_start = 0xfa;
const byte midi_stop = 0xfc;
const byte midi_clock = 0xf8;
const byte midi_note = 0x90;
const byte midi_status = 0x80;
const byte midi_ch = 10;  // rhythm channel

// gate time (duration)

#define isStatus(data) (data >= 0x80)  // check status byte

int cntClock = 0;
int isRun = 0;
int reqRun = 0;  //  request to run
int tpqn = TPQN24;
int durHH = 10;

byte data;
int fData;
const int flag_st = 0;
const int flag_d1 = 1;
const int flag_d2 = 2;
bool isMidiOmni = false;
byte key;
byte vel;
byte cntQN; // count for quater note to lit led.

void handleStart();
void handleStop();
void handleNoteOn(byte key, byte vel);
void Sync();
void StartClockPulse();
void StartFirstClockPulse();
void EndClockPulse();

// inputs
Btn btnOmni;

// timers

#define DUR_BD 20
#define DUR_SD 20
#define DUR_RS 20
#define DUR_AC 30
#define DUR_HH_CLOSE 5
#define DUR_HH_PEDAL 35
#define DUR_HH_OPEN 150
#define WIDTH_CLOCK 20
#define WIDTH_LED 20

Metro tmLed = Metro(WIDTH_LED);    // midi/clock indicator
Metro tmClock = Metro(WIDTH_CLOCK);  // clock pulse width
Metro tmBD = Metro(DUR_BD);           // duration BD
Metro tmSD = Metro(DUR_SD);           // duration SD
Metro tmRS = Metro(DUR_RS);           // duration RS
Metro tmAC = Metro(DUR_AC);           // duration AC
Metro tmHH = Metro(DUR_HH_CLOSE);     // duration HH

// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  Serial.begin(31250);
  pinMode(led_midi, OUTPUT);
  pinMode(led_run, OUTPUT);
  pinMode(led_omni, OUTPUT);
  pinMode(CLOCK, INPUT);
  pinMode(ST, INPUT);
  pinMode(IO_BD, INPUT);
  pinMode(IO_SD, INPUT);
  pinMode(IO_RS, INPUT);
  pinMode(IO_HH, INPUT);
  pinMode(IO_AC, INPUT);
//  pinMode(SW_OMNI, INPUT_PULLUP);

  digitalWrite(IO_BD, LOW);
  digitalWrite(IO_SD, LOW);
  digitalWrite(IO_RS, LOW);
  digitalWrite(IO_HH, LOW);
  digitalWrite(IO_AC, LOW);

  digitalWrite(led_midi, HIGH);
  digitalWrite(led_run, HIGH);
  digitalWrite(led_omni, HIGH);

  fData = flag_st;  // status
  key = vel = 0;
  cntQN = 0;

  btnOmni.init(SW_OMNI, BTN_POLAR_PUSH_LOW);
}

// the loop routine runs over and over again forever:
void loop() {
  if (Serial.available() <= 0) goto TIMER_PROC;

  data = Serial.read();

  if (isStatus(data)) {
    if (data == midi_start) {
      handleStart();
    }
    else if(data == midi_stop) {
      handleStop();
    }
    else if(data == midi_clock) {
      Sync();
    }
    else if((data & 0xf0) == midi_note) {
      if (isMidiOmni ||  (data & 0x0f == midi_ch - 1)) {
        fData = flag_d1;      
      } else {
        fData = flag_st;  // waiting next status      
      }
    }
    else if((data & 0xf0) != 0xf0) {
      // other status other than realtime message
      fData = flag_st;  // waiting status
    }
  }
  else {
      // data byte
      if (fData == flag_d1) {
          key = data;
          fData = flag_d2;
      }
      else if (fData == flag_d2) {
          vel = data;
          fData = flag_d1;    // running status available
          if (vel > 0) handleNoteOn(key, vel);
      }
  }

TIMER_PROC:
//  if (isRun) {
    if (tmClock.check() == 1) {
      EndClockPulse();
    }
//  }
  if (tmBD.check() == 1) {
    digitalWrite(IO_BD, LOW);
    pinMode(IO_BD, INPUT);
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


  // read sw
  int res = btnOmni.readNCheck();
  if (res == BTN_ISCHANGE) {
    if (btnOmni.isDown()) {
      isMidiOmni = !isMidiOmni;
      digitalWrite(led_omni, !isMidiOmni);
    }
  }  

  // midi/clock led off
  if (tmLed.check() == 1) {
    digitalWrite(led_midi, HIGH);
  }
}

void Sync() {
  if (reqRun) {
    StartFirstClockPulse();
    cntClock = 0;
    return;
  }
  if (!isRun) return;
  if (++cntClock == tpqn) {
    StartClockPulse();
    cntClock = 0;
  }
}

void StartClockPulse() {
  // start clock pulse (negative)
  digitalWrite(CLOCK, LOW);
  tmClock.reset();
  if ((cntQN++ & 0x03) == 0) {
    digitalWrite(led_midi, LOW);
    tmLed.interval(WIDTH_LED);
    tmLed.reset();
  }
}

void StartFirstClockPulse()
{
  isRun = 1;
  reqRun = 0;
  pinMode(ST, OUTPUT);
  digitalWrite(ST, HIGH);
  pinMode(CLOCK, OUTPUT);
  StartClockPulse();
}

void EndClockPulse() {
  // end clock pulse
  digitalWrite(CLOCK, HIGH);
}

void handleStart(void)
{
  reqRun = 1;
  cntQN = 0;
  digitalWrite(led_run, LOW);
}

void handleStop(void)
{
  digitalWrite(ST, LOW);
  isRun = 0;
  pinMode(CLOCK, INPUT);
  pinMode(ST, INPUT);
  digitalWrite(led_run, HIGH);
}

void handleNoteOn(byte key, byte vel)
{
    int pin = 0;
    int dur = 0;
    switch (key) {
        case 48: // BD
        pin = IO_BD;
        dur = DUR_BD;
        tmBD.reset();
        break;
        case 51: // RS
        pin = IO_RS;
        dur = DUR_RS;
        tmRS.reset();
        break;
        case 50: // SD1
        case 52: // SD2
        pin = IO_SD;
        dur = DUR_SD;
        tmSD.reset();
        break;
        case 54: // C.HH
        pin = IO_HH;
        dur = DUR_HH_CLOSE;
        tmHH.interval(dur);
        tmHH.reset();
        break;
        case 56: // P.HH
        pin = IO_HH;
        dur = DUR_HH_PEDAL;
        tmHH.interval(dur);
        tmHH.reset();
        break;
        case 58: // O.HH
        pin = IO_HH;
        dur = DUR_HH_OPEN;
        tmHH.interval(dur);
        tmHH.reset();
        break;
        default:
        return;
    }

    if (vel > 90) { 
      pinMode(IO_AC, OUTPUT);
      digitalWrite(IO_AC, HIGH);
      tmAC.reset();
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    digitalWrite(led_midi, LOW);
    if (dur > 0) {
      tmLed.interval(dur);
      tmLed.reset();
    }
}
