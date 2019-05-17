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

func staticFile(w io.Writer, path, name string) {
	buf, err := ioutil.ReadFile(path)
	must(err)

	fmt.Fprintf(w, "#define %s_SIZE %#x\n", strings.ToUpper(name), len(buf))
	fmt.Fprintf(w, "uint8_t %s[] = {\n\t", name)

	for i := range buf {
		fmt.Fprintf(w, "%#02x, ", buf[i])

		if i%16 == 0 && i != 0 {
			fmt.Fprintf(w, "\n\t")
		}
	}

	fmt.Fprintf(w, "\n};\n")

}

func main() {
	fh, err := os.Create("static.c")
	must(err)

	w := bufio.NewWriter(fh)

	staticFile(w, "static/0000Kernel.BIN.C", "kernel_bin_c")
	staticFile(w, "static/Compiler.BIN.Z", "compiler_bin_z")

	must(w.Flush())
	must(fh.Close())
}
