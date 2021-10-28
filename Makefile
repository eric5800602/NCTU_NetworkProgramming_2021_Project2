N_PATH := bin
COMMANDS_PATH := commands

all:np_simple.cpp
	g++ np_simple.cpp -o np_simple
all_with_commands:
	g++ np_simple.cpp -o np_simple
clean:
	rm -f np_simple
