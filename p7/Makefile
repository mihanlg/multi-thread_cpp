COMPILER         = g++
OPTIONS          = -std=c++11
LIBRARIES        =  -Wl,--no-as-needed -lpthread

TARGET = dict_server
CPP = main.cpp

BUILD_LIST += build
BUILD_LIST += run

FILENAME = ./dict_english.txt

all: $(BUILD_LIST)

build: $(CPP)
	$(COMPILER) $(OPTIONS) -o $(TARGET) $(CPP) $(LIBRARIES)

run: $(TARGET)
	./$(TARGET) $(FILENAME)

clean:
	rm -rf $(TARGET) *.o *~
