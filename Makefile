all:
	g++ -I include/ main.cpp lib/* -l git2 -std=c++17 -lstdc++fs -o syzInspector

clean:
	rm syzInspector