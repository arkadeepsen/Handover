ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

CFLAGS += $(EXTRA_CFLAGS)

OBJS += gatewayNAT.o

LIBS   += -l pthread
LIBS   +=  -lnetfilter_queue -lnfnetlink

ALL=gatewayNAT

all: $(ALL)

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif
ifeq ($(QUIET), 1)
Q=@
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<
	
gatewayNAT: $(OBJS)
	$(Q)$(CC) -o gatewayNAT $(OBJS) $(LIBS)
	@$(E) "  LD " $@
	
clean:
	rm -f core *~ *.o gatewayNAT
	rm -f *.d *.gcno *.gcda *.gcov
	rm -f lcov.info
	rm -rf lcov-html
