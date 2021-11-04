N_PATH := bin
COMMANDS_PATH := commands

all:np_simple.cpp
	g++ np_simple.cpp -o np_simple
	g++ np_single_proc.cpp -o np_single_proc
	g++ np_multi_proc.cpp -o np_multi_proc -lrt
1:
	g++ np_simple.cpp -o np_simple
2:
	g++ np_single_proc.cpp -o np_single_proc
3:
	g++ np_multi_proc.cpp -o np_multi_proc -lrt
clean:
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc
