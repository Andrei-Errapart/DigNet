# fixme: very rudimentary makefile :(
SRC	:=	main.cxx

OBJS	:= $(SRC:.cxx=.o)

CFLAGS = -g -Wall -fpermissive
INCLUDES = -I ../../base/include -I ../../base/utils
LIBS = -L ../../base/utils -lutils
# DFLAGS = -D __USE_BSD

CC = g++
TARGET = RtcmEcho

all: $(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(DFLAGS) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o:	%.cxx
	$(CC) $(DFLAGS) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
