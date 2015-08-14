CC=clang

FRAMEWORKS:= -framework IOKit -framework CoreFoundation

SOURCE=yubikeylockd.c

CFLAGS=-Wall -Werror -g $(SOURCE)
LDFLAGS=$(LIBRARIES) $(FRAMEWORKS)
OUT=-o yubikeylockd

all: yubikeylockd

clean:
		rm -rf yubikeylockd

yubikeylockd:
		$(CC) $(CFLAGS) $(LDFLAGS) $(OUT)

install:
		install ./yubikeylockd /usr/local/bin/yubikeylockd
		install ./com.podtynnyi.com.yubikeylockd.plist  ~/Library/LaunchAgents/com.podtynnyi.com.yubikeylockd.plist
		launchctl load com.podtynnyi.com.yubikeylockd.plist
