CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wno-unused-parameter -I./src

GTK_CFLAGS := $(shell pkg-config --cflags gtk4 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs gtk4 2>/dev/null)

ifneq ($(filter cli,$(MAKECMDGOALS)),cli)
ifeq ($(GTK_CFLAGS),)
$(error GTK4 development headers not found. \
  Install libgtk-4-dev (Debian/Ubuntu), gtk4-devel (Fedora), or gtk4 (Arch/MSYS2))
endif
endif

# Library objects (no GTK dependency — pure C library)
LIB_OBJS = src/project.o src/disasm.o src/annotate.o src/memmap.o \
           src/symbols.o src/export.o

# App objects (require GTK)
APP_OBJS = src/main.o src/ui_listing.o src/ui_panels.o src/ui_search.o
CLI_OBJS = src/test_load.o

TARGET = z80bench
CLI_TARGET = z80bench-cli

.PHONY: all clean cli

all: $(TARGET)

cli: $(CLI_TARGET)

$(TARGET): $(LIB_OBJS) $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS)

$(CLI_TARGET): $(LIB_OBJS) $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(APP_OBJS): CFLAGS += $(GTK_CFLAGS)

src/%.o: src/%.c src/z80bench_core.h src/z80bench.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET) $(CLI_TARGET)
