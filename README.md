# rustdrv 
rustdrv is a repo that is mirroring the src/tests/rustdrv changes from my freebsd fork. The goal of this is project is to create a testing framework for FreeBSD current and future driver devices. More to add about how to make it work

## Tested Driver
The driver that is being tested is: https://github.com/Acesp25/freebsd-kernel-module-rust
I need to add dynamic pathing to my tests so right now it wont work out of the box.

## Setup
To setup and run the tests,
1. Clone rustdrv under ```freebsd src/tests/rustdrv```
2. Clone the previously mentioned driver and follow the repo's instructions to build it.
3. Edit the #define paths in the .c test files to match your enviroment. You should only need to edit DRIVER_PATH and MODULE_PATH
4. cd to the installed rustdrv folder
5. Use the provided Makefile to build and install the tests:
``` sh
make
sudo make install # installs the tests and creates a Kyuafile into /usr/tests/rustdrv
```
4. Run the tests:
 ```sh
sudo kyua test -k /usr/tests/rustdrv/Kyuafile
```

## Comments
You need to have built the kld from the provided repo above and adjuted the paths in the .c files accordingly.
WIP
