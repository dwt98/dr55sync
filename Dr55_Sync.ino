#include <Btn.h>

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
const int led_clock = 4; // LOW active
const int led_note  = 5; // LOW active
const int led_run  = 6; // LOW active
const int led_omni = 7;  // midi omni on, LOW active 

// midi status byte
const byte midi_start = 0xfa;
const byte midi_stop = 0xfc;
const byte midi_clock = 0xf8;
const byte midi_note = 0x90;
const byte midi_status = 0x80;
const byte midi_ch = 10;  // rhythm channel

// gate time (duration)
const int durHHClose = 5;
const int durHHPedal = 35;
const int durHHOpen = 150;
const int durBD = 20;
const int durSD = 20;
const int durRS = 20;
const int durAC = 20;

#define isStatus(data) (data >= 0x80)  // check status byte

int cntClock = 0;
int isRun = 0;
int reqRun = 0;  //  request to run
int tpqn = TPQN24;
int durHH = 10;
unsigned long tm = 0;
unsigned long tm_bd = 0;
unsigned long tm_rs = 0;
unsigned long tm_sd = 0;
unsigned long tm_hh = 0;
unsigned long tm_ac = 0;
unsigned long tm_led_nt = 0;  // for led of note
unsigned long tm_led_cl = 0;  // for led of clock

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

Btn btnOmni;

// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  Serial.begin(31250);
  pinMode(led_clock, OUTPUT);
  pinMode(led_note, OUTPUT);
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

  digitalWrite(led_clock, HIGH);
  digitalWrite(led_note, HIGH);
  digitalWrite(led_run, HIGH);
  digitalWrite(led_omni, HIGH);
  tm = millis();
  tm_bd = tm_sd = tm_rs = tm_hh = tm_ac = 0;
  tm_led_nt = tm_led_cl = 0;

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
  if (isRun) {
    if ((tm + 20) < millis()) {
      EndClockPulse();
    }
  }
  if ((tm_bd + durBD) < millis()) {
    digitalWrite(IO_BD, LOW);
    pinMode(IO_BD, INPUT);
  }
  if ((tm_rs + durRS) < millis()) {
    digitalWrite(IO_RS, LOW);
    pinMode(IO_RS, INPUT);
  }
  if ((tm_sd + durSD) < millis()) {
    digitalWrite(IO_SD, LOW);
    pinMode(IO_SD, INPUT);
  }
  if ((tm_hh + durHH) < millis()) {
    digitalWrite(IO_HH, LOW);
    pinMode(IO_HH, INPUT);
  }
  if ((tm_ac + durAC) < millis()) {
    digitalWrite(IO_AC, LOW);
    pinMode(IO_AC, INPUT);
  }
  if ((tm_led_nt + 10) < millis()){
    digitalWrite(led_note, HIGH);
  }

  // read sw
  int res = btnOmni.readNCheck();
  if (res == BTN_ISCHANGE) {
    if (btnOmni.isDown()) {
      isMidiOmni = !isMidiOmni;
     digitalWrite(led_omni, !isMidiOmni);
    }
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
  tm = millis();
  if ((cntQN++ & 0x03) == 0) {
    digitalWrite(led_clock, LOW);
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
  digitalWrite(led_clock, HIGH);
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
    switch (key) {
        case 48: // BD
        pin = IO_BD;
        tm_bd = tm_led_nt = millis();
        break;
        case 51: // RS
        pin = IO_RS;
        tm_rs = tm_led_nt = millis();
        break;
        case 50: // SD1
        case 52: // SD2
        pin = IO_SD;
        tm_sd = tm_led_nt = millis();
        break;
        case 54: // C.HH
        pin = IO_HH;
        durHH = durHHClose;
        tm_hh = tm_led_nt = millis();
        break;
        case 56: // P.HH
        pin = IO_HH;
        durHH = durHHPedal;
        tm_hh = tm_led_nt = millis();
        break;
        case 58: // O.HH
        pin = IO_HH;
        durHH = durHHOpen;
        tm_hh = tm_led_nt = millis();
        break;
        default:
        return;
    }

    if (vel > 90) { 
      pinMode(IO_AC, OUTPUT);
      digitalWrite(IO_AC, HIGH);
      tm_ac = tm_led_nt;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    digitalWrite(led_note, LOW);
    
}
