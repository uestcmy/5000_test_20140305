CC = gcc 
myflag= -Wall -Wextra -ggdb3 
CFLAGS =  -DV6_TRD -DHARDWARE -I$(PWD) `pkg-config --cflags gtk+-2.0`
#CFLAGS = -DV6_TRD -I$(PWD) `pkg-config --cflags gtk+-2.0` -I../include/
LFLAGS = `pkg-config --libs gtk+-2.0`

GUI_OBJS = xpmon.o

all = GUI

GUI:  $(GUI_OBJS) 
	$(CC) -o xpmon $(CFLAGS) $(GUI_OBJS) $(LFLAGS)


xpmon.o:  ConfigGui.c ../include/xpmon_be.h ConfigGui.h
	$(CC) -c $(CFLAGS) -I ../include/ ConfigGui.c -o xpmon.o 


clean:
	rm -f $(GUI_OBJS) xpmon *.o *.out *.h~  *.c~ Makefile~ *.*.orig


