CFLAGS=-I../rofi/include/ `pkg-config --cflags pango cairo glib-2.0 gmodule-export-2.0`
CFLAGS+=-fPIC
OUTPUT=libblezz.so

${OUTPUT}: blezz.o
	$(LINK.cc) -shared $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm ${OUTPUT} blezz.o
