CC     = clang
CFLAGS = -Wall -Wextra -I src
LDFLAGS = -lSDL2 -lm

BUILDDIR = build
SRC      = src/main.c
OBJ      = $(BUILDDIR)/main.o
TARGET   = screenPJ

all: $(BUILDDIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

.PHONY: all clean
