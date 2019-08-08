/******************************************************************************
Validates that the attached device is working.

Fischer Moseley @ SparkFun Electronics
Original Creation Date: August 8, 2019

This code is Lemonadeware; if you see me (or any other SparkFun employee) at the
local, and you've found our code helpful, please buy us a round!

Hardware Connections:
Attach the Qwiic Shield to your Arduino/Photon/ESP32 or other
Plug the button into the shield
Print it to the serial monitor at 115200 baud.

Distributed as-is; no warranty is given.
******************************************************************************/

#define I2C_ADDRESS DEFAULT_BUTTON_ADDRESS //change to DEFAULT_SWITCH_ADDRESS if you're using Qwiic Switch or Qwiic Arcade

#include <SparkFun_Qwiic_Button.h>
QwiicButton button;

void setup(){
    Serial.begin(115200);
    Wire.begin(); //Join I2C bus
    Wire.setClock(400000); //Set I2C clock speed to 400kHz
    button.begin(I2C_ADDRESS);

    //reset to default settings
    button.setDebounceTime(10);
    button.LEDconfig(0,1,0,0);
    button.resetInterruptConfig();

    //check if button will acknowledge over I2C
    if(button.isConnected()){
        Serial.println("Device will acknowledge!");
    }

    else {
        Serial.println("Device did not acknowledge! Freezing.");
        while(1);
    }
}

void loop(){
    if(button.isPressed()) button.LEDon(200);
    if(!button.isPressed()) button.LEDoff();

    if(button.isPressedQueueEmpty() == false) { //if the queue of pressed events is not empty
        //then print the time since the last and first button press
        Serial.print(button.timeSinceLastPress()/1000.0);
        Serial.print("s since the button was last pressed   ");
        Serial.print(button.timeSinceFirstPress()/1000.0);
        Serial.print("s since the button was first pressed   ");
    }
    
    //if the queue of pressed events is empty, just print that the queue is empty!
    if(button.isPressedQueueEmpty() == true) {
        Serial.print("ButtonPressed Queue is empty! ");
    } 

    if(button.isClickedQueueEmpty() == false) { //if the queue of clicked events is not empty
        //then print the time since the last and first button click
        Serial.print(button.timeSinceLastClick()/1000.0);
        Serial.print("s since the button was last clicked   ");
        Serial.print(button.timeSinceFirstClick()/1000.0);
        Serial.print("s since the button was first clicked");
    }
    //if the queue of clicked events is empty, just print that the queue is empty!
    if(button.isPressedQueueEmpty() == true) {
        Serial.print("  ButtonClicked Queue is empty!");
    }

    Serial.println(); //print a new line to not clutter up the serial monitor

    if(Serial.available()) { //if the user sent a character
        
        uint8_t data = Serial.read();
        if(data == 'p' || data == 'P') { //if the character is p or P, then pop a value off of the pressed Queue
            button.popPressedQueue();
            Serial.println("Popped PressedQueue!");
        }

        if(data == 'c' || data == 'C') { //if the character is c or C, then pop a value off of the pressed Queue
            button.popClickedQueue();
            Serial.println("Popped ClickedQueue!");
        }
    }
    delay(20); //let's not hammer too hard on the I2C bus
}