@echo Programming the Qwiic Button. If this looks incorrect, abort and retry.
@pause
:loop

@echo -
@echo Flashing bootloader...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -e -Uefuse:w:0xFF:m -Uhfuse:w:0b11010111:m -Ulfuse:w:0xE2:m
@timeout 1

@echo -
@echo Resetting EEPROM...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -Uflash:w:Reset_EEPROM.ino.hex
@echo EEPROM reset complete...
@timeout 1

@echo -
@echo Flashing firmware...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -Uflash:w:Qwiic_Button_Firmware.ino.hex
@echo Done programming! Move on to next board.
@pause

goto loop