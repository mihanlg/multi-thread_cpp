COMPILER         = g++
OPTIONS          = -std=c++11

TARGET = shell2

OBJS += main.o
OBJS += call.o

BUILD_LIST += build
BUILD_LIST += test

all: $(OBJS) $(BUILD_LIST)

build: $(OBJS)
	$(COMPILER) $(OPTIONS) -o $(TARGET) $(OBJS)
	
run: $(TARGET)
	./$(TARGET)
	
test:
	./run_tests.sh ./$(TARGET)
	
./%.o: ./%.cpp
	$(COMPILER) $(OPTIONS) -c $< -o $@
  
./call.o: ./call.h

clean:
	rm -rf $(TARGET) *.o *~ *.file *.test *.txt tests/*.run.stderr tests/*run.out
