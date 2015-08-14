# yubikeylockd

Simple daemon for locking and unlocking OS X with Yubikey.


## Install

Run make and make install


## Additional requirements
  * Configured integration with Yubico PAM module
  * Require password *immediately* after sleep or screen saver begins
  ![](https://leto34g.storage.yandex.net/rdisk/81b6a915275c651eb4cf5ab517dc954da80256e081f3492e8c0eee9748fc28bc/inf/L79veo8a6pKruGrqf6TRQXGuPF70A_-59NpZO1XQMKNkJTPWMeuX72_8ZEG4cLBUnlvYO-F3HB6X30BxPBd9cw==?uid=0&filename=2015-08-14%2013-29-11%20Security%20%26%20Privacy.png&disposition=inline&hash=&limit=0&content_type=image%2Fpng&tknv=v2&rtoken=876553fcd08c3e4d9acc94b407c85688&force_default=no&ycrid=na-483f731601d2e7290b5708ed5bae18e2-downloader9g)

## How it works

When you attach Yubikey for the first time launchctl will run yubikeylockd daemon
that will simply monitor the state of the given USB device.
Daemon based on the sample provided by Apple for IOKit development.

It does two things: 
* when device is attached it makes activity via
```IOPMAssertionDeclareUserActivity``` call to turn screen on
* after device is detached it runs shell command ```pmset displaysleepnow```
 to put display to sleep and (if you configured it) also lock the OS X
