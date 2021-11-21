set MICRO=../../../Micro
rem make.exe -f %MICRO%/Makefile NAME=modembox_crawler MCU=atmega1280 "SRC=cmrpcktctrl.c main.c statledctrl.c usart.c" LFUSE=0xFF HFUSE=0xD9 EFUSE=0xFC CFLAGS="-DF_CPU=3686400 -DMODEMBOX_CRAWLER" %*

make.exe -f %MICRO%/Makefile NAME=modembox_crawler MCU=atmega1280 "SRC=cmrpcktctrl.c main.c statledctrl.c usart.c" LFUSE=0xFF HFUSE=0xD9 EFUSE=0xFC CFLAGS="-DF_CPU=3686400 -DMODEMBOX_CRAWLER" LDFLAGS="-Wl,-u,vfprintf -lprintf_flt -lm" %*
