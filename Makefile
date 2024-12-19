CC := $(CXX)

BINDIR := bin/
BUILDDIR := build/
INCDIR := include/
LIBDIR := lib/
SRCDIR := src/
HELPERDIR := helpers/
TOOLDIR := tools/
PROJECTNAME := syzInspector

.DEFAULT_GOAL := $(PROJECTNAME)

.phony: all $(PROJECTNAME) $(BINDIR) helpers tools

all: $(PROJECTNAME) helpers tools

$(BUILDDIR)%.o: $(LIBDIR)%.cpp $(INCDIR)%.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -c $< -o $@

$(BUILDDIR)file_api.o: $(LIBDIR)file_api.cpp $(INCDIR)file_api.h | $(BUILDDIR)
	@echo "  CC     $@"
	@$(CC) -I $(INCDIR) -std=c++17 -lstdc++fs -c $< -o $@

ALL_OBJS = $(BUILDDIR)argparse.o $(BUILDDIR)blocking_bugs.o $(BUILDDIR)bug_info.o \
			$(BUILDDIR)date.o $(BUILDDIR)environment.o $(BUILDDIR)exec_api.o \
			$(BUILDDIR)file_api.o $(BUILDDIR)fuzz_prep.o $(BUILDDIR)fuzz.o $(BUILDDIR)git.o \
			$(BUILDDIR)git_traverse.o $(BUILDDIR)my_string.o $(BUILDDIR)psf.o \
			$(BUILDDIR)result.o $(BUILDDIR)bisect.o $(BUILDDIR)shell_api.o $(BUILDDIR)json.o \
			$(BUILDDIR)syzlang.o $(BUILDDIR)template_parse.o $(BUILDDIR)version.o

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

tools: git_test

GT_OBJS = $(BUILDDIR)git.o $(BUILDDIR)shell_api.o $(BUILDDIR)date.o $(BUILDDIR)file_api.o \
			$(BUILDDIR)exec_api.o $(BUILDDIR)my_string.o $(BUILDDIR)version.o

git_test: $(TOOLDIR)git_test.cpp $(GT_OBJS) | $(BINDIR) $(BUILDDIR)
	@echo "  CC     $(BUILDDIR)git_test.o"
	@$(CC) -I $(INCDIR) -c $(TOOLDIR)git_test.cpp -o $(BUILDDIR)git_test.o
	@echo "  LN     $(TOOLDIR)git_test"
	@$(CC) $(BUILDDIR)git_test.o $(GT_OBJS) -o $(BINDIR)git_test

$(BUILDDIR):
	@echo "DIR    $(BUILDDIR)"
	@mkdir $(BUILDDIR)

$(BINDIR):
	@echo "DIR    $(BINDIR)"
	@mkdir $(BINDIR)

clean:
	$(RM) -r $(BINDIR) $(BUILDDIR)
	$(RM) $(HELPERDIR)diffdate
