all:
	# lib/* = lib/argparse.cpp lib/date.cpp lib/bug_info.cpp lib/inspector_config.cpp lib/file_api.h
	g++ -I include/ main.cpp lib/* -l git2 -o syzInspector

clean:
	rm syzInspector