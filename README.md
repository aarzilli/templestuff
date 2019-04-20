# Contents
- TempleOS.ISO: TempleOS ISO image as of april 2019
- iso.go: utility to view the headers of a ISO9660 file
- eltorito.pdf: El-Torito specification
- bootsector: TempleOS ISO extracted boot sector
- 0000Kernel.BIN.C: Kernel binary extracted from TempleOS.ISO
- bootsector.gpr/bootsector.rep: Ghidra (9.0.2) project disassembling bootsector and the early stages of 0000Kernel.BIN.C (only up to the 64bit switch)
- Compress.HC.Z: Kernel/Compress.HC.Z extracted from TempleOS.ISO
- tosdeflate.go: utility to decompress TempleOS '.Z' files. The decompression routines are copied from GIMP and therefore are probably GPL licensed (although they copied it from TempleOS, which is Public Domain and Terry copied it from a magazine which is... ?)

