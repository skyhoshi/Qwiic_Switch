/* This file defines the pseudo register map of the Qwiic Switch/Button, and which bits
contain what information in the register bytes. */

//This is the pseudo register map of the product. If user asks for 0x02 then get the 3rd
//uint8_t inside the register map.
struct memoryMap {
  uint8_t id;
  uint8_t status;
  uint8_t firmwareMinor;
  uint8_t firmwareMajor;
  uint8_t interruptEnable;
  uint16_t timeSinceLastButtonPressed;
  uint16_t timeSinceLastButtonClicked;
  uint8_t ledBrightness; //Brightness of LED. If pulse cycle enabled, this is the max brightness of the pulse.
  uint8_t ledPulseGranularity; //Number of steps to take to get to ledBrightness. 1 is best for most applications.
  uint16_t ledPulseCycleTime; //Total pulse cycle in ms, does not include off time. LED pulse disabled if zero.
  uint16_t ledPulseOffTime; //Off time between pulses, in ms
  uint16_t buttonDebounceTime;
  uint8_t i2cAddress;
};

const uint8_t statusButtonPressedBufferEmptyBit = 5;
const uint8_t statusButtonPressedBufferFullBit = 4;
const uint8_t statusButtonClickedBufferEmptyBit = 3;
const uint8_t statusButtonClickedBufferFullBit = 2;
const uint8_t statusButtonClickedBit = 1;
const uint8_t statusButtonPressedBit = 0;

const uint8_t enableInterruptButtonClickedBit = 1;
const uint8_t enableInterruptButtonPressedBit = 0;