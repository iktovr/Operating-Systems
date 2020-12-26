CPP_FLAGS = -std=c++14 -Wall -pedantic -Werror -g
LD_FLAGS = -lpthread -lzmqpp -lzmq -lsodium

all: main node

main: main.cpp network.hpp
	g++ ${CPP_FLAGS} -o main main.cpp ${LD_FLAGS}

node: node.cpp network.hpp
	g++ ${CPP_FLAGS} -o node node.cpp ${LD_FLAGS}

clean:
	rm -f main node