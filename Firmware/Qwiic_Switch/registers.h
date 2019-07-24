/* This file defines the pseudo register map of the Qwiic Switch/Button, and which bits
contain what information in the register bytes. */

//This is the pseudo register map of the product. If user asks for 0x02 then get the 3rd
//byte inside the register map.
struct memoryMap {
  byte id;
  byte status;
  byte firmwareMinor;
  byte firmwareMajor;
  byte interruptEnable;
  uint16_t timeSinceLastButtonPressed;
  uint16_t timeSinceLastButtonClicked;
  byte ledBrightness; //Brightness of LED. If pulse cycle enabled, this is the max brightness of the pulse.
  byte ledPulseGranularity; //Number of steps to take to get to ledBrightness. 1 is best for most applications.
  uint16_t ledPulseCycleTime; //Total pulse cycle in ms, does not include off time. LED pulse disabled if zero.
  uint16_t ledPulseOffTime; //Off time between pulses, in ms
  uint16_t buttonDebounceTime;
  byte i2cAddress;
};

const byte statusButtonPressedBufferEmptyBit = 5;
const byte statusButtonPressedBufferFullBit = 4;
const byte statusButtonClickedBufferEmptyBit = 3;
const byte statusButtonClickedBufferFullBit = 2;
const byte statusButtonClickedBit = 1;
const byte statusButtonPressedBit = 0;

const byte enableInterruptButtonClickedBit = 1;
const byte enableInterruptButtonPressedBit = 0;