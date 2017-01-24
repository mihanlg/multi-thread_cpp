COMPILER         = g++
OPTIONS          = -std=c++11 -fopenmp -o

TARGET = sort_openmp
CPP = main.cpp

BUILD_LIST += build
BUILD_LIST += run

N = 10000000

all: $(BUILD_LIST)

build: $(CPP)
	$(COMPILER) $(OPTIONS) $(TARGET) $(CPP)

run: $(TARGET)
	./$(TARGET) $(N)

clean:
	rm -rf $(TARGET) *.o *~
