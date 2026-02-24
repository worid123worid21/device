CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread

TARGET  = pantilt_ctrl
SRCS    = main.c servo_module.c

all: $(TARGET)

$(TARGET): $(SRCS) servo_module.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) -lm

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean