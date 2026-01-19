#include <Arduino.h>

// --- CONFIGURATION ---
#define PULSE_WIDTH 1000   // 1ms
#define SERIAL_BAUD 115200 // Set your Serial Monitor to this
#define DEBUG true         // Set to true to enable serial logging

// Pins
const int bncIn = A0;
const int bncOut = A1;
const int btnUp = A2;
const int btnDown = A3;

// Display Pins
const int segA = 7;
const int segB = 6;
const int segC = 4;
const int segD = 3;
const int segE = 2;
const int segF = 8;
const int segG = 9;

int upsamplingFactor = 2;

const byte numbers[10] = {
    0b0000001, 0b1001111, 0b0010010, 0b0000110, 0b1001100,
    0b0100100, 0b0100000, 0b0001111, 0b0000000, 0b0000100};

// Timing
volatile unsigned long isrTime = 0;
volatile bool newSignal = false;
unsigned long lastPulseTime = 0;

// Buttons
volatile unsigned long btnUpLastPress = 0;
volatile unsigned long btnDownLastPress = 0;
const unsigned long BTN_DEBOUNCE = 200000;

void displayNumber(int num)
{
  if (num < 0 || num > 9)
    return;
  byte segments = numbers[num];
  digitalWrite(segA, bitRead(segments, 6));
  digitalWrite(segB, bitRead(segments, 5));
  digitalWrite(segC, bitRead(segments, 4));
  digitalWrite(segD, bitRead(segments, 3));
  digitalWrite(segE, bitRead(segments, 2));
  digitalWrite(segF, bitRead(segments, 1));
  digitalWrite(segG, bitRead(segments, 0));
}

void handleInterrupt()
{
  isrTime = micros();
  newSignal = true;
}

void handleBtnUp()
{
  if (micros() - btnUpLastPress > BTN_DEBOUNCE)
  {
    btnUpLastPress = micros();
    if (upsamplingFactor < 9)
    {
      upsamplingFactor++;
      displayNumber(upsamplingFactor);
    }
  }
}

void handleBtnDown()
{
  if (micros() - btnDownLastPress > BTN_DEBOUNCE)
  {
    btnDownLastPress = micros();
    if (upsamplingFactor > 0)
    {
      upsamplingFactor--;
      displayNumber(upsamplingFactor);
    }
  }
}

// --- THE FIX ---
// Since micros() is accurate (proven by logs), we use it for delays.
// This bypasses the F_CPU mismatch issue entirely.
void smartDelay(unsigned long us)
{
  unsigned long start = micros();
  while (micros() - start < us)
  {
    // Busy wait using the hardware timer
    asm(""); // Prevent compiler from optimizing loop away
  }
}

void setup()
{
  if (DEBUG)
    Serial.begin(SERIAL_BAUD);

  pinMode(segA, OUTPUT);
  pinMode(segB, OUTPUT);
  pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT);
  pinMode(segE, OUTPUT);
  pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT);

  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(btnUp), handleBtnUp, FALLING);
  attachInterrupt(digitalPinToInterrupt(btnDown), handleBtnDown, FALLING);

  pinMode(bncIn, INPUT);
  pinMode(bncOut, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(bncIn), handleInterrupt, RISING);

  displayNumber(upsamplingFactor);
  if (DEBUG)
    Serial.println("System Ready.");
}

void loop()
{
  if (newSignal)
  {
    noInterrupts();
    unsigned long arrival = isrTime;
    newSignal = false;
    interrupts();

    unsigned long interval = arrival - lastPulseTime;

    // Filter noise (< 25ms)
    if (interval < 25000)
      return;

    lastPulseTime = arrival;
    // Cap interval to 100ms in case it's disconnected for a while
    if (interval > 100000)
      interval = 100000;

    // Calculate Delay
    unsigned long upsampledDelay = (interval / upsamplingFactor) - PULSE_WIDTH;

    // OUTPUT SEQUENCE
    for (int i = 0; i < upsamplingFactor; i++)
    {
      digitalWrite(bncOut, HIGH);
      smartDelay(PULSE_WIDTH);
      digitalWrite(bncOut, LOW);

      // Wait for next pulse (but skip (don't do waiting) after the last pulse)
      if (i < upsamplingFactor - 1)
      {

        // Capture time BEFORE doing work (printing)
        unsigned long waitStart = micros();

        // Now we can print. The time this takes will be "absorbed"
        // by the while loop below.
        if (i == 0 && DEBUG)
        {
          Serial.print("Int:");
          Serial.print(interval);
          Serial.print(" Dly:");
          Serial.println(upsampledDelay);
        }

        // Now wait for the REMAINDER of the time.
        // Since 'waitStart' was set before printing, this loop
        // automatically subtracts the print duration.
        while (micros() - waitStart < upsampledDelay)
        {
          asm("");
        }
      }
    }
  }
}