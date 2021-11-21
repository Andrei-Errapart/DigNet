# fixme: very rudimentary makefile :(
SRC	:=	\
		GpsServer.cxx			\
		DigNetDB.cxx			\
		DigNetMessages.cxx		\
		Counter.cxx			\
		FileDownloader.cxx		\
		Thread.cxx			\
		utils.cxx			\
		main.cxx			\
		SendEmail.cxx

OBJS	:= $(SRC:.cxx=.o)

CFLAGS = -g -Wall 
INCLUDES = -I ../../base/include -I ../../base/utils -I/usr/include/mysql
LIBS = -L ../../base/utils -lutils -levent -lpthread -lmysqlclient -lz -lrt
DFLAGS = -D __USE_BSD
#  -D FORCED_CRASH

CC = g++
TARGET = GpsServer

all: $(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(DFLAGS) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o:	%.cxx
	$(CC) $(DFLAGS) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
