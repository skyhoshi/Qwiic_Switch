/*
  An I2C based Button
  By: Nathan Seidle and Fischer Moseley
  SparkFun Electronics
  Date: July 31st, 2019
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

  Library Inclusion:
  Wire.h        Used for interfacing with the I2C hardware for responding to I2C events.
  EEPROM.h      Used for interfacing with the onboard EEPROM for storing and retrieving settings.
  nvm.h         Used for defining the storage locations in non-volatile memory (EEPROM) to store and retrieve settings from.
  queue.h       Used for defining a FIFO-queue that contains the timestamps associated with pressing and clicking the button.
  registers.h   Used for defining a memoryMap object that serves as the register map for the device. 
  led.h         Used for configuring the behavior of the onboard LED (in the case of the Qwiic Button) 
                  or the offboard LED (in the case of the Qwiic Switch)

  PinChangeInterrupt.h    Nico Hoo's library for triggering an interrupt on a pin state change (either low->high or high->low)
  avr/sleep.h             Needed for sleep_mode which saves power on the ATTiny
  avr/power.hardware      Needed for powering down peripherals such as the ADC/TWI and Timers on the ATTiny
*/

#include <Wire.h>
#include <EEPROM.h>
#include "nvm.h"
#include "queue.h"
#include "registers.h"
#include "led.h"

#include "PinChangeInterrupt.h" //Nico Hood's library: https://github.com/NicoHood/PinChangeInterrupt/
//Used for pin change interrupts on ATtinys (encoder button causes interrupt)
//Note: To make this code work with Nico's library you have to comment out https://github.com/NicoHood/PinChangeInterrupt/blob/master/src/PinChangeInterruptSettings.h#L228

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Software Self-Identification configuration
//Please update these appropriately before uploading!
#define __SWITCH__ //The device that this code will run on. Set to __BUTTON__ for the Qwiic Button, and __SWITCH__ for the Qwiic Switch.

#define SWITCH_DEVICE_ID 0x5E
#define BUTTON_DEVICE_ID 0x5D
#define FIRMWARE_MAJOR 0x00 //Firmware Version. Helpful for tech support.
#define FIRMWARE_MINOR 0x01

#if defined __SWITCH__
#define DEVICE_ID SWITCH_DEVICE_ID
#define DEFAULT_I2C_ADDRESS 0x46
#define FORCED_I2C_ADDRESS 0x47
#endif

#if defined __BUTTON__
#define DEVICE_ID BUTTON_DEVICE_ID
#define DEFAULT_I2C_ADDRESS 0x60
#define FORCED_I2C_ADDRESS 0x60 //This is can whatever we want as this parameter is only used by the switch
#endif

//Hardware connections
#if defined(__AVR_ATmega328P__)
//For developement on an Uno
const uint8_t addressPin0 = 3;
const uint8_t addressPin1 = 4;
const uint8_t addressPin2 = 5;
const uint8_t addressPin3 = 6;
const uint8_t ledPin = 9; //PWM
const uint8_t statusLedPin = 8;
const uint8_t switchPin = 2;
const uint8_t interruptPin = 7; //pin is active-low, high-impedance when not triggered, goes low-impedance to ground when triggered

#elif defined(__AVR_ATtiny84__)
const uint8_t addressPin0 = 9;
const uint8_t addressPin1 = 10;
const uint8_t addressPin2 = 1;
const uint8_t addressPin3 = 2;
const uint8_t ledPin = 7; //PWM
const uint8_t statusLedPin = 3;
const uint8_t switchPin = 8;
const uint8_t interruptPin = 0; //pin is active-low, high-impedance when not triggered, goes low-impedance to ground when triggered
#endif

//Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//These are the defaults for all settings

//Variables used in the I2C interrupt.ino file so we use volatile
volatile memoryMap registerMap {
  {0,0,0},        //buttonStatus {isReady, isPressed, hasBeenClicked}
  0x000A,         //buttonDebounceTime
  {0,0,0},        //interruptConfig {pressedEnable, clickedEnable, status}
  {0,1,0},        //pressedQueueStatus {isFull, isEmpty, popRequest}
  0x00000000,     //pressedQueueFront
  0x00000000,     //pressedQueueBack
  {0,1,0},        //clickedQueueStatus {isFull, isEmpty, popRequest}
  0x00000000,     //clickedQueueFront
  0x00000000,     //clickedQueueBack
  0x00,           //ledBrightness
  0x01,           //ledPulseGranularity
  0x0000,         //ledPulseCycleTime
  0x0000,         //ledPulseOffTime
  DEFAULT_I2C_ADDRESS,  //i2cAddress
  DEVICE_ID,            //id
  FIRMWARE_MINOR,       //firmwareMinor
  FIRMWARE_MAJOR,       //firmwareMajor
};


//This defines which of the registers are read-only (0) vs read-write (1)
memoryMap protectionMap = {
  {0,0,1},        //buttonStatus {isReady, isPressed, hasBeenClicked}
  0xFFFF,         //buttonDebounceTime
  {1,1,1},        //interruptConfig {pressedEnable, clickedEnable, status}
  {0,0,1},        //pressedQueueStatus {isFull, isEmpty, popRequest}
  0x00000000,     //pressedQueueFront
  0x00000000,     //pressedQueueBack
  {0,0,1},        //clickedQueueStatus {isFull, isEmpty, popRequest}
  0x00000000,     //clickedQueueFront
  0x00000000,     //clickedQueueBack
  0xFF,           //ledBrightness
  0xFF,           //ledPulseGranularity
  0xFFFF,         //ledPulseCycleTime
  0xFFFF,         //ledPulseOffTime
  0xFF,           //i2cAddress
  0x00,           //id
  0x00,           //firmwareMinor
  0x00,           //firmwareMajor
};

//Cast 32bit address of the object registerMap with uint8_t so we can increment the pointer
uint8_t *registerPointer = (uint8_t *)&registerMap;
uint8_t *protectionPointer = (uint8_t *)&protectionMap;

volatile uint8_t registerNumber; //Gets set when user writes an address. We then serve the spot the user requested.

volatile boolean updateFlag = true; //Goes true when we receive new bytes from user. Causes LEDs and things to update in main loop.

Queue ButtonPressed, ButtonClicked; //init FIFO buffer for storing timestamps associated with button presses and clicks

LEDconfig onboardLED; //init the onboard LED

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup(void) {
  //configure I/O
  pinMode(addressPin0, INPUT_PULLUP); //internally pull up address pins
  pinMode(addressPin1, INPUT_PULLUP); //when the user solders a jumper the pin will read as LOW, otherwise it will read as HIGH
  pinMode(addressPin2, INPUT_PULLUP);
  pinMode(addressPin3, INPUT_PULLUP);

  pinMode(ledPin, OUTPUT); //PWM
  analogWrite(ledPin, 0);

  pinMode(statusLedPin, OUTPUT); //No PWM
  digitalWrite(statusLedPin, 0);

  pinMode(switchPin, INPUT_PULLUP); //GPIO with internal pullup, goes low when button is pushed
  pinMode(interruptPin, INPUT); //High-impedance input until we have an int and then we output low. Pulled high with 10k with cuttable jumper.

  //Disable ADC
  ADCSRA = 0;

  //Disable Brown-Out Detect
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS);

  //Power down various bits of hardware to lower power usage
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN); //May turn off millis
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  readSystemSettings(&registerMap); //Load all system settings from EEPROM

#if defined(__AVR_ATmega328P__)
  //Debug values
  registerMap.ledBrightness = 255; //Max brightness
  registerMap.ledPulseGranularity = 1; //Amount to change LED at each step

  registerMap.ledPulseCycleTime = 500; //Total amount of cycle, does not include off time. LED pulse disabled if zero.
  registerMap.ledPulseOffTime = 500; //Off time between pulses
#endif

  onboardLED.update(&registerMap); //update LED variables, get ready for pulsing
  setupInterrupts(); //Enable pin change interrupts for I2C, switch, etc
  updateI2C(&registerMap); //Determine the I2C address we should be using and begin listening on I2C bus

#if defined(__AVR_ATmega328P__)
  Serial.begin(115200);
  Serial.println("Qwiic Button");
  Serial.print("Address: 0x");
  Serial.println(registerMap.i2cAddress, HEX);
#endif

  digitalWrite(statusLedPin, HIGH); //turn on the status LED to notify that we've setup everything properly  
}

void loop(void) {
  if (updateFlag == true){
    //Calculate LED values based on pulse settings if anything has changed
    onboardLED.update(&registerMap);

    //update interruptPin output
    if(registerMap.interruptConfig.status) { //if the interrupt is triggered
      pinMode(interruptPin, OUTPUT); //make the interrupt pin a low-impedance connection to ground
      digitalWrite(interruptPin, LOW);
    }

    else { //go to high-impedance mode on the interrupt pin if the interrupt is not triggered
      pinMode(interruptPin, INPUT);
    }

    //update our I2C address if necessary
    updateI2C(&registerMap);

    //Record anything new to EEPROM (like new LED values)
    //It can take ~3.4ms to write a byte to EEPROM so we do that here instead of in an interrupt
    recordSystemSettings(&registerMap);

    updateFlag = false; //clear flag
  }

  sleep_mode(); //Stop everything and go to sleep. Wake up if I2C event occurs.

  updateI2C(&registerMap);
  onboardLED.pulse(ledPin); //update the brightness of the LED
}

/*
Update slave I2C address to what's configured with registerMap.i2cAddress and/or the address jumpers.
Only stops the I2C bus if the address to switch to is different than the previous one, so we can call it as much as we like!
Returns 1 if the address was changed, or 0 if the address wasn't changed. */

bool updateI2C(memoryMap* map) {
  uint8_t proposedAddress = NULL;

    //if we're on the button, check all three IO pins to see if we're going to get the address from jumpers or registerMap
    if(DEVICE_ID == BUTTON_DEVICE_ID) {
        uint8_t IOaddress = DEFAULT_I2C_ADDRESS;
        bitWrite(IOaddress, 0, !digitalRead(addressPin0));
        bitWrite(IOaddress, 1, !digitalRead(addressPin1));
        bitWrite(IOaddress, 2, !digitalRead(addressPin2));
        bitWrite(IOaddress, 3, !digitalRead(addressPin3));

        //if any of the address jumpers are set, we use jumpers
        if(IOaddress != DEFAULT_I2C_ADDRESS) {
            // note: we're not checking to see if this is a valid address, as it's implied that whoever is editing the
            // #defines at the top of the file knows what they're doing
            proposedAddress = IOaddress;
        }

        //if none of the address jumpers are set, we use registerMap (but check to make sure that the value is legal first)
        else {
            //if the value is legal, then set it
            if(map->i2cAddress > 0x07 && map->i2cAddress < 0x78) proposedAddress = map->i2cAddress;

            //if the value is illegal, default to the default I2C address for our platform
            else proposedAddress = DEFAULT_I2C_ADDRESS;
        }
    }

    //if we're on the switch, just check the one IO pin to see if we're going to get the address from jumpers or registerMap
    if(DEVICE_ID == SWITCH_DEVICE_ID) {
        //if the jumper is closed, use the forced I2C address

        // note: we're not checking to see if this is a valid address, as it's implied that whoever is editing the 
        // #defines at the top of the file knows what they're doing
        if(!digitalRead(addressPin0)) proposedAddress = FORCED_I2C_ADDRESS;

        //if the jumper isn't closed, we use registerMap (but check to make sure that the value is legal first)
        else {
            //if the value is legal, then set it
            if(map->i2cAddress > 0x07 && map->i2cAddress < 0x78) proposedAddress = map->i2cAddress;

            //if the value is illegal, default to the default I2C address for our platform
            else proposedAddress = DEFAULT_I2C_ADDRESS;
        }
    }

    //if proposedAddress is different from the existing address OR I2C hasn't been init'd before, reconfigure the Wire instance
    if( (proposedAddress != map->i2cAddress) || (!map->buttonStatus.isReady) ) {

        //save new address to the register map
        map->i2cAddress = proposedAddress;

        //reconfigure Wire instance
        Wire.end(); //stop I2C on old address
        Wire.begin(proposedAddress); //rejoin the I2C bus on new address

        //The connections to the interrupts are severed when a Wire.begin occurs, so here we reattach them
        Wire.onReceive(receiveEvent);
        Wire.onRequest(requestEvent);
        map->buttonStatus.isReady = true;
        return 1;
    }
    return 0;
}

//Reads the current system settings from EEPROM
//If anything looks weird, reset setting to default value
void readSystemSettings(memoryMap* map){
  //Read what I2C address we should use
  EEPROM.get(LOCATION_I2C_ADDRESS, map->i2cAddress);
  if (map->i2cAddress == 255){
    map->i2cAddress = DEFAULT_I2C_ADDRESS; //By default, we listen for DEFAULT_I2C_ADDRESS
    EEPROM.update(LOCATION_I2C_ADDRESS, map->i2cAddress);
  }

  //Error check I2C address we read from EEPROM
  if (map->i2cAddress < 0x08 || map->i2cAddress > 0x77){
    //User has set the address out of range
    //Go back to defaults
    map->i2cAddress = DEFAULT_I2C_ADDRESS;
    EEPROM.update(LOCATION_I2C_ADDRESS, map->i2cAddress);
  }

  //Read the interrupt bits
  EEPROM.get(LOCATION_INTERRUPTS, map->interruptConfig.byteWrapped);
  if (map->interruptConfig.byteWrapped == 0xFF){ //Blank
    map->interruptConfig.byteWrapped = 0x03; //By default, enable the click and pressed interrupts
    EEPROM.update(LOCATION_INTERRUPTS, map->interruptConfig.byteWrapped);
  }

  EEPROM.get(LOCATION_LED_PULSEGRANULARITY, map->ledPulseGranularity);
  if (map->ledPulseGranularity == 0xFF){
    map->ledPulseGranularity = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSEGRANULARITY, map->ledPulseGranularity);
  }

  EEPROM.get(LOCATION_LED_PULSECYCLETIME, map->ledPulseCycleTime);
  if (map->ledPulseCycleTime == 0xFFFF){
    map->ledPulseCycleTime = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSECYCLETIME, map->ledPulseCycleTime);
  }

  EEPROM.get(LOCATION_LED_PULSEOFFTIME, map->ledPulseOffTime);
  if (map->ledPulseOffTime == 0xFFFF){
    map->ledPulseOffTime = 0; //Default to none
    EEPROM.update(LOCATION_LED_PULSECYCLETIME, map->ledPulseOffTime);
  }

  EEPROM.get(LOCATION_BUTTON_DEBOUNCE_TIME, map->buttonDebounceTime);
  if (map->buttonDebounceTime == 0xFFFF){
    map->buttonDebounceTime = 10; //Default to 10ms
    EEPROM.update(LOCATION_BUTTON_DEBOUNCE_TIME, map->buttonDebounceTime);
  }

  //Read the starting value for the LED
  EEPROM.get(LOCATION_LED_BRIGHTNESS, map->ledBrightness);
  if (map->ledPulseCycleTime > 0){
    //Don't turn on LED, we'll pulse it in main loop
    analogWrite(ledPin, 0);
  }
  else { //Pulsing disabled
    //Turn on LED to setting
    analogWrite(ledPin, map->ledBrightness);
  }
}

//If the current setting is different from that in EEPROM, update EEPROM
void recordSystemSettings(memoryMap* map) {
  //Error check the current I2C address
  if (map->i2cAddress < 0x08 || map->i2cAddress > 0x77){
    //User has set the address out of range
    //Go back to defaults
    map->i2cAddress = DEFAULT_I2C_ADDRESS;
    updateI2C(map); //Determine the I2C address we should be using and begin listening on I2C bus
  }

  EEPROM.update(LOCATION_I2C_ADDRESS, map->i2cAddress);
  EEPROM.update(LOCATION_INTERRUPTS, map->interruptConfig.byteWrapped);
  EEPROM.update(LOCATION_LED_BRIGHTNESS, map->ledBrightness);
  EEPROM.update(LOCATION_LED_PULSEGRANULARITY, map->ledPulseGranularity);
  EEPROM.update(LOCATION_LED_PULSECYCLETIME, map->ledPulseCycleTime);
  EEPROM.update(LOCATION_LED_PULSEOFFTIME, map->ledPulseOffTime);
  EEPROM.update(LOCATION_BUTTON_DEBOUNCE_TIME, map->buttonDebounceTime);
}