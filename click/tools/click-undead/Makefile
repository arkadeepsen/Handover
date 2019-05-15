SHELL = /bin/bash


top_srcdir = ../..
srcdir = .
top_builddir = ../..
subdir = tools/click-undead
conf_auxdir = $(top_srcdir)

prefix = /usr/local
bindir = /usr/local/bin
HOST_TOOLS = host

VPATH = .:$(top_srcdir)/$(subdir):$(top_srcdir)/tools/lib:$(top_srcdir)/include

ifeq ($(HOST_TOOLS),build)
CC = 
CXX = 
LIBCLICKTOOL = libclicktool_build.a
DL_LIBS = 
DL_LDFLAGS = 
else
CC = gcc
CXX = g++
LIBCLICKTOOL = libclicktool.a
DL_LIBS = -ldl
DL_LDFLAGS = -rdynamic
endif
INSTALL = /usr/bin/install -c
mkinstalldirs = $(conf_auxdir)/mkinstalldirs

ifeq ($(V),1)
ccompile = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(1)
cxxlink = $(CXXLINK) $(1)
x_verbose_cmd = $(1) $(3)
verbose_cmd = $(1) $(3)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
cxxlink = @/bin/echo ' ' $(2) $@ && $(CXXLINK) $(1)
x_verbose_cmd = $(if $(2),/bin/echo ' ' $(2) $(3) &&,) $(1) $(3)
verbose_cmd = @$(x_verbose_cmd)
endif

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s

.c.o:
	$(call ccompile,-c $< -o $@,CC)
.s.o:
	$(call ccompile,-c $< -o $@,ASM)
.S.o:
	$(call ccompile,-c $< -o $@,ASM)
.cc.o:
	$(call cxxcompile,-c $< -o $@,CXX)


OBJS = click-undead.o

CPPFLAGS =  -DCLICK_TOOL
CFLAGS = -g -O2 -W -Wall
CXXFLAGS = -g -O2 -W -Wall
DEPCFLAGS = -MD -MP

DEFS = -DHAVE_CONFIG_H
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(top_srcdir)/tools/lib -I$(srcdir)
LDFLAGS = 
LIBS =   $(DL_LIBS)

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

all: $(LIBCLICKTOOL) all-local
all-local: click-undead

$(LIBCLICKTOOL):
	@cd ../lib; $(MAKE) $(LIBCLICKTOOL)

click-undead: Makefile $(OBJS) ../lib/$(LIBCLICKTOOL)
	$(call cxxlink,$(DL_LDFLAGS) $(OBJS) ../lib/$(LIBCLICKTOOL) $(LIBS),LINK)
	@-mkdir -p ../../bin; ln -sf ../tools/click-undead/$@ ../../bin/$@

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(LIBCLICKTOOL) install-local
install-local: all-local
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(bindir))
	$(call verbose_cmd,$(INSTALL) click-undead,INSTALL,$(DESTDIR)$(bindir)/click-undead)
uninstall:
	/bin/rm -f $(DESTDIR)$(bindir)/click-undead

clean:
	rm -f *.d *.o click-undead ../../bin/click-undead
distclean: clean
	-rm -f Makefile

.PHONY: all all-local clean distclean \
	install install-local uninstall $(LIBCLICKTOOL)
