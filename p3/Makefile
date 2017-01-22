COMPILER         = -clang++
OPTIONS          = -std=c++11 -pedantic -Wall -Werror -o
LINKER_OPT       = -L/usr/lib -lboost_system

BUILD_LIST += proxy_server
BUILD_LIST += start

all: $(BUILD_LIST)

proxy_server: main.cpp
	$(COMPILER) $(OPTIONS) proxy_server main.cpp $(LINKER_OPT)

start: proxy_server
	./proxy_server settings

netcat: proxy_server
	./proxy_server netcat_settings

clean:
	rm -rf *.o