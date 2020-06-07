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
	install -d $(DESTDIR)$(PREFIX)/lib/
	install -m 644 ddcmp $(DESTDIR)$(PREFIX)/lib/

clean:
	rm -f ddcmp $(OBJ)
