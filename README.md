# rustdrv 
rustdrv is a small set of example tests for an exisiting FreeBSD Rust driver. The goal of repo is to create a testing framework for FreeBSD current and future driver devices.

## Tested Driver
The driver that is being tested is: https://github.com/Acesp25/freebsd-kernel-module-rust

## Setup
To setup and run the tests,
1. Clone rustdrv under ```src/tests/rustdrv```
2. Clone the previously mentioned driver and follow the repo's instructions to build it.
3. cd to the installed rustdrv folder
4. Edit the DRIVER_PATH variable to point towards your driver in the Makefile
5. Use the provided Makefile to build and install the tests:
``` sh
make
sudo make install # installs the tests and creates a Kyuafile into /usr/tests/rustdrv
```
6. Run the tests:
 ```sh
sudo kyua test -k /usr/tests/rustdrv/Kyuafile
```

## Tests Included
### Core Tests: core_test.c
1. Load/Unload test
2. Open/Close test
3. Read/Write test
4. Jail test

### Error Tests: error_test.c
1. Null input test
2. Unload while open Test
3. Hot unload test
4. Permission test

### Performace Tests: perf_test.c
1. Stress load test
2. Concurrency test
3. Leakage test
