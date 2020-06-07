ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

DEPS=
OBJ=ddcmp.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ddcmp: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

install: ddcmp
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 ddcmp $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f ddcmp $(OBJ)
