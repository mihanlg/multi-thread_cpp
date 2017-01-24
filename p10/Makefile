COMPILER         = mpic++.mpich
EXECUTOR         = mpiexec.mpich
OPTIONS          = -o

N = 100
TARGET = exp_mpi
CPP = main.cpp

BUILD_LIST += build
BUILD_LIST += start

all: $(BUILD_LIST)

build: $(CPP)
	$(COMPILER) $(OPTIONS) $(TARGET) $(CPP)

start: $(TARGET)
	$(EXECUTOR) ./$(TARGET) $(N)

clean:
	rm -rf $(TARGET) *.o *~
