# Default CFLAGS for older Pis
CFLAGS = -Wall -lgpiod
TARGET = pi-latency
SRC = pi-latency.c

# Detect if we are on a Pi 5 (aarch64)
PI5_ARCH := $(shell uname -m | grep aarch64)

# Pi 5 specific build
ifneq ($(PI5_ARCH),)
    # Change libraries and add compile flag for Pi 5
    CFLAGS = -Wall -lpiolib -DIS_PI5
endif

# Final compile rule
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
