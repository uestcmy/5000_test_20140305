CC = gcc
XX = g++

DEBUG_FLAG = -O0 -g3 -Wall 
RELEASE_FLAG = -O0 -Wall

TARGET = ./ml605_api_test

%.o: %.c
	$(CC) $(RELEASE_FLAG) -c -I ../include/ $< -o $@
%.o: %.cpp
	$(XX) $(RELEASE_FLAG) -c -I ../include/ $< -o $@
	
SOURCES = $(wildcard *.c *.cpp)
OBJS = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES)))

$TARGET: $(OBJS)
	$(XX) $(OBJS) -o $(TARGET) -lpthread
	chmod a+x $(TARGET)
	
clean:
	rm -rf *.o $(TARGET)
