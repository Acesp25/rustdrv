# rustdrv 
rustdrv is a repo that is mirroring the src/tests/rustdrv changes from my freebsd fork. The goal of this is project is to create a testing framework for FreeBSD current and future driver devices. More to add about how to make it work

## Tested Driver
The driver that is being tested is: https://github.com/Acesp25/freebsd-kernel-module-rust
I need to add dynamic pathing to my tests so right now it wont work out of the box.

## Setup
To setup the tests in your freebsd testing suite,
1. Install rustdrv under ```freebsd src/tests/rustdrv```
2. cd to the installed rustdrv folder
3. Use the provided Makefile to build and install the tests:
``` sh
make
sudo make install # installs the tests and creates a Kyuafile into /src/tests/rustdrv
```
4. Run the tests with: ```sudo kyua test -k /usr/tests/rustdrv/Kyuafile```

## Comments
WIP
