## Click bsdmodule driver Makefile ##
SHELL = /bin/bash

srcdir := .
top_srcdir := ..
top_builddir := ..
subdir := bsdmodule

prefix = /usr/local
exec_prefix = ${prefix}
bindir = /usr/local/bin
sbindir = /usr/local/sbin
libdir = /usr/local/lib
freebsd_includedir = /usr/include
freebsd_srcdir = NONE

VPATH = .:$(top_srcdir)/lib:$(top_srcdir)/$(subdir)

CC = gcc
CPP = gcc -E
CXX = g++
CXXCPP = g++ -E
DEPCFLAGS = -MD -MP
LD = ld
STRIP = strip
PERL = perl
INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644
mkinstalldirs = ../mkinstalldirs

CLICK_BUILDTOOL = $(top_builddir)/bin/click-buildtool

target_cpu = x86_64

ifeq ($(V),1)
ccompile = $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = $(CXXCOMPILE) $(1)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
endif

quiet_cmd_cxxcompile = CXX $(quiet_modtag) $(subst $(obj)/,,$@)
cmd_cxxcompile = $(CXXCOMPILE) -c -o $@ $<

quiet_cmd_ccompile = CC $(quiet_modtag) $(subst $(obj)/,,$@)
cmd_ccompile = $(COMPILE) -c -o $@ $<

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s .ii

.c.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.s.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.S.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.cc.o:
	$(CXXCOMPILE) $(DEPCFLAGS) -c $<
.cc.s:
	$(CXXCOMPILE) -S $<
.cc.ii:
	$(CXXCOMPILE) -E $< > $@


ifndef INSTALLOBJS
INSTALLOBJS = click.ko
endif

GENERIC_OBJS = string.o straccum.o nameinfo.o \
	bitvector.o bighashmap_arena.o hashallocator.o \
	ipaddress.o ipflowid.o etheraddress.o \
	packet.o in_cksum.o \
	error.o timestamp.o glue.o task.o timer.o atomic.o gaprate.o \
	element.o \
	confparse.o args.o variableenv.o lexer.o elemfilter.o routervisitor.o \
	routerthread.o router.o master.o timerset.o handlercall.o notifier.o \
	integers.o crc32.o iptable.o \
	driver.o \
	$(EXTRA_DRIVER_OBJS)

BSDMODULE_OBJS = config.o sched.o module.o module_c.o \
	clickfs.o clickfs_vnops.o clickfs_tree.o clickfs_element.o

EXTRA_DRIVER_OBJS = 

OBJS = $(GENERIC_OBJS) $(ELEMENT_OBJS) $(BSDMODULE_OBJS) elements.o

#CPPFLAGS =  -DCLICK_BSDMODULE -DBSD_NETISRSCHED
CPPFLAGS =  -DCLICK_BSDMODULE

ifeq ($(target_cpu),x86_64)
CFLAGS =  -O2 -Wno-undef -g -fPIC -fno-builtin
CXXFLAGS =  -O2 -fno-exceptions -fno-rtti -fpermissive -Wno-undef -Wno-pointer-arith -g -fPIC -fno-builtin
else
CFLAGS =  -O2 -Wno-undef -g -fno-builtin
CXXFLAGS =  -O2 -fno-exceptions -fno-rtti -fpermissive -Wno-undef -Wno-pointer-arith -g -fno-builtin
endif

DEFS = -DHAVE_CONFIG_H -D_KERNEL
INCLUDES = -nostdinc -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(srcdir) -I$(top_srcdir) -I. -I$(freebsd_includedir)
LDFLAGS = 
LIBS = 

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

FINDELEMFLAGS =  --checksum elements.csmk
-include elements.csmk

ifneq ($(ELEMENT_CHECKSUM),)
ifneq ($(shell $(ELEMENT_CHECKSUMCOMMAND)),$(ELEMENT_CHECKSUM))
$(shell rm -f $(ELEMENTSCONF).conf $(ELEMENTSCONF).csmk)
endif
endif


all: $(INSTALLOBJS)

ifneq ($(MAKECMDGOALS),clean)
-include elements.mk
endif

click.ko: Makefile vnode_if.h vnode_if_newproto.h vnode_if_typedef.h $(OBJS)
	$(LD) -Bshareable -o click.ko $(OBJS)

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

elemlist:
	@/bin/rm -f elements.conf elements.csmk
	@$(MAKE) elements.conf
elements.conf: $(top_builddir)/config.status $(CLICK_BUILDTOOL)
	echo "bsdmodule  analysis app aqm ethernet icmp ip simple standard tcpudp test threads wifi" | $(CLICK_BUILDTOOL) findelem -r bsdmodule -p $(top_srcdir) -X $(srcdir)/elements.exclude $(FINDELEMFLAGS) > elements.conf
elements.mk: elements.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) elem2make < elements.conf > elements.mk
elements.cc: elements.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) elem2export < elements.conf > elements.cc
	@rm -f elements.d

vnode_if.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -h
vnode_if_newproto.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -p
vnode_if_typedef.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -q


DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(INSTALLOBJS)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	for i in $(INSTALLOBJS); do $(INSTALL_DATA) $$i $(DESTDIR)$(libdir)/$$i; done
install-include:
	@:
install-man:
	@:
uninstall:
	for i in $(INSTALLOBJS); do /bin/rm -f $(DESTDIR)$(libdir)/$$i; done

clean:
	-rm -f *.d *.o click.ko elements.mk elements.cc elements.conf elements.csmk \
	vnode_if.h vnode_if_newproto.h vnode_if_typedef.h
distclean: clean
	-rm -f Makefile

.PHONY: all clean distclean elemlist \
	install install-include install-man uninstall
