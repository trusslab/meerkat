CC := $(CXX)

BINDIR := bin/
BUILDDIR := build/
INCDIR := include/
LIBDIR := lib/
SRCDIR := src/
HELPERDIR := helpers/
TOOLDIR := tools/
PROJECTNAME := bisector

.DEFAULT_GOAL := $(PROJECTNAME)

.PHONY: all $(PROJECTNAME) helpers tools syzkaller

all: $(PROJECTNAME) helpers tools syzkaller

$(BUILDDIR)%.o: $(LIBDIR)%.cpp $(INCDIR)%.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -c $< -o $@

$(BUILDDIR)file_api.o: $(LIBDIR)file_api.cpp $(INCDIR)file_api.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -std=c++17 -lstdc++fs -c $< -o $@

ALL_OBJS = $(BUILDDIR)argparse.o $(BUILDDIR)version.o $(BUILDDIR)template_parse.o\
			$(BUILDDIR)date.o $(BUILDDIR)environment.o $(BUILDDIR)exec_api.o \
			$(BUILDDIR)file_api.o $(BUILDDIR)fuzz_prep.o $(BUILDDIR)fuzz.o $(BUILDDIR)git.o \
			$(BUILDDIR)my_string.o $(BUILDDIR)psf.o $(BUILDDIR)syzlang.o $(BUILDDIR)json.o \
			$(BUILDDIR)result.o $(BUILDDIR)bisect.o $(BUILDDIR)shell_api.o $(BUILDDIR)port.o \
			$(BUILDDIR)syzkaller.o $(BUILDDIR)vm.o

$(PROJECTNAME): $(SRCDIR)$(PROJECTNAME).cpp $(ALL_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)$(PROJECTNAME).o"
	@$(CC) -I $(INCDIR) -c $(SRCDIR)$(PROJECTNAME).cpp -o $(BUILDDIR)$(PROJECTNAME).o
	@echo "  LN     $(BINDIR)$(PROJECTNAME)"
	@$(CC) $(BUILDDIR)$(PROJECTNAME).o $(ALL_OBJS) -o $(BINDIR)$(PROJECTNAME)

helpers: diffdate

diffdate: $(HELPERDIR)diffdate.cpp | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)diffdate.o"
	@$(CC) -I $(INCDIR) -c $(HELPERDIR)diffdate.cpp -o $(BUILDDIR)diffdate.o
	@echo "  LN     $(HELPERDIR)diffdate"
	@$(CC) $(BUILDDIR)diffdate.o $(DD_OBJS) -o $(HELPERDIR)diffdate

tools: git_test description_test runner

GT_OBJS = $(BUILDDIR)git.o $(BUILDDIR)shell_api.o $(BUILDDIR)date.o $(BUILDDIR)file_api.o \
			$(BUILDDIR)exec_api.o $(BUILDDIR)my_string.o $(BUILDDIR)version.o

git_test: $(TOOLDIR)git_test.cpp $(GT_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)git_test.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)git_test.cpp -o $(BUILDDIR)git_test.o
	@echo "  LN     $(TOOLDIR)git_test"
	@$(CC) $(BUILDDIR)git_test.o $(GT_OBJS) -o $(BINDIR)git_test

DT_OBJS = $(BUILDDIR)syzlang.o $(BUILDDIR)template_parse.o $(BUILDDIR)file_api.o \
			$(BUILDDIR)argparse.o $(BUILDDIR)environment.o $(BUILDDIR)json.o \
			$(BUILDDIR)my_string.o $(BUILDDIR)exec_api.o $(BUILDDIR)date.o \
			$(BUILDDIR)port.o

description_test: $(TOOLDIR)description_test.cpp $(DT_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)description_test.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)description_test.cpp -o $(BUILDDIR)description_test.o
	@echo "  LN     $(TOOLDIR)description_test"
	@$(CC) $(BUILDDIR)description_test.o $(DT_OBJS) -o $(BINDIR)description_test

RR_OBJS = $(BUILDDIR)file_api.o $(BUILDDIR)argparse.o $(BUILDDIR)environment.o \
			$(BUILDDIR)json.o $(BUILDDIR)my_string.o $(BUILDDIR)exec_api.o \
			$(BUILDDIR)date.o $(BUILDDIR)port.o $(BUILDDIR)vm.o $(BUILDDIR)syzkaller.o \
			$(BUILDDIR)fuzz.o $(BUILDDIR)fuzz_prep.o $(BUILDDIR)shell_api.o $(BUILDDIR)result.o \
			$(BUILDDIR)bisect.o $(BUILDDIR)git.o $(BUILDDIR)version.o

runner: $(TOOLDIR)runner.cpp $(RR_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)runner.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)runner.cpp -o $(BUILDDIR)runner.o
	@echo "  LN     $(TOOLDIR)runner"
	@$(CC) $(BUILDDIR)runner.o $(RR_OBJS) -o $(BINDIR)runner

$(BUILDDIR):
	@echo "DIR    $(BUILDDIR)"
	@mkdir $(BUILDDIR)

$(BINDIR):
	@echo "DIR    $(BINDIR)"
	@mkdir $(BINDIR)

syzkaller:
	$(MAKE) -C syzkaller/
	$(MAKE) -C syzkaller/ symbolize

clean:
	$(RM) -r $(BINDIR) $(BUILDDIR)
	$(RM) $(HELPERDIR)diffdate
	$(MAKE) -C syzkaller/ clean
