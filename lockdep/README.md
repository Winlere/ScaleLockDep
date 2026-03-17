# lockdep

This folder contains the actual implementation of lockdep library. It can detect potential and actual deadlocks and provide a depedency or ownership graph.

Based on `dlsym` it hooks the `pthread_mutex` library transparently, so you don't need to modify your current application.

## Components

- `core.c` Core functionalities for deadlock detections.
- `graph.c` DFS graph algorithm.
- `hook.c` Hooked API handlers.
- `lockdep.h` Global header.
- `log.c` Log printing.
- `state.c` Thread and lock states.

## Parameters

All parameters are defined in `lockdep.h`. You may adjust accordingly:

- `LOCKDEP_MAX_LOCK_SLOTS` Deafult 256. Maximum number of locks that the library can track.
- `LOCKDEP_MAX_HELD_LOCK_SLOTS` Default 64. Maximum number of locks per-process that the library can track.
- `LOCKDEP_MAX_THREAD_SLOTS` Defaule 128. Maximum number of threads that the library can track.

## Build

Simply use `make` and will compile to a library file `liblockdep.so`.

## Usage

Use `LD_PRELOAD`.

You can give a flag `LOCKDEP_DEBUG` to turn on debug logs. If not given no debug message will print.

Example:

```bash
LD_PRELOAD=$PWD/liblockdep.so ./test

LOCKDEP_DEBUG=1 LD_PRELOAD=$PWD/liblockdep.so ./test
```
