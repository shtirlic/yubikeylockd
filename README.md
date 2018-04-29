# yubikeylockd

Simple daemon for locking and unlocking macOS with Yubikey.

## Install

Via Homebrew formula:

```
brew install https://raw.githubusercontent.com/shtirlic/yubikeylockd/master/yubikeylockd.rb
```

## Additional requirements
  * [YubiKey using the native smart card (PIV) mode](https://www.yubico.com/why-yubico/for-businesses/computer-login/mac-os-login/)
  * Require password *immediately* after sleep or screen saver begins
  ![](http://i.imgur.com/URXUukP.png)

## How it works

When you attach Yubikey for the first time `launchctl` will run `yubikeylockd` daemon
that will simply monitor the state of the Yubikey USB devices.
Daemon based on the sample provided by Apple for IOKit development.

It does two things:
* when device is attached it makes activity via
```IOPMAssertionDeclareUserActivity``` call to turn screen on
* after device is detached it uses ```IORequestIdle``` to put display to sleep and (if you configured it) also locks the OS X
