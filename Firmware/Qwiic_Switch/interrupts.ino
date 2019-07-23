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
  if (registerMap.timeSinceLastButtonPressed == 0){
    registerMap.timeSinceLastButtonPressed = ButtonPressed.pop(); //Update the register with the next-oldest timestamp

    //Update the status register with the state of the ButtonPressed buffer
    bitWrite(registerMap.status, statusButtonPressedBufferEmptyBit, ButtonPressed.isEmpty());
    bitWrite(registerMap.status, statusButtonPressedBufferFullBit, ButtonPressed.isFull());
  }

  if (registerMap.timeSinceLastButtonClicked == 0){
    registerMap.timeSinceLastButtonClicked = ButtonClicked.pop();

    //Update the status register with the state of the ButtonClicked buffer
    bitWrite(registerMap.status, statusButtonClickedBufferEmptyBit, ButtonClicked.isEmpty());
    bitWrite(registerMap.status, statusButtonClickedBufferFullBit, ButtonClicked.isFull());
  }

  if (interruptState == STATE_INT_INDICATED)
  {
    //If the user has cleared all the interrupt bits then clear interrupt pin
    if ( !bitRead(registerMap.status, statusButtonPressedBit) //ask nate: is this the right register? should it be the interruptEnable register
        && !bitRead(registerMap.status, statusButtonClickedBit) )
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
  bitWrite(registerMap.status, statusButtonPressedBit, !digitalRead(switchPin));

  //Calculate time stamps before we start sending bytes via I2C
  if (ButtonPressed.back() > 0)
    registerMap.timeSinceLastButtonPressed = millis() - ButtonPressed.back();

  if (ButtonClicked.back() > 0)
    registerMap.timeSinceLastButtonClicked = millis() - ButtonClicked.back();

  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.

  Wire.write((registerPointer + registerNumber), sizeof(memoryMap) - registerNumber);
}

//Called any time the pin changes state
void buttonInterrupt()
{
  //delay(registerMap.buttonDebounceTime); //Software debounce
  if(!ButtonPressed.isEmpty()){
    if ( millis() < (10 + ButtonPressed.front()) ){
      return;
    }
  }
 
  interruptCount++; //For debug

  bitWrite(registerMap.status, statusButtonPressedBit, !digitalRead(switchPin));

  ButtonPressed.push(millis());
  bitWrite(registerMap.status, statusButtonPressedBufferEmptyBit, ButtonPressed.isEmpty());
  bitWrite(registerMap.status, statusButtonPressedBufferFullBit, ButtonPressed.isFull());

  if (digitalRead(switchPin) == HIGH) //User has released the button, we have completed a click cycle
  {
    bitWrite(registerMap.status, statusButtonClickedBit, 1);
    bitWrite(registerMap.status, statusButtonClickedBufferEmptyBit, ButtonClicked.isEmpty());
    bitWrite(registerMap.status, statusButtonClickedBufferFullBit, ButtonClicked.isFull());

    ButtonClicked.push(millis());
  }

  //Only change states if we are in a no-interrupt state.
  if (interruptState == STATE_INT_CLEARED)
  {
    //See if user has pressed or clicked the button
    if( bitRead(registerMap.status, statusButtonPressedBit) 
          || bitRead(registerMap.status, statusButtonClickedBit) )    
    {
      //Check if pressed or clicked interrupt is enabled
      if( bitRead(registerMap.interruptEnable, enableInterruptButtonPressedBit) 
          || bitRead(registerMap.interruptEnable, enableInterruptButtonClickedBit) )
      {
        interruptState = STATE_BUTTON_INT;
      }
    }
  }
}
