COMPILER         = g++
OPTIONS          = -std=c++11
LIBRARIES        = -lrt -pthread

TARGET = cash_server
CPP = main.cpp

BUILD_LIST += build
BUILD_LIST += run

N = 1000

all: $(BUILD_LIST)

build: $(CPP)
	$(COMPILER) $(OPTIONS) -o $(TARGET) $(CPP) $(LIBRARIES)

run: $(TARGET)
	./$(TARGET) $(N)

clean:
	rm -rf $(TARGET) *.o *~
