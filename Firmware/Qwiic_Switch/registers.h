/******************************************************************************
registers.h
Fischer Moseley @ SparkFun Electronics
Original Creation Date: July 31, 2019

This file defines the memoryMap struct, which acts as the pseudo register map
of the Qwiic Button/Switch. It also serves as an easy way to access variables
and manipulate the state of the device.

During I2C transactions, the memoryMap object is wrapped as a collection of
bytes. The byte that the user is interested in (either to read or write) is
selected with a register pointer. For instance, if the user sets the pointer
to 0x0e, they will be addressing the 4th uint8_t sized object in this struct.
In this case, that would be the interruptConfig register!

This code is beerware; if you see me (or any other SparkFun employee) at the
local, and you've found our code helpful, please buy us a round!

Distributed as-is; no warranty is given.
******************************************************************************/

typedef union {
  struct {
    bool : 6;
    bool isPressed : 1;  //not mutable by user, set to zero if button is not pushed, set to one if button is pushed
    bool hasBeenClicked : 1; //mutable by user, basically behaves like an interrupt. Defaults to zero on POR, but gets set to one every time the button gets clicked. Can be cleared by the user, and that happens regularly in the accompnaying arduino library
  };
  uint8_t byteWrapped;
} statusRegisterBitField;

typedef union {
  struct {
    bool: 5;
    bool pressedEnable : 1; //user mutable, set to 1 to enable an interrupt when the button is pressed. Defaults to 0.
    bool clickedEnable : 1; //user mutable, set to 1 to enable an interrupt when the button is clicked. Defaults to 0.
    bool status : 1; //user mutable, gets set to 1 when the interrupt is triggered. User is expected to write 0 to clear the interrupt.
  };
  uint8_t byteWrapped;
} interruptConfigBitField;

typedef union {
  struct {
    bool: 5;
    bool isFull : 1; //user immutable, returns 1 or 0 depending on whether or not the queue is full
    bool isEmpty : 1; //user immutable, returns 1 or 0 depending on whether or not the queue is empty
    bool popRequest : 1; //user mutable, user sets to 1 to pop from queue, we pop from queue and set the bit back to zero.
  };
  uint8_t byteWrapped;
} queueStatusBitField;

typedef struct memoryMap { 
  //Button Status/Configuration                       Register Address
  statusRegisterBitField buttonStatus;                    // 0x00
  uint16_t buttonDebounceTime;                            // 0x01

  //Interrupt Configuration
  interruptConfigBitField interruptConfig;                // 0x03
  
  //ButtonPressed queue manipulation and status functions
  queueStatusBitField pressedQueueStatus;                 // 0x04      
  unsigned long pressedQueueFront;                        // 0x05
  unsigned long pressedQueueBack;                         // 0x09 

  queueStatusBitField clickedQueueStatus;                 // 0x0D
  unsigned long clickedQueueFront;                        // 0x0E
  unsigned long clickedQueueBack;                         // 0x12

  //LED Configuration
  uint8_t ledBrightness;                                  // 0x16
  uint8_t ledPulseGranularity;                            // 0x17
  uint16_t ledPulseCycleTime;                             // 0x18
  uint16_t ledPulseOffTime;                               // 0x1A

  //Device Configuration
  uint8_t i2cAddress;                                     // 0x1C
  uint8_t id;                                             // 0x1D
  uint8_t firmwareMinor;                                  // 0x1E
  uint8_t firmwareMajor;                                  // 0x1F
};