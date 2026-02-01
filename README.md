# xv6-kernel-labs

Deep kernel modifications to the xv6 teaching operating system, implementing core OS concepts including system calls, process scheduling, and memory management.

## Overview

This repository contains three major kernel extensions to xv6:

1. **System Call Implementation** - Custom `getlastcat` syscall for tracking file operations
2. **MLFQ Scheduler** - Multi-Level Feedback Queue scheduler with priority decay
3. **Memory Mapping** - Full `mmap`/`munmap` implementation with lazy allocation

## Features

### System Call: `getlastcat`
- Tracks the most recent filename passed to the `cat` command
- Demonstrates understanding of xv6 syscall mechanism
- Handles edge cases: no args, invalid files, multiple files

**Modified files:** `syscall.h`, `syscall.c`, `sysfile.c`, `user.h`, `usys.S`

### MLFQ Scheduler
- Priority-based scheduling with CPU decay
- Recalculates priorities every 100 ticks using decay formula: `cpu = cpu/2`, `priority = cpu/2 + nice`
- Implements `nice()` syscall to adjust process priority (0-20)
- Implements `getschedstate()` to retrieve scheduler information
- Round-robin among equal-priority processes

**Modified files:** `proc.h`, `proc.c`, `sysproc.c`, `psched.h`

### Memory Mapping (`mmap`/`munmap`)
- Anonymous and file-backed memory mappings
- Lazy allocation via page fault handler
- Support for:
  - `MAP_PRIVATE` / `MAP_SHARED`
  - `MAP_FIXED` / `MAP_ANONYMOUS`
  - `MAP_GROWSUP` (guard page extension)
- Proper handling in `fork()` (mapping inheritance) and `exit()` (cleanup)
- Write-back for shared file-backed mappings on unmap

**Modified files:** `mmap.h`, `mmap.c`, `proc.h`, `proc.c`, `trap.c`, `vm.c`, `memlayout.h`

## Building

```bash
make clean
make qemu-nox
```

Exit xv6 with `Ctrl-a x`

## Project Structure

```
├── mmap.h          # mmap flag definitions
├── mmap.c          # mmap/munmap implementation
├── psched.h        # scheduler info structure
├── proc.h          # process structure with scheduler and mmap fields
├── proc.c          # MLFQ scheduler, fork/exit with mmap support
├── trap.c          # page fault handler for lazy allocation
├── syscall.c       # syscall dispatch table
├── sysfile.c       # getlastcat and mmap syscall wrappers
├── sysproc.c       # nice, getschedstate, sleep modifications
└── ...             # other xv6 kernel files
```

## Technical Highlights

- **Kernel-space programming** in C with x86 assembly
- **Virtual memory management** with page tables and lazy allocation
- **Process scheduling** with priority decay algorithms
- **System call interface** between user and kernel space
- **Interrupt handling** via page fault traps

## References

- [xv6 Book](https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf)
- Based on MIT's 6.828 xv6 operating system
