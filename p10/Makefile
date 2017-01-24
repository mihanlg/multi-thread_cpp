COMPILER         = -mpic++.mpich
EXECUTOR         = mpiexec.mpich
OPTIONS          = -o 
LINKER_OPT       = 

N = 100

BUILD_LIST += build
BUILD_LIST += start

all: $(BUILD_LIST)

build: main.cpp
	$(COMPILER) $(OPTIONS) exp_mpi main.cpp $(LINKER_OPT)

exec: exp_mpi
	$(EXECUTOR) ./exp_mpi $(N)

clean:
	rm -rf *.o
