@echo Programming the Qwiic Button. If this looks incorrect, abort and retry.
@pause
:loop

@echo -
@echo Flashing bootloader...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -e -Uefuse:w:0xFF:m -Uhfuse:w:0b11010111:m -Ulfuse:w:0xE2:m


@echo -
@echo Erasing EEPROM...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -Uflash:w:EEPROM_erase.ino.hex:i

@timeout 1

@echo -
@echo Flashing firmware...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -e -Uflash:w:Qwiic_Button.ino.hex:i

@echo Done programming! Move on to next board.
@pause

goto loop