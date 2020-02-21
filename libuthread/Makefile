lib := libuthread.a
objs := sem.o tps.o
preobjs := thread.o queue.o

CC := gcc
CFLAGS := -Wall -Werror -Wextra -pipe -pthread
ifneq ($(D),1)
CFLAGS += -O2
else
CFLAGS += -g
endif

ARCHIVE := ar rcs

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o, %.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

$(lib): $(objs) $(preobjs)
	@echo "Archive	$@"
	$(Q)$(ARCHIVE) $@ $^

%.o: %.c
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	@echo "CLEAN"
	$(Q)rm -f $(lib) $(objs) $(deps)
