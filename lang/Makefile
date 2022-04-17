CC=gcc
CHK=cppcheck
SAN=valgrind

SOURCES=main.c
TARGET=main.out

CHKFLAGS=--std=c89
CFLAGS=-Wall -Wextra -pedantic -std=c89 -O1 -g
LFLAGS=-g
SANFLAGS=

COMP=$(CC) $(CFLAGS) -o $@ -c $<
LINK=$(CC) $(LFLAGS) -o $@ $^
CHECK=$(CHK) $(CHKFLAGS) $<
SANATIZE=$(SAN) $(SANFLAGS) ./$@

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o

%.o: %.c Makefile
	$(CHECK)
	$(COMP)

$(TARGET): $(SOURCES:.c=.o)
	$(LINK)
	$(SANATIZE)

.PHONY: all clean
