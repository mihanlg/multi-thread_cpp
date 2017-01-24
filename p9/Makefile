COMPILER         = g++
OPTIONS          = -std=c++11 -fopenmp -o

BUILD_LIST += build
BUILD_LIST += run

N = 10000000

all: $(BUILD_LIST)

build: main.cpp
	$(COMPILER) $(OPTIONS) sort_openmp main.cpp

run: sort_openmp
	./sort_openmp $(N)

clean:
	rm -rf *.o
