#include <Arduino.h>

// --- CONFIGURATION ---
#define PULSE_WIDTH 100      // in microseconds
#define SERIAL_BAUD 115200   // Set your Serial Monitor to this
#define DEBUG true           // Set to true to enable serial logging
#define FILTER_DURATION 1000 // in microseconds, filter out pulses shorter than this

// Pins
const int bncIn = A0;
const int bncOut = A1;
const int btnUp = A3;
const int btnDown = A2;

// Flipped Display Pins (180-degree rotation)
const int segA = 3; // Was segD
const int segB = 2; // Was segE
const int segC = 8; // Was segF
const int segD = 7; // Was segA
const int segE = 6; // Was segB
const int segF = 4; // Was segC
const int segG = 9; // Stays the same (Middle)

int upsamplingFactor = 2;

const byte numbers[10] = {
    0b0000001, 0b1001111, 0b0010010, 0b0000110, 0b1001100,
    0b0100100, 0b0100000, 0b0001111, 0b0000000, 0b0000100};

// Segment Map: 0 = ON, 1 = OFF
// Order: . A B C D E F G (Bit 6 = A ... Bit 0 = G)

const byte letters[26] = {
    0b0001000, // A (Upper)
    0b1100000, // b (Lower)
    0b0110001, // C (Upper)
    0b1000010, // d (Lower)
    0b0110000, // E (Upper)
    0b0111000, // F (Upper)
    0b0100001, // G (Upper - no tail)
    0b1101000, // h (Lower)
    0b1111001, // I (Number 1 / Right side)
    0b1110011, // J (Right side + bottom)
    0b1001000, // K (Displayed as H/h usually, or specific symbol)
    0b1110001, // L (Upper)
    0b1010101, // m (Two small humps - rare approx) OR use 0b1101010 (n)
    0b1101010, // n (Lower)
    0b1100010, // o (Lower)
    0b0011000, // P (Upper)
    0b0001100, // q (Upper with tail)
    0b1111010, // r (Lower)
    0b0100100, // S (Same as 5)
    0b1110000, // t (Lower)
    0b1000001, // U (Upper)
    0b1100011, // v (Looks like u or lower o) - usually u
    0b1010101, // W (Impossible - usually blank or approx)
    0b1001000, // X (Impossible - usually H)
    0b1000100, // y (Lower)
    0b0010010  // Z (Same as 2)
};

// Timing
volatile unsigned long isrTime = 0;
volatile bool newSignal = false;
unsigned long lastPulseTime = 0;

// Buttons
volatile unsigned long btnUpLastPress = 0;
volatile unsigned long btnDownLastPress = 0;
const unsigned long BTN_DEBOUNCE = 200000;

void displayByteSegment(byte segments)
{

  digitalWrite(segA, bitRead(segments, 6));
  digitalWrite(segB, bitRead(segments, 5));
  digitalWrite(segC, bitRead(segments, 4));
  digitalWrite(segD, bitRead(segments, 3));
  digitalWrite(segE, bitRead(segments, 2));
  digitalWrite(segF, bitRead(segments, 1));
  digitalWrite(segG, bitRead(segments, 0));
}

void displayNumber(int num)
{
  if (num < 0 || num > 9)
    return;

  // if (num == 0)
  // {
  //   if (DEBUG)
  //     Serial.println("Displaying OFF with startup sequence.");
  //   // Display OFF with 500ms delays
  //   displayByteSegment(numbers[0]);
  //   delay(1);
  //   if (DEBUG)
  //     Serial.println("Displaying OFF with startup sequence.");
  //   // Display F
  //   displayByteSegment(letters[5]);
  //   delay(1);
  //   if (DEBUG)
  //     Serial.println("Displaying OFF with startup sequence.");
  //   byte empty = 0b1111111;
  //   displayByteSegment(empty);
  //   delay(1);
  //   if (DEBUG)
  //     Serial.println("Displaying OFF with startup sequence.");
  //   displayByteSegment(letters[5]);
  //   delay(1);
  //   if (DEBUG)
  //     Serial.println("Finished displaying OFF with startup sequence.");
  // }

  displayByteSegment(numbers[num]);
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
    Serial.println("System Ready. Made by Olgierd Matusiewicz.");
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

    // Filter noise (< 1ms)
    if (interval < FILTER_DURATION)
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