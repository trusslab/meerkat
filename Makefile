all:
	g++ -I include/ main.cpp lib/argparse.cpp lib/date.cpp -o syzInspector

clean:
	rm syzInspector