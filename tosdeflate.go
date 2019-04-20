package main

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"os"

	"github.com/aarzilli/templestuff/tosz"
)

import "C"

func must(err error) {
	if err != nil {
		panic(err)
	}
}

const debug = false

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintf(os.Stderr, "wrong number of arguments\n")
		fmt.Fprintf(os.Stderr, "%s <file.Z>", os.Args[0])
		os.Exit(1)
	}
	b, err := ioutil.ReadFile(os.Args[1])
	must(err)
	if debug {
		fmt.Printf("on disk size: %#x\n", len(b))
	}
	exp := tosz.ExpandFile(b)
	io.Copy(os.Stdout, bytes.NewReader(exp))
}
