CC := $(CXX)

BINDIR := bin/
BUILDDIR := build/
INCDIR := include/
LIBDIR := lib/
SRCDIR := src/
TOOLDIR := tools/
PROJECTNAME := meerkat
PREFIX := mk-

.DEFAULT_GOAL := $(PROJECTNAME)

.PHONY: all $(PROJECTNAME) tools syzkaller

all-$(PROJECTNAME): $(PROJECTNAME) tools

all: all-$(PROJECTNAME) syzkaller

$(BUILDDIR)%.o: $(LIBDIR)%.cpp $(INCDIR)%.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -c $< -o $@

# Special build instruction here for the library
$(BUILDDIR)file_api.o: $(LIBDIR)file_api.cpp $(INCDIR)file_api.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -std=c++17 -lstdc++fs -c $< -o $@

ALL_OBJS = $(BUILDDIR)argparse.o $(BUILDDIR)version.o $(BUILDDIR)template_parse.o\
			$(BUILDDIR)date.o $(BUILDDIR)environment.o $(BUILDDIR)exec_api.o \
			$(BUILDDIR)file_api.o $(BUILDDIR)linux.o $(BUILDDIR)fuzz.o $(BUILDDIR)git.o \
			$(BUILDDIR)my_string.o $(BUILDDIR)syzlang.o $(BUILDDIR)json.o $(BUILDDIR)dedup.o \
			$(BUILDDIR)report.o $(BUILDDIR)bisect.o $(BUILDDIR)shell_api.o $(BUILDDIR)port.o \
			$(BUILDDIR)syzkaller.o $(BUILDDIR)vm.o $(BUILDDIR)make.o

$(PROJECTNAME): $(SRCDIR)$(PROJECTNAME).cpp $(ALL_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)$(PROJECTNAME).o"
	@$(CC) -I $(INCDIR) -c $(SRCDIR)$(PROJECTNAME).cpp -o $(BUILDDIR)$(PROJECTNAME).o
	@echo "  LN     $(BINDIR)$(PROJECTNAME)"
	@$(CC) $(BUILDDIR)$(PROJECTNAME).o $(ALL_OBJS) -o $(BINDIR)$(PROJECTNAME)

tools: runner deduplicate

RR_OBJS = $(BUILDDIR)file_api.o $(BUILDDIR)argparse.o $(BUILDDIR)environment.o \
			$(BUILDDIR)json.o $(BUILDDIR)my_string.o $(BUILDDIR)exec_api.o \
			$(BUILDDIR)date.o $(BUILDDIR)port.o $(BUILDDIR)vm.o $(BUILDDIR)syzkaller.o \
			$(BUILDDIR)fuzz.o $(BUILDDIR)linux.o $(BUILDDIR)shell_api.o $(BUILDDIR)report.o \
			$(BUILDDIR)bisect.o $(BUILDDIR)git.o $(BUILDDIR)version.o $(BUILDDIR)make.o \
			$(BUILDDIR)dedup.o

runner: $(TOOLDIR)runner.cpp $(RR_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)runner.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)runner.cpp -o $(BUILDDIR)runner.o
	@echo "  LN     $(BINDIR)$(PREFIX)runner"
	@$(CC) $(BUILDDIR)runner.o $(RR_OBJS) -o $(BINDIR)$(PREFIX)runner

DD_OBJS = $(BUILDDIR)dedup.o $(BUILDDIR)file_api.o $(BUILDDIR)my_string.o $(BUILDDIR)report.o \
			$(BUILDDIR)exec_api.o

deduplicate: $(TOOLDIR)deduplicate.cpp $(DD_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)deduplicate.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)deduplicate.cpp -o $(BUILDDIR)deduplicate.o
	@echo "  LN     $(BINDIR)$(PREFIX)deduplicate"
	@$(CC) $(BUILDDIR)deduplicate.o $(DD_OBJS) -o $(BINDIR)$(PREFIX)deduplicate

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

syzkaller-clean:
	$(MAKE) -C syzkaller/ clean
