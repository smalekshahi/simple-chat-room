CC := /usr/bin/gcc
CFLAGS := -fPIC -g
HEADERS :=  -I/usr/local/include -I./
LDFLAGS := -shared
LIBS := -L/usr/local/lib
SRCDIR := .
OBJDIR := .
BINDIR := .
OBJS = client.o server.o common.o scr.o protocol.o
TARGET := $(BINDIR)/scr
TARGET_LIB = libscr.so

all: $(TARGET) $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} $(CFLAGS) $(HEADERS) $(LIBS) -o $@ $^

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(HEADERS) $(LIBS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(HEADERS) -o $@ -c $<

.PHONY: clean
clean: 
	rm $(BINDIR)/scr $(OBJDIR)/*.o  libscr.so
