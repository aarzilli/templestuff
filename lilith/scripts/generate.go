package main

import (
	"bufio"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"strings"
)

func must(err error) {
	if err != nil {
		panic(err)
	}
}

type staticFileEntry struct {
	name string
	p string
	sz int
}

var staticFiles = []staticFileEntry{}

func staticFile(w io.Writer, path, name string) {
	buf, err := ioutil.ReadFile(path)
	must(err)
	
	fmt.Fprintf(w, "uint8_t %s[] = {\n\t", name)
	for i := range buf {
		if i%16 == 0 && i != 0 {
			fmt.Fprintf(w, "\n\t")
		}
		
		fmt.Fprintf(w, "%#02x, ", buf[i])
	}
	fmt.Fprintf(w, "0x00,\n};\n")
	
	p := path[strings.Index(path, "/")+1:]
	
	staticFiles = append(staticFiles, staticFileEntry{ name, p, len(buf) })
}

func staticFileTable(w io.Writer) {
	fmt.Fprintf(w, "struct builtin_file builtin_files[] = {\n")
	for _, sf := range staticFiles {
		fmt.Fprintf(w, "\t{ %q, %d, %s},\n", sf.p, sf.sz+1, sf.name)
	}
	fmt.Fprintf(w, "};\n")
	fmt.Fprintf(w, "#define NUM_BUILTIN_FILES %d\n", len(staticFiles))
	fmt.Fprintf(w, "#define KERNEL_BIN_C_SIZE %d\n", staticFiles[0].sz)
}

func main() {
	fh, err := os.Create("static.c")
	must(err)

	w := bufio.NewWriter(fh)
	
	staticFile(w, "static/0000Kernel.BIN.C", "kernel_bin_c")
	staticFile(w, "static/Compiler.BIN.Z", "compiler_bin_z")
	staticFile(w, "static/OpCodes.DD.Z", "opcodes_dd_z")
	staticFile(w, "static/Lilith1.HC", "lilith_1_hc")
	
	staticFileTable(w)

	must(w.Flush())
	must(fh.Close())
}
