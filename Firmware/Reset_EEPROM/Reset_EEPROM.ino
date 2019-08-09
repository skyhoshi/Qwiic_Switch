#include <EEPROM.h>
void setup() {
    for(unsigned long addr; addr < EEPROM.length(); addr++) {
        EEPROM.update(addr, 0xFF);
    }
}

void loop() {
    for(unsigned long addr; addr < EEPROM.length(); addr++) {
        EEPROM.update(addr, 0xFF);
    }
}