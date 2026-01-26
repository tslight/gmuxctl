CC	= cc
CFLAGS  = -O2 -pedantic -std=c11
CFLAGS += -Wall -Wconversion -Wextra -Wshadow -Wunused
CFLAGS += -Wmissing-prototypes -Wstrict-prototypes
CFLAGS += -Wuninitialized -Wimplicit-fallthrough

PREFIX	= /usr
BINDIR	= $(DESTDIR)$(PREFIX)/bin

INSTALL_PROGRAM = install -s

PROG	= backlight
OBJS	= backlight.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) -o $@

$(OBJS): *.o: *.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL_PROGRAM) $(PROG) $(BINDIR)

clean:
	rm -f $(PROG) $(OBJS)

release: all
	@mkdir $(PROG)-${VERS}
	@cp Makefile *.c $(PROG)-$(VERS)/
	@tar -czf ../$(PROG)-$(VERS).tar.gz $(PROG)-$(VERS)
	@rm -rf $(PROG)-$(VERS)/
	@echo "made release ${VERS}"

.PHONY: all install clean
