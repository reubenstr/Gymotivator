#include <Arduino.h>
/*
  Gymotivator

  Reuben Strangelove
  Summer 2020

  Device plays motivational platitudes at interval and/or when the device detects the lights turning on.

  MCU: Arduino Mini (ATmega328P)
    Powered by 3.3v (XY-V17B 3.3v output supply)

  Sound module: XY-V17B
    Powered by 5.0v
    3.3v out by onboard LDO, 100ma
    Set up to use UART (9600 BAUD) for control
    3.3v TTL required (UART)

  Light sensor: 

  Notes:
    The first sound file (sorted by alphabetical order) is reserved for only when the device detects the amblient lights turning on.  

    Next version needs to have a light sensitivity adjustment for user setup.
*/

#define PIN_PHOTO_RESISTOR A1
#define PIN_POT_VOLUME A0
#define PIN_SWITCH_SENSOR 10
#define PIN_BUTTON_ACTIVE 3
#define PIN_SELECTOR_1 9
#define PIN_SELECTOR_2 8
#define PIN_SELECTOR_3 7
#define PIN_SELECTOR_4 6
#define PIN_SELECTOR_5 5
#define PIN_LED_BUILTIN 13

#define PIN_LED_ACTIVATE 4

const int lightLevelTrigger = 185;     // Note: found by trial and error. Light sensor value negatively correlates to light brightness.
const int numberOfSoundsAvailable = 9; // All sounds including reserved first sound.
const int lengthOfFirstSoundMs = 7000; // play length of the first sound in milliseconds, add another second

// https://github.com/JChristensen/JC_Button
#include <JC_Button.h>
Button buttonActivate(PIN_BUTTON_ACTIVE);

// Play the sound by number.
// The number of the sound is the sounds's place when the sounds in memory are sorted alphabetically.
void playSoundOnModule(byte s)
{
  Serial.write(0xAA);
  Serial.write(0x07);
  Serial.write(0x02);
  Serial.write(s >> 8);
  Serial.write(s & 0x00FF);
  Serial.write((0xAA + 0x07 + 0x02 + (s >> 8) + (s & 0x00FF)) & 0x00FF);
}

// set volume: 0 = min; 30 = max per XY-V17B specifications
void setVolume(unsigned int volume)
{
  if (volume > 30)
    volume = 30;
  Serial.write(0xAA);
  Serial.write(0x13);
  Serial.write(0x01);
  Serial.write(volume);
  Serial.write((0xAA + 0x13 + 0x01 + volume) & 0x00FF);
}

void stopPlayback()
{
  Serial.write(0xAA);
  Serial.write(0x04);
  Serial.write(0x00);
  Serial.write(0xAE);
}

void playSound()
{
  const int numberOfSoundsToCheck = 3;
  static int previousSounds[numberOfSoundsToCheck];

  while (1)
  {
    digitalWrite(PIN_LED_BUILTIN, HIGH);

    int randomSound = random(2, numberOfSoundsAvailable + 1);

    // check if random sound is in the list of previously played sounds.
    bool matchFoundFlag = false;
    for (int i = 0; i < numberOfSoundsToCheck; i++)
    {
      if (previousSounds[i] == randomSound)
      {
        matchFoundFlag = true;
      }
    }

    // if a match is not found, then store and play the sound.
    if (matchFoundFlag == false)
    {
      // rotate the sound buffer.
      for (int i = 0; i < numberOfSoundsToCheck - 1; i++)
      {
        previousSounds[i] = previousSounds[i + 1];
      }

      previousSounds[numberOfSoundsToCheck - 1] = randomSound;

      playSoundOnModule(randomSound);

      break;
    }
  }

  digitalWrite(PIN_LED_BUILTIN, LOW);
}

void setup()
{

  Serial.begin(9600);

  delay(1000); // Allow the XY-V17B sound module time to startup

  pinMode(PIN_PHOTO_RESISTOR, INPUT);
  pinMode(PIN_POT_VOLUME, INPUT);
  pinMode(PIN_SWITCH_SENSOR, INPUT);
  pinMode(PIN_SELECTOR_1, INPUT);
  pinMode(PIN_SELECTOR_2, INPUT);
  pinMode(PIN_SELECTOR_3, INPUT);
  pinMode(PIN_SELECTOR_4, INPUT);
  pinMode(PIN_SELECTOR_5, INPUT);
  pinMode(PIN_BUTTON_ACTIVE, INPUT);
  pinMode(PIN_LED_ACTIVATE, OUTPUT);
  pinMode(PIN_LED_BUILTIN, OUTPUT);

  digitalWrite(PIN_SELECTOR_1, HIGH);
  digitalWrite(PIN_SELECTOR_2, HIGH);
  digitalWrite(PIN_SELECTOR_3, HIGH);
  digitalWrite(PIN_SELECTOR_4, HIGH);
  digitalWrite(PIN_SELECTOR_5, HIGH);
  digitalWrite(PIN_SWITCH_SENSOR, HIGH);

  buttonActivate.begin();
}

void loop()
{
  static unsigned long delayMillis;
  static unsigned long soundDelay;
  static bool activeState = false;

  // Read activate button.
  buttonActivate.read();
  if (buttonActivate.wasPressed())
  {
    activeState = !activeState;
    if (activeState)
    {
      delayMillis = millis();
      playSound();
    }
  }

  // Turn on/off activate LED.
  digitalWrite(PIN_LED_ACTIVATE, activeState);

  // Set delay.
  if (digitalRead(PIN_SELECTOR_1) == 0)
    soundDelay = random(30000, 900000);
  else if (digitalRead(PIN_SELECTOR_2) == 0)
    soundDelay = 30000;
  else if (digitalRead(PIN_SELECTOR_3) == 0)
    soundDelay = 60000;
  else if (digitalRead(PIN_SELECTOR_4) == 0)
    soundDelay = 300000;
  else if (digitalRead(PIN_SELECTOR_5) == 0)
    soundDelay = 900000;
  else
    soundDelay = 500;

  // Play sound.
  if (activeState)
  {
    if ((delayMillis + soundDelay) < millis())
    {
      delayMillis = millis();
      playSound();
    }
  }

  // Set volume.
  static int oldVolume;
  int volumeReading = 1023 - analogRead(PIN_POT_VOLUME);
  int volume = map(volumeReading, 0, 1023, 10, 30);
  if (volume != oldVolume)
  {
    oldVolume = volume;
    setVolume(volume);
  }

  // Detect light changes.
  // Light sensor value negatively correlated to light brightness.
  static int previousLightReading;
  if (digitalRead(PIN_SWITCH_SENSOR) == 0)
  {
    int lightReading = 1023 - analogRead(PIN_PHOTO_RESISTOR);

    // Light reading mapped for easy debugging via UART.
    lightReading = map(lightReading, 0, 1023, 0, 255);

    if (previousLightReading >= lightLevelTrigger && lightReading < lightLevelTrigger)
    {
      if (!activeState)
      {
        playSoundOnModule(1);
        delayMillis = millis();
        activeState = true;
      }
    }

    // Add delay before turning off the device when the lights dim.
    static unsigned long delayedReactionMillis;
    if (lightReading < lightLevelTrigger)
    {
      delayedReactionMillis = millis();
    }
    else
    {
      if ((delayedReactionMillis + 3000) < millis())
      {
        activeState = false;      
      }
    }

    // Calibration data.
    // Serial.write(lightReading);
    // delay(1000);

    previousLightReading = lightReading;
  }
}