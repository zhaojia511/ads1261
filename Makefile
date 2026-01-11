PYTHON = python3

.PHONY: build flash monitor clean

build:
	idf.py build

flash: build
	idf.py -p /dev/ttyUSB0 flash

monitor:
	idf.py -p /dev/ttyUSB0 monitor

clean:
	idf.py fullclean

all: build flash monitor
