//Turn on interrupts for the various pins
void setupInterrupts()
{
  //Attach interrupt to switch
  attachPCINT(digitalPinToPCINT(switchPin), buttonInterrupt, CHANGE);
}

//When Qwiic Button receives data bytes from Master, this function is called as an interrupt
void receiveEvent(int numberOfBytesReceived){
  registerNumber = Wire.read(); //Get the memory map offset from the user

  //Begin recording the following incoming bytes to the temp memory map
  //starting at the registerNumber (the first byte received)
  for (uint8_t x = 0 ; x < numberOfBytesReceived - 1 ; x++){
    uint8_t temp = Wire.read(); //We might record it, we might throw it away

    if ( (x + registerNumber) < sizeof(memoryMap)){
      //Clense the incoming byte against the read only protected bits
      //Store the result into the register map
      *(registerPointer + registerNumber + x) &= ~*(protectionPointer + registerNumber + x); //Clear this register if needed
      *(registerPointer + registerNumber + x) |= temp & *(protectionPointer + registerNumber + x); //Or in the user's request (clensed against protection bits)
    }
  }

  //Update the ButtonPressed and ButtonClicked queues.
  //If the user has requested to pop the oldest event off the stack then do so!
  if (registerMap.pressedQueueStatus.popRequest){
    //Update the register with the next-oldest timestamp
    ButtonPressed.pop();
    registerMap.pressedQueueBack = ButtonPressed.back();

    //Update the status register with the state of the ButtonPressed buffer
    registerMap.pressedQueueStatus.isFull = ButtonPressed.isFull();
    registerMap.pressedQueueStatus.isEmpty = ButtonPressed.isEmpty();
    
    //Clear the popRequest bit so we know the popping is done
    registerMap.pressedQueueStatus.popRequest = false;
  }

  //If the user has requested to pop the oldest event off the stack then do so!
  if (registerMap.clickedQueueStatus.popRequest){
    //Update the register with the next-oldest timestamp
    ButtonClicked.pop();
    registerMap.clickedQueueBack = ButtonClicked.back();

    //Update the status register with the state of the ButtonClicked buffer
    registerMap.clickedQueueStatus.isFull = ButtonClicked.isFull();
    registerMap.clickedQueueStatus.isEmpty = ButtonClicked.isEmpty();

    //Clear the popRequest bit so we know the popping is done
    registerMap.clickedQueueStatus.popRequest = false;
  }

  //Update interrupt flag
  if (interruptState == STATE_INT_INDICATED){
    //If the user has cleared all the interrupt bits then clear interrupt pin
    //ask nate: is this the right register? should it be the interruptEnable register
    if (!registerMap.buttonStatus.isPressed && !registerMap.buttonStatus.hasBeenClicked){
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
void requestEvent(){
  registerMap.buttonStatus.isPressed = !digitalRead(switchPin); //have to take the inverse of the switch pin because the switch is pulled up, not pulled down

  //Calculate time stamps before we start sending bytes via I2C
  registerMap.pressedQueueBack = millis() - ButtonPressed.back();
  registerMap.pressedQueueFront = millis() - ButtonPressed.front();

  registerMap.clickedQueueBack = millis() - ButtonClicked.back();
  registerMap.clickedQueueFront = millis() - ButtonClicked.front();

  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.

  Wire.write((registerPointer + registerNumber), sizeof(memoryMap) - registerNumber);
}

//Called any time the pin changes state
void buttonInterrupt(){

  //Debounce by comparing with last value in the queue. If there is no last value, then this is the first and we should record it.
  if(!ButtonPressed.isEmpty()){
    if ( millis() < (registerMap.buttonDebounceTime + ButtonPressed.front()) ){
      return;
    }
  }
 
  interruptCount++; //For debug

  //Update the ButtonPressed queue and registerMap
  registerMap.buttonStatus.isPressed = !digitalRead(switchPin); //have to take the inverse of the switch pin because the switch is pulled up, not pulled down
  ButtonPressed.push(millis());
  registerMap.pressedQueueStatus.isEmpty = ButtonPressed.isEmpty();
  registerMap.pressedQueueStatus.isFull = ButtonPressed.isFull();

  //Update the ButtonClicked queue and registerMap if necessary
  if (digitalRead(switchPin) == HIGH){ //User has released the button, we have completed a click cycle
    //update the ButtonClicked queue and then registerMap
    registerMap.buttonStatus.hasBeenClicked = true;
    ButtonClicked.push(millis());
    registerMap.clickedQueueStatus.isEmpty = ButtonClicked.isEmpty();
    registerMap.clickedQueueStatus.isFull = ButtonClicked.isFull();
  }

  //Update interrupt flag!
  if(interruptState == STATE_INT_CLEARED){ //Only change states if we are in a no-interrupt state.
    //set the interrupt flag if the pressed interrupt is enabled and the button is pressed
    if(registerMap.interruptConfig.pressedEnable || registerMap.buttonStatus.isPressed)
      interruptState = STATE_BUTTON_INT;

    //set the interrupt flag if the clicked interrupt is enabled and the button has been clicked
    if(registerMap.interruptConfig.clickedEnable || registerMap.buttonStatus.hasBeenClicked)
      interruptState = STATE_BUTTON_INT;
  }
}
