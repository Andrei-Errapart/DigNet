set MICRO=../../../Micro
make.exe -f %MICRO%/Makefile NAME=modembox_crawler MCU=atmega1280 "SRC=cmrpcktctrl.c main.c statledctrl.c usart.c" LFUSE=0xFF HFUSE=0xD9 EFUSE=0xFC CFLAGS="-DF_CPU=6000000" %*

