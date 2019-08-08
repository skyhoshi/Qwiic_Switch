:loop

@echo Programming the Qwiic Button. If this looks incorrect, abort and retry.
@pause
@echo -
@echo Flashing bootloader...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -e -Uefuse:w:0xFF:m -Uhfuse:w:0b11010111:m -Ulfuse:w:0xE2:m
@pause
@echo -
@echo Flashing code...
@avrdude -C avrdude.conf -pattiny84 -cusbtiny -Uflash:w:Qwiic_Button_Firmware.ino.hex
@echo Done!
@pause

goto loop