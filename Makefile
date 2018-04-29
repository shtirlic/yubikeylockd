CC=clang

FRAMEWORKS:= -framework IOKit -framework CoreFoundation

SOURCE=yubikeylockd.c

CFLAGS=-Wall -Werror -O2 $(SOURCE)
LDFLAGS=$(LIBRARIES) $(FRAMEWORKS)
OUT=-o yubikeylockd

all: yubikeylockd

clean:
		rm -rf yubikeylockd

yubikeylockd:
		$(CC) $(CFLAGS) $(LDFLAGS) $(OUT)
