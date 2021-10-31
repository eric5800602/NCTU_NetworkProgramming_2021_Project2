N_PATH := bin
COMMANDS_PATH := commands

all:np_simple.cpp
	g++ np_simple.cpp -o np_simple
	g++ np_single_proc.cpp -o np_single_proc
all_with_commands:
	g++ np_simple.cpp -o np_simple
	g++ np_single_proc.cpp -o np_single_proc
clean:
	rm -f np_simple
	rm -f np_single_proc
