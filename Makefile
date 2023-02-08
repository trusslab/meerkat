all:
	g++ -I include/ main.cpp lib/* -std=c++17 -lstdc++fs -o syzInspector

debug:
	g++ -I include/ main.cpp lib/* -std=c++17 -lstdc++fs -o syzInspector-debug

clean:
	rm syzInspector
