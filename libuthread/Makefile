lib := libuthread.a
objs := sem.o
preobjs := thread.o queue.o

CC := gcc
CFLAGS := -Wall -Werror -Wextra
CFLAGS += -g
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
