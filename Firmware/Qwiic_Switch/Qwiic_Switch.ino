/*
  An I2C based Button
  By: Nathan Seidle, Fischer Moseley
  SparkFun Electronics
  Date: June 5th, 2019
  License: This code is public domain but you buy me a beer if you use this and
  we meet someday (Beerware license).

  Qwiic Button is an I2C based button that records any button presses to a queue.

  Qwiic Button maintains a queue of events. To remove events from the queue write
  the appropriate register (timeSinceLastButtonClicked or timeSinceLastButtonPressed)
  to zero. The register will then be filled with the next available event time.

  There is also an accompanying Arduino Library located here:
  https://github.com/sparkfun/SparkFun_Qwiic_Switch_Arduino_Library

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14641

  To install support for ATtiny84 in Arduino IDE: https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md
  This core is installed from the Board Manager menu
  This core has built in support for I2C S/M and serial
  If you have Dave Mellis' ATtiny installed you may need to remove it from \Users\xx\AppData\Local\Arduino15\Packages

  To support 400kHz I2C communication reliably ATtiny84 needs to run at 8MHz. This requires user to
  click on 'Burn Bootloader' before code is loaded.
*/

#include <Wire.h>
#include <EEPROM.h>
#include "nvm.h"
#include "queue.h"
#include "registers.h"

#include "PinChangeInterrupt.h" //Nico Hood's library: https://github.com/NicoHood/PinChangeInterrupt/
//Used for pin change interrupts on ATtinys (encoder button causes interrupt)
//Note: To make this code work with Nico's library you have to comment out https://github.com/NicoHood/PinChangeInterrupt/blob/master/src/PinChangeInterruptSettings.h#L228

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Hardware connections
#if defined(__AVR_ATmega328P__)
//For developement on an Uno
const byte addressPin = 6;
const byte ledPin = 9; //PWM
const byte statusLedPin = 7;
const byte switchPin = 2;
const byte interruptPin = 7; //Pin goes low when an event occurs
#elif defined(__AVR_ATtiny84__)
const byte addressPin = 9;
const byte ledPin = 7; //PWM
const byte statusLedPin = 3;
const byte switchPin = 8;
const byte interruptPin = 0; //Pin goes low when an event occurs
#endif

//Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//These are the defaults for all settings
//Variables used in the I2C interrupt.ino file so we use volatile
volatile memoryMap registerMap = {
  .id = 0x5d,
  .status = 0x00, //1 - button clicked, 0 - button pressed
  .firmwareMinor = 0x00, //Firmware version. Helpful for tech support.
  .firmwareMajor = 0x01,
  .interruptEnable = 0x03, //0 - button pressed interrupt, button clicked interrupt
  .timeSinceLastButtonPressed = 0x0000,
  .timeSinceLastButtonClicked = 0x0000,
  .ledBrightness = 0xFF, //Max brightness
  .ledPulseGranularity = 0x01, //1 is best for most pulse scenarios
  .ledPulseCycleTime = 0x00,
  .ledPulseOffTime = 0x00,
  .buttonDebounceTime = 10, //In ms
  .i2cAddress = I2C_ADDRESS_DEFAULT,
};

//This defines which of the registers are read-only (0) vs read-write (1)
memoryMap protectionMap = {
  .id = 0x00,
  .status = (1 << statusButtonClickedBit) | (1 << statusButtonPressedBit), //ask nate why these are read/write!
  .firmwareMinor = 0x00,
  .firmwareMajor = 0x00,
  .interruptEnable = (1 << enableInterruptButtonClickedBit) | (1 << enableInterruptButtonPressedBit),
  .timeSinceLastButtonPressed = 0xFFFF,
  .timeSinceLastButtonClicked = 0xFFFF,
  .ledBrightness = 0xFF,
  .ledPulseGranularity = 0xFF,
  .ledPulseCycleTime = 0xFFFF,
  .ledPulseOffTime = 0xFFFF,
  .buttonDebounceTime = 0xFFFF, //In ms
  .i2cAddress = 0xFF,
};

//Cast 32bit address of the object registerMap with uint8_t so we can increment the pointer
uint8_t *registerPointer = (uint8_t *)&registerMap;
uint8_t *protectionPointer = (uint8_t *)&protectionMap;

volatile byte registerNumber; //Gets set when user writes an address. We then serve the spot the user requested.

volatile boolean updateOutputs = false; //Goes true when we receive new bytes from user. Causes LEDs and things to update in main loop.

//Interrupt turns on when button is pressed,
//Turns off when interrupts are cleared by command
enum State {
  STATE_BUTTON_INT = 0,
  STATE_INT_CLEARED,
  STATE_INT_INDICATED,
};
volatile byte interruptState = STATE_INT_CLEARED;

volatile uint8_t interruptCount = 0; //Debug
byte oldCount = 0;

//FIFO-style circular ring buffer for storing button timestamps. 
//The memory map is loaded with new time when user writes the timeSinceLastButton to zero.
Queue ButtonPressed, ButtonClicked;

//Used for LED pulsing
unsigned long ledAdjustmentStartTime; //Start time of micro adjustment
unsigned long ledPulseStartTime; //Start time of overall LED pulse
uint16_t pulseLedAdjustments; //Number of adjustments to LED to make: registerMap.ledBrightness / registerMap.ledPulseGranularity * 2
unsigned long timePerAdjustment; //ms per adjustment step of LED: registerMap.ledPulseCycleTime / pulseLedAdjustments
int16_t pulseLedBrightness = 0; //Analog brightness of LED
int16_t ledBrightnessStep; //Granularity but will become negative as we pass max brightness

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup(void)
{
  //configure I/O
  pinMode(addressPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT); //PWM
  analogWrite(ledPin, 0); //Off
  pinMode(statusLedPin, OUTPUT); //No PWM
  pinMode(switchPin, INPUT_PULLUP); //GPIO with internal pullup
  pinMode(interruptPin, INPUT); //High-impedance input until we have an int and then we output low. Pulled high with 10k with cuttable jumper.

#if defined(__AVR_ATmega328P__)
  Serial.begin(115200);
  Serial.println("Qwiic Button");
  Serial.print("Address: 0x");
  Serial.print(registerMap.i2cAddress, HEX);
  Serial.println();
#endif

  //Disable ADC
  ADCSRA = 0;

  //Disable Brown-Out Detect
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS);

  //Power down various bits of hardware to lower power usage
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN); //May turn off millis
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  readSystemSettings(); //Load all system settings from EEPROM

#if defined(__AVR_ATmega328P__)
  //Debug values
  registerMap.ledBrightness = 255; //Max brightness
  registerMap.ledPulseGranularity = 1; //Amount to change LED at each step

  registerMap.ledPulseCycleTime = 500; //Total amount of cycle, does not include off time. LED pulse disabled if zero.
  registerMap.ledPulseOffTime = 500; //Off time between pulses
  //End debug values
#endif

  calculatePulseValues();
  resetPulseValues();

  setupInterrupts(); //Enable pin change interrupts for I2C, switch, etc

  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
}

void loop(void)
{
  //Set interrupt pin as needed
  //Interrupt pin state machine
  //There are three states: Button Int, Int Cleared, Int Indicated
  //BUTTON_INT state is set if user presses button
  //INT_CLEARED state is set in the I2C interrupt when Clear Ints command is received.
  //INT_INDICATED state is set once we change the INT pin to go low

  //If we are in button interrupt state, then set INT low
  if (interruptState == STATE_BUTTON_INT)
  {
    //Set the interrupt pin low to indicate interrupt
    pinMode(interruptPin, OUTPUT);
    digitalWrite(interruptPin, LOW);
    interruptState = STATE_INT_INDICATED;
  }

  if (updateOutputs == true)
  {
    //Record anything new to EEPROM like new LED values
    //It can take ~3.4ms to write EEPROM byte so we do that here instead of in interrupt
    recordSystemSettings();

    //Calculate LED values based on pulse settings
    calculatePulseValues();

    resetPulseValues();

    updateOutputs = false;
  }

  sleep_mode(); //Stop everything and go to sleep. Wake up if I2C event occurs.

  if (interruptCount != oldCount)
  {
    oldCount = interruptCount;
#if defined(__AVR_ATmega328P__)
  //displayBuffer();
#endif
  }

  //Pulse LED on/off based on settings
  //Brightness will increase with each cycle based on granularity value (5)
  //To get to max brightness (207) we will need ceil(207/5*2) LED adjustments = 83
  //At each discrete adjustment the LED will spend X amount of time at a brightness level
  //Time spent at this level is calc'd by taking total time (1000 ms) / number of adjustments / up/down (2) = 12ms per step

  if (registerMap.ledPulseCycleTime == 0){ //Just set the LED to a static value if cycle time is zero
    analogWrite(ledPin, registerMap.ledBrightness);
  }

  if (registerMap.ledPulseCycleTime > 0) //Otherwise run in cyclic mode
  {
    if (millis() - ledPulseStartTime <= registerMap.ledPulseCycleTime)
    {
      //Pulse LED
      //Change LED brightness if enough time has passed
      if (millis() - ledAdjustmentStartTime >= timePerAdjustment)
      {
        pulseLedBrightness += ledBrightnessStep;

        //If we've reached a max brightness, reverse step direction
        if (pulseLedBrightness > (registerMap.ledBrightness - registerMap.ledPulseGranularity))
        {
          ledBrightnessStep *= -1;
        }
        if (pulseLedBrightness < 0) pulseLedBrightness = 0;
        if (pulseLedBrightness > 255) pulseLedBrightness = 255;

        analogWrite(ledPin, pulseLedBrightness);
        ledAdjustmentStartTime = millis();
      }
    }
    else if (millis() - ledPulseStartTime <= registerMap.ledPulseCycleTime + registerMap.ledPulseOffTime)
    {
      //LED off
      analogWrite(ledPin, 0);
    }
    else
    {
      //Pulse cycle and off time have expired. Reset pulse settings.
      resetPulseValues();
    }
  } //LED pulse enable
}

//Calculate LED values based on pulse settings
void calculatePulseValues(){
  pulseLedAdjustments = ceil((float)registerMap.ledBrightness / registerMap.ledPulseGranularity * 2.0);
  timePerAdjustment = round((float)registerMap.ledPulseCycleTime / pulseLedAdjustments);
  ledBrightnessStep = registerMap.ledPulseGranularity;
}

//At the completion of a pulse cycle, reset timers
void resetPulseValues()
{
  ledPulseStartTime = millis();
  ledAdjustmentStartTime = millis();
  ledBrightnessStep = registerMap.ledPulseGranularity; //Return positive
  pulseLedBrightness = 0;
}

//Begin listening on I2C bus as I2C slave using the global variable registerMap.i2cAddress
void startI2C()
{
  Wire.end(); //Before we can change addresses we need to stop

  if (digitalRead(addressPin) == HIGH) //Default is HIGH, the jumper is open
    Wire.begin(registerMap.i2cAddress); //Start I2C and answer calls using address from EEPROM
  else
    Wire.begin(I2C_FORCED_ADDRESS); //Force address to I2C_ADDRESS_JUMPER if user has closed the solder jumper

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

//Reads the current system settings from EEPROM
//If anything looks weird, reset setting to default value
void readSystemSettings(void)
{
  //Read what I2C address we should use
  EEPROM.get(LOCATION_I2C_ADDRESS, registerMap.i2cAddress);
  if (registerMap.i2cAddress == 255)
  {
    registerMap.i2cAddress = I2C_ADDRESS_DEFAULT; //By default, we listen for I2C_ADDRESS_DEFAULT
    EEPROM.update(LOCATION_I2C_ADDRESS, registerMap.i2cAddress);
  }

  //Error check I2C address we read from EEPROM
  if (registerMap.i2cAddress < 0x08 || registerMap.i2cAddress > 0x77)
  {
    //User has set the address out of range
    //Go back to defaults
    registerMap.i2cAddress = I2C_ADDRESS_DEFAULT;
    EEPROM.update(LOCATION_I2C_ADDRESS, registerMap.i2cAddress);
  }

  //Read the interrupt bits
  EEPROM.get(LOCATION_INTERRUPTS, registerMap.interruptEnable);
  if (registerMap.interruptEnable == 0xFF) //Blank
  {
    registerMap.interruptEnable = 0x03; //By default, enable the click and pressed interrupts
    EEPROM.update(LOCATION_INTERRUPTS, registerMap.interruptEnable);
  }

  EEPROM.get(LOCATION_LED_PULSEGRANULARITY, registerMap.ledPulseGranularity);
  if (registerMap.ledPulseGranularity == 0xFF)
  {
    registerMap.ledPulseGranularity = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSEGRANULARITY, registerMap.ledPulseGranularity);
  }

  EEPROM.get(LOCATION_LED_PULSECYCLETIME, registerMap.ledPulseCycleTime);
  if (registerMap.ledPulseCycleTime == 0xFFFF)
  {
    registerMap.ledPulseCycleTime = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSECYCLETIME, registerMap.ledPulseCycleTime);
  }

  EEPROM.get(LOCATION_LED_PULSEOFFTIME, registerMap.ledPulseOffTime);
  if (registerMap.ledPulseOffTime == 0xFFFF)
  {
    registerMap.ledPulseOffTime = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSECYCLETIME, registerMap.ledPulseOffTime);
  }

  EEPROM.get(LOCATION_BUTTON_DEBOUNCE_TIME, registerMap.buttonDebounceTime);
  if (registerMap.buttonDebounceTime == 0xFFFF)
  {
    registerMap.buttonDebounceTime = 10; //Default to 10ms
    EEPROM.update(LOCATION_BUTTON_DEBOUNCE_TIME, registerMap.buttonDebounceTime);
  }

  //Read the starting value for the LED
  EEPROM.get(LOCATION_LED_BRIGHTNESS, registerMap.ledBrightness);
  if (registerMap.ledPulseCycleTime > 0)
  {
    //Don't turn on LED, we'll pulse it in main loop
    analogWrite(ledPin, 0);
  }
  else //Pulsing disabled
  {
    //Turn on LED to setting
    analogWrite(ledPin, registerMap.ledBrightness);
  }

}

//If the current setting is different from that in EEPROM, update EEPROM
void recordSystemSettings(void)
{
  //Error check the current I2C address
  if (registerMap.i2cAddress < 0x08 || registerMap.i2cAddress > 0x77)
  {
    //User has set the address out of range
    //Go back to defaults
    registerMap.i2cAddress = I2C_ADDRESS_DEFAULT;
    startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
  }

  EEPROM.update(LOCATION_I2C_ADDRESS, registerMap.i2cAddress);
  EEPROM.update(LOCATION_INTERRUPTS, registerMap.interruptEnable);
  EEPROM.update(LOCATION_LED_BRIGHTNESS, registerMap.ledBrightness);
  EEPROM.update(LOCATION_LED_PULSEGRANULARITY, registerMap.ledPulseGranularity);
  EEPROM.update(LOCATION_LED_PULSECYCLETIME, registerMap.ledPulseCycleTime);
  EEPROM.update(LOCATION_LED_PULSEOFFTIME, registerMap.ledPulseOffTime);
  EEPROM.update(LOCATION_BUTTON_DEBOUNCE_TIME, registerMap.buttonDebounceTime);
}