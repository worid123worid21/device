CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
TARGET  = pantilt_ctrl
SRCS    = main.c servo_module.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean