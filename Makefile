all:
	#files="lib/argparse.cpp lib/date.cpp lib/bug_info.cpp lib/inspector_config.cpp lib/file_api.cpp lib/exec_api.cpp lib/shell_api.cpp lib/psf.cpp"
	g++ -I include/ main.cpp lib/* -l git2 -o syzInspector

clean:
	rm syzInspector