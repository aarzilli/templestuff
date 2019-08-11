# Introduction

Lilith is an execution environment for TempleOS programs on Linux. Just like [Wine](https://www.winehq.org) let's you execute Windows programs under Linux, Lilith let's you execute execute TempleOS programs under Linux.

# Usage

`make`, `lilith StartOS.HC` FILL THIS SECTION WHEN ADAM WORKS

# Known Bugs

lots FILL THIS SECTION WHEN ADAM WORKS

# Implementation notes

## TempleOS boot sequence

A TempleOS session is composed by executing three important programs:

1. the Kernel: `/0000Boot/0000Kernel.BIN.C`
2. the Compiler: `/Compiler/Compiler.BIN.Z`
3. Adam: `/Adam`

The Kernel contains the hardware abstractions common to other OS kernels (keyboard&mouse, hard drives, screen, file system) as well as a good chunk of HolyC standard library (MAlloc/Free, Print, StrLen...) and also a compression library. The Kernel is distributed as HolyC source as well as a binary program, hence the `.BIN` extension. I believe `.C` stands for 'cleartext' or something, it means the file is un-compressed (most TempleOS files have the `.Z` extension that means they are compressed and the kernel will uncompress automatically on read and compress them automatically on write).

The Compiler program contains the HolyC compiler. Like the kernel this program is distributed as HolyC source as well as a binary. In this case however the binary is compressed, so the extension is `.BIN.Z`. Since the Compiler is written in HolyC and it is also the only known implementation of HolyC the only way to compile the compiler is to load Compiler.BIN.Z and execute it.

Finally Adam contains an implementation of the GUI environment, the DolDoc file format handling code, a debugger, the 'God' library (which lets you talk to god), a debugger, a command line interpreter, a window manager, etc.

Adam is only distributed in source format.

The boot sequence, extremely simplified, goes like this:

1. the bootloader loads 0000Kernel.BIN.C in memory (the bootloader is the third program, along with kernel and compiler, that is distributed in binary format)
2. the kernel upgrades the processor from 16bit to 32bit and from 32bit to 64bit and finally calls KMain, from this point on everything that happens is driven by code in KMain
3. after some very early initialization LoadKernel is called which relocates the kernel code, before this point a lot of code in the kernel is just broken
4. after some more initialization the kernel loads the compiler by executing the following code:

```
Cd("/Compiler");
Load("Compiler", LDF_SILENT);
```

this loads the compiler in memory and relocates its code (how this and LoadKernel works will be explained in later sections)

5. Finally KMain executes "/StartOS.HC" which uses the compiler to compile and load adam (`"/Adam/MakeAdam"` is the file that gets compiled) and spawns the window manager task
6. Once StartOS is done KMain calls SrvTaskCont which simply loops over the jobs queue (more on how the message queue works later)

At this point the 'Adam' task is completed. Note that there is a difference between 'Adam' the task and 'Adam' the program, 'Adam' the task also contains the kernel and compiler code in addition to Adam-the-program's code.

Whenever I've used the word 'program' so far I've been lying and I could have used 'library' instead. In TempleOS there is no difference between programs and 'libraries' (also called DLLs on windows and shared objects on linux). Every program is a relocatable binary, it can be loaded by any other program and all its exported functions can be called directly.

I will keep using program but remember that anything I say about programs also applies to libraries, because they are the same thing in TempleOS.

## TempleOS Tasks

A task is TempleOS's verison of a process. However, since all tasks share the same memory (remember: there's no memory protection in TempleOS), you could also say that a task is TempleOS's version of a thread. There's no distinction, in TempleOS, between processes and threads.

### TempleOS Tasks: CPU cores and lightweight locking

A task is always associated with a single memory core, TempleOS's scheduler makes no attempt to load-balance workload between cores. Combined with the fact that every task is running on ring0 this means that TempleOS programs have a possibility to implement locking in a very simple way: a task can simply disable interrupts by executing the CLI instruction to insure that no other task assigned to the same core will run (until interrupts are re-enabled).

This trick is used to full effect by the window manager: all graphical programs must run on core 0 and all operations that manipulate window manager related state perform their locking by simply disabling interrupts.

This presents a big problem for lilith: since each task gets mapped to a real linux thread lilith doesn't have any way to disable certain tasks from being scheduled. For most programs this doesn't matter, but a program that changed its draw_it function after having set the DISPLAY_SHOW flag (which signals to the window manager that a window should be opened for the task) will not be thread safe.

### TempleOS Tasks: symbol tables

Each task has a symbol table. The symbol table contains a bunch of different things but most importantly it contains exported functions, whenever a task loads another program in memory the program being loaded can use an 'import' declaration to link to a function exported by the current task, simultaneously every function 'exported' by the loaded program will end up inside the symbol table of the current task.

The symbol table of a task is linked to the symbol table of the task that spawned it. When resolving imported functions TempleOS will look into the symbol table of the parent task if the current task doesn't contain it.

Let's take this simple program:

```
public extern U0 Print(U8 *fmt,...);

"Hello Mr. God!\n";
```

The first line is a function declaration, which lets the compiler know that we are going to use a function called `Print` with a specific signature. The second line is a naked string which will cause the compiler to insert a call to `Print` automatically. Since the `Print` function was declared but not defined the compiler will generate an import statement for it. When the program is loaded, using the `Load` function the kernel will try to resolve the `Print` import statement by looking into the task symbol table. Normally the task symbol table will not contain an entry for `Print` so the kernel will move on to look into the parent's symbol table. Typically, if you are issuing the `Load` command from a TempleOS command line, the parent task will be Adam, since Adam loaded the kernel it *will* contain an entry for `Print` in its symbol table and that's what will be used for it.

See also [TempleOS Binary File Format](#templeos-binary-file-format) for more informations.

### TempleOS Tasks: message queue

TODO

## TempleOS Binary File Format

Let's see what the simple hello world program is compiled to:

```
public extern U0 Print(U8 *fmt,...);

"Hello Mr. God!\n";
```

This will produce a HelloWorld.BIN file that's 96 bytes uncompressed. The binary is composed of a *header*, the program's code and finally a *patch table*.

The header will look like this:


| Offset | Name | Size | Contents | Notes |
| --- | --- | --- | --- | --- |
| 0x00 | jump 			| 2 | 0xeb 0x1e | All TempleOS binaries start with this two bytes, it actually is a jump instruction, specifically `JMP IP+0x1e` which will skip the rest of the |
| 0x02 | module_align_bits	| 1 | 0x00 | This is the file alignment, the file should be loaded at an address that is a multiple of `(1<<module_align_bits)` |
| 0x03 | reserved		| 1 | 0x00 | Not used |
| 0x04 | signature		| 4 | 0x54 0x4f 0x53 0x42 | This is the file signature, it spells 'TOSB' |
| 0x08 | org			| 16 | 0x7FFFFFFFFFFFFFFF | This specifies a fixed address where the file should be loaded, the value 0x7FFFFFFFFFFFFFFF specifies a position independent binary. As far as I can tell the compiler can't produce non-relocatable binaries and it's unclear to what extent this field is actually supported |
| 0x10 | patch_table_offset	| 16 | 0x0000000000000040 | Offset, from the start of the file of the patch table, in this case it's 0x40 bytes in |
| 0x18 | file_size		| 16 | 0x0000000000000060 | Size of the file (in this case 0x60 bytes) |

When a program file is loaded in memory the address of the byte immediately following the header is called the `module_base`.

Following the hader, at offset 0x20, is the programs code:

```
           00000020 6a 00             PUSH         0x0
           00000022 68 11 00 00 00    PUSH         0x11
           00000027 e8 00 00 00 00    CALL         0x00
           0000002c 48 83 c4 10       ADD          RSP,0x10
           00000030 c3                RET
           hello_world_string:
           00000031 48 65 6c 6c...    ds           "Hello Mr. God\n"
```

in this case there's only one function (the main function) so the code can be followed very simply. 
There are two interesting things here:
1. the PUSH instruction at 0x22 is actually trying to set up the arguments for a call to `Print`. The argument, 0x11, is supposed to be the address of the `"Hello Mr. God\n"` string, however when this file will be loaded in memory the string will not be at address 0x11 but at `module_base + 0x11`, so this instruction will have to be patched
2. the CALL instruction at 0x27, the argument of this instruction is simply 0x00 because the function we want to call, `Print`, isn't defined by this program. The patch table will specify that we need a `Print` function and will ask the kernel to fix the instruction at 0x27 with the address that contains `Print`.

Finally, starting at offset 0x40 we have the patch table. The patch table is divided into patch entries and each patch entry starts with a 1 byte type field, followed by a 4 byte argument and a NUL terminated string. After this an arbitrary number of bytes follow, as determined by the type and argument values.

In general the patch table will contain the following types of entries:

1. import entries, specifying that the program needs a specific function and a list of offsets, within the file, that need to be patched with the correct address of the imported function
2. the [relocation table](https://en.wikipedia.org/wiki/Relocation_table).
3. various ways to allocate memory at link time
4. the 'main' entry, that specifies the entry point for the program.
5. the end-of-patch-table entry

This program contains 4 patch table entries, a patch table, the main entry, a single import entry and an end-of-patch-table entry.

The first one is the relocation table

| Offset | Name | Size | Contents | Notes |
| --- | --- | --- | --- | --- |
| 0x40 | entry type		| 1 | 0x14 | indicates that this is a IET_ABS_ADDR entry, which is the relocation table |
| 0x41 | entry argument		| 4 | 0x00000001 | this indicates that only one offset needs to be relocated |
| 0x45 | entry string 		| 1 | 0x00 | this field is unused for relocation table entries |
| 0x46 | list of relocations	| 4 | 0x00000003 | this is a list of 32bit integers, one for each offset to be relocated, as specified by the entry argument |

This entry specifies that the kernel should add the `module_base` of this program to the 32bit integer located at `module_base+0x3`. In practice this patches the argument of the PUSH instruction at 0x22. For example if the coded is loaded at 0x40000 after applying the relocation table it will look like this:

```
           00040020 6a 00             PUSH         0x0
           00040022 68 31 00 04 00    PUSH         0x40031
           00040027 e8 00 00 00 00    CALL         0x00
           0004002c 48 83 c4 10       ADD          RSP,0x10
           00040030 c3                RET
           hello_world_string:
           00040031 48 65 6c 6c...    ds           "Hello Mr. God\n"
```

the argument of PUSH becomes `0x40031 = 0x40020 + 0x11` and now matches the actual address of the string we want to print.

The second entry in the patch table is the main entry:

| Offset | Name | Size | Contents | Notes |
| --- | --- | --- | --- | --- |
| 0x4a | entry type		| 1 | 0x19 | IET_MAIN |
| 0x4b | entry argument		| 4 | 0x00000000 | offset of the main function from module_base |
| 0x4f | entry string 		| 1 | 0x00 | unused |

A IET_MAIN entry doesn't actually contain a body.

The third entry is the import entry:

| Offset | Name | Size | Contents | Notes |
| --- | --- | --- | --- | --- |
| 0x50 | entry type		| 1 | 0x8 | IET_REL_I32 |
| 0x51 | entry argument		| 4 | 0x00000008 | offset from module_base of the address to patch |
| 0x55 | entry string 		| 6 | 'Print\0' | name of the imported function |

This requests the kernel to find a function called `Print` (by looking through the task symbol table) and place its address at address `module_base+0x8`, which happens to be where the argument of the CALL instruction is stored.

The final entry is the end-of-patch-table entry which is simply a single 0x00, this entry doesn't have a numerical argument or a string argument.

