CC=clang

FRAMEWORKS:= -framework IOKit -framework CoreFoundation

SOURCE=yubikeylockd.c

CFLAGS=-Wall -Werror -O2 $(SOURCE)
LDFLAGS=$(LIBRARIES) $(FRAMEWORKS)
OUT=-o yubikeylockd

all: yubikeylockd install

clean:
		rm -rf yubikeylockd

yubikeylockd:
		$(CC) $(CFLAGS) $(LDFLAGS) $(OUT)

install:
		install ./yubikeylockd /usr/local/bin/yubikeylockd
		launchctl unload com.podtynnyi.com.yubikeylockd.plist
		install ./com.podtynnyi.com.yubikeylockd.plist  ~/Library/LaunchAgents/com.podtynnyi.com.yubikeylockd.plist
		launchctl load com.podtynnyi.com.yubikeylockd.plist
