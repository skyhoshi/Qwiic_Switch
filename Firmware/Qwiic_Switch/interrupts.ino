//Turn on interrupts for the various pins
void setupInterrupts()
{
  //Attach interrupt to switch
  attachPCINT(digitalPinToPCINT(switchPin), buttonInterrupt, CHANGE);
}

//When Qwiic Button receives data bytes from Master, this function is called as an interrupt
void receiveEvent(int numberOfBytesReceived)
{
  registerNumber = Wire.read(); //Get the memory map offset from the user

  //Begin recording the following incoming bytes to the temp memory map
  //starting at the registerNumber (the first byte received)
  for (byte x = 0 ; x < numberOfBytesReceived - 1 ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    if ( (x + registerNumber) < sizeof(memoryMap))
    {
      //Clense the incoming byte against the read only protected bits
      //Store the result into the register map
      *(registerPointer + registerNumber + x) &= ~*(protectionPointer + registerNumber + x); //Clear this register if needed
      *(registerPointer + registerNumber + x) |= temp & *(protectionPointer + registerNumber + x); //Or in the user's request (clensed against protection bits)
    }
  }

  //If the user has zero'd out one of the time registers then zero out then advance the tail
  //and load the next event
  if (registerMap.timeSinceLastButtonPressed == 0)
  {
    if (buttonStackPressedTail != buttonStackPressedHead)
    {
      buttonStackPressedTail++;
      buttonStackPressedTail %= BUTTON_STACK_SIZE;
      registerMap.timeSinceLastButtonPressed = buttonTimePressedStack[buttonStackPressedTail];
    }
  }

  if (registerMap.timeSinceLastButtonClicked == 0)
  {
    if (buttonStackClickedTail != buttonStackClickedHead)
    {
      buttonStackClickedTail++;
      buttonStackClickedTail %= BUTTON_STACK_SIZE;
      registerMap.timeSinceLastButtonClicked = buttonTimeClickedStack[buttonStackClickedTail];
    }
  }

  if (interruptState == STATE_INT_INDICATED)
  {
    //If the user has cleared all the interrupt bits then clear interrupt pin
    if ( (registerMap.status & (1 << statusButtonClickedBit)) == 0
         && (registerMap.status & (1 << statusButtonPressedBit)) == 0
       )
    {
      //This will set the int pin to high impedance (aka pulled high by external resistor)
      digitalWrite(interruptPin, LOW); //Push pin to disable internal pull-ups
      pinMode(interruptPin, INPUT); //Go to high impedance

      interruptState = STATE_INT_CLEARED; //Go to next state
    }
  }

  updateOutputs = true; //Update things like LED brightnesses in the main loop
}

//Respond to GET commands
//When Qwiic Button gets a request for data from the user, this function is called as an interrupt
//The interrupt will respond with bytes starting from the last byte the user sent to us
//While we are sending bytes we may have to do some calculations
void requestEvent()
{
  //Calculate time stamps before we start sending bytes via I2C
  if (buttonTimePressedStack[buttonStackPressedTail] > 0)
    registerMap.timeSinceLastButtonPressed = millis() - buttonTimePressedStack[buttonStackPressedTail];

  if (buttonTimePressedStack[buttonStackClickedTail] > 0)
    registerMap.timeSinceLastButtonClicked = millis() - buttonTimeClickedStack[buttonStackClickedTail];

  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.
  Wire.write((registerPointer + registerNumber), sizeof(memoryMap) - registerNumber);
}

//Called any time the pin changes state
void buttonInterrupt()
{
  delay(registerMap.buttonDebounceTime); //Software debounce

  interruptCount++; //For debug

  registerMap.status ^= (1 << statusButtonPressedBit); //Toggle the pressed bit
  buttonTimePressedStack[buttonStackPressedHead++] = millis();
  buttonStackPressedHead %= BUTTON_STACK_SIZE;

  if (digitalRead(switchPin) == LOW) //User has released the button, we have completed a click cycle
  {
    registerMap.status |= (1 << statusButtonClickedBit); //Set the clicked bit
    buttonTimeClickedStack[buttonStackClickedHead++] = millis();
    buttonStackClickedHead %= BUTTON_STACK_SIZE;
  }

  //Only change states if we are in a no-interrupt state.
  if (interruptState == STATE_INT_CLEARED)
  {
    //See if user has pressed or clicked the button
    if ( registerMap.status & (1 << statusButtonPressedBit)
         || registerMap.status & (1 << statusButtonClickedBit) )
    {
      //Check if pressed or clicked interrupt is enabled
      if ( registerMap.interruptEnable & (1 << enableInterruptButtonPressedBit)
           || registerMap.interruptEnable & (1 << enableInterruptButtonClickedBit))
      {
        interruptState = STATE_BUTTON_INT; //Go to next state
      }
    }
  }
}
