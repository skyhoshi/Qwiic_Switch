//Location in EEPROM for each thing we want to store between power cycles
enum eepromLocations {
  LOCATION_I2C_ADDRESS, //Device's address
  LOCATION_INTERRUPTS,
  LOCATION_LED_BRIGHTNESS,
  LOCATION_LED_PULSEGRANULARITY,
  LOCATION_LED_PULSECYCLETIME,
  LOCATION_LED_PULSEOFFTIME,
  LOCATION_BUTTON_DEBOUNCE_TIME,
};

//Defaults for the I2C address
const byte I2C_ADDRESS_DEFAULT = 0x5F;
const byte I2C_FORCED_ADDRESS = 0x5E; //This is the address we go to incase user closes the address jumper