package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"bufio"
	"encoding/binary"
)

// https://wiki.osdev.org/ISO_9660

func must(err error) {
	if err != nil {
		panic(err)
	}
}

func main() {
	b, err := ioutil.ReadFile("TempleOS.ISO")
	must(err)
	for i := 0; i < 32*1024; i++ {
		if b[i] != 0 {
			fmt.Printf("non zero byte at %d\n", i)
			os.Exit(1)
		}
	}
	fmt.Printf("data length: %d\n", len(b))
	readVolumeDescriptors(b)
}

func readVolumeDescriptors(b []byte) {
	off := 32 * 1024
	for {
		volDescr := b[off]
		switch volDescr {
		case 0x00, 0x01, 0x02, 0x03, 0xff:
			// ok
			fmt.Printf("Volume %#x\n", volDescr)
		default:
			fmt.Printf("wrong volume descriptor at %d %#x\n", off, volDescr)
			os.Exit(1)
		}
		off++
		
		if string(b[off:off+5]) != "CD001" {
			fmt.Printf("wrong identifier at %d: %q\n", off, string(b[off:off+5]))
			os.Exit(1)
		}
		off += 5
		
		if b[off] != 0x01 {
			fmt.Printf("wrong version at %d: %d\n", off, b[off])
			os.Exit(1)
		}
		off++
		
		data := &reader{ b[off:off+2041], 0 }
		off += 2041
		
		switch volDescr {
		case 0x00: // boot record
			bootSystemIdentifier := data.strA(32, "Boot System Identifier")
			data.strA(32, "Boot Identifier")
			if bootSystemIdentifier == "EL TORITO SPECIFICATION" {
				bootoff := data.int32_LSB("El Torito boot catalog block number")
				bootoff *= 2048
				fmt.Printf("\tEl Torito boot catalog start offset: %#x\n", bootoff)
				showElToritoBootCatalog(b, int(bootoff))
			} else {
				fmt.Printf("\tContents:\n")
				printCanonical(data.rest(), "\t")
			}
			fmt.Printf("\n")
		case 0x01, 0x02: // primary and supplementary volumes
			data.unused(1)
			data.strA(32, "System Identifier")
			data.strD(32, "Volume Identifier")
			data.unused(8)
			data.int32_LSB_MSB("Volume Space Size")
			data.unused(32)
			data.int16_LSB_MSB("Volume Set Size")
			data.int16_LSB_MSB("Volume Sequence Number")
			data.int16_LSB_MSB("Logical Block Size")
			data.int32_LSB_MSB("Path Table Size")
			data.int32_LSB("Location of Type-L Path Table")
			data.int32_LSB("Location of the Optional Type-L Path Table")
			data.int32_MSB("Location of Type-M Path Table")
			data.int32_MSB("Location of Optional Type-M Path Table")
			fmt.Printf("\tDirectory entry for the root directory:\n")
			printCanonical(data.bytes(34), "\t") //TODO: decode this
			data.strD(128, "Volume Set Identifier")
			data.strA(128, "Publisher Identifier")
			data.strA(128, "Data Preparer Identifier")
			data.strA(128, "Application Identifier")
			data.strD(38, "Copyright File Identifier")
			data.strD(36, "Abstract File Identifier")
			data.strD(37, "Bibliographic File Identifier")
			data.decDateTime(17, "Volume Creation Date and Time")
			data.decDateTime(17, "Volume Modification Date and Time")
			data.decDateTime(17, "Volume Expiration Date and Time")
			data.decDateTime(17, "Volume Effective Date and Time")
			data.int8("File Structure Version")
			data.unused(1)
			app := data.bytes(512)
			fmt.Printf("\tApplication data:\n")
			printCanonical(app, "\t")
			data.unused(653)
			if len(data.rest()) != 0 {
				panic("internal error")
			}
			fmt.Printf("\n")
			
		case 0xff:
			fmt.Printf("\n")
		}
		
		if volDescr == 0xff {
			fmt.Printf("start of data at %d\n", off)
			break
		}
	}
}

type reader struct {
	buf []byte
	off int
}

func (rdr *reader) rest() []byte {
	d := rdr.buf[rdr.off:]
	rdr.off = len(rdr.buf)
	return d
}

func (rdr *reader) strA(n int, lbl string) string {
	d := rdr.buf[rdr.off:rdr.off+n]
	for len(d) > 0 {
		if d[len(d)-1] != 0 {
			break
		}
		d = d[:len(d)-1]
	}
	s := string(d)
	rdr.off += n
	if lbl != "" {
		fmt.Printf("\t%s: %q\n", lbl, s)
	}
	return s
}

func (rdr *reader) strD(n int, lbl string) string {
	return rdr.strA(n, lbl)
}

func (rdr *reader) unused(n int) {
	rdr.off += n
}

func (rdr *reader) bytes(n int) []byte {
	r := rdr.buf[rdr.off:rdr.off+n]
	rdr.off += n
	return r
}

func (rdr *reader) decDateTime(n int, lbl string) {
	year := rdr.strD(4, "")
	month := rdr.strD(2, "")
	day := rdr.strD(2, "")
	hour := rdr.strD(2, "")
	minute := rdr.strD(2, "")
	second := rdr.strD(2, "")
	hundredthsOfSecond := rdr.strD(2, "")
	tz := rdr.int8("")
	if lbl != "" {
		fmt.Printf("\t%s: %s-%s-%s %s:%s:%s.%s %d\n", lbl, year, month, day, hour, minute, second, hundredthsOfSecond, tz)
	}
}

func (rdr *reader) int16_LSB(lbl string) uint16 {
	n := binary.LittleEndian.Uint16(rdr.buf[rdr.off:rdr.off+2])
	rdr.off += 2
	if lbl != "" {
		fmt.Printf("\t%s: %d\n", lbl, n)
	}
	return n
}

func (rdr *reader) int16_MSB(lbl string) uint16 {
	n := binary.BigEndian.Uint16(rdr.buf[rdr.off:rdr.off+2])
	rdr.off += 2
	if lbl != "" {
		fmt.Printf("\t%s: %d\n", lbl, n)
	}
	return n
}

func (rdr *reader) int16_LSB_MSB(lbl string) uint16 {
	soff := rdr.off
	n1 := rdr.int16_LSB("")
	n2 := rdr.int16_MSB("")
	if n1 != n2 {
		panic(fmt.Errorf("LSB/MSB mismatch at %d %d %d", soff, n1, n2))
	}
	return n1
}

func (rdr *reader) int32_LSB(lbl string) uint32 {
	n := binary.LittleEndian.Uint32(rdr.buf[rdr.off:rdr.off+4])
	rdr.off += 4
	if lbl != "" {
		fmt.Printf("\t%s: %d\n", lbl, n)
	}
	return n
}

func (rdr *reader) int32_MSB(lbl string) uint32 {
	n := binary.BigEndian.Uint32(rdr.buf[rdr.off:rdr.off+4])
	rdr.off += 4
	if lbl != "" {
		fmt.Printf("\t%s: %d\n", lbl, n)
	}
	return n
}

func (rdr *reader) int32_LSB_MSB(lbl string) uint32 {
	soff := rdr.off
	n1 := rdr.int32_LSB("")
	n2 := rdr.int32_MSB("")
	if n1 != n2 {
		panic(fmt.Errorf("LSB/MSB mismatch at %d %d %d %x", soff, n1, n2, rdr.buf[soff:soff+16]))
	}
	return n1
}

func (rdr *reader) int8(lbl string) int {
	r := int(rdr.buf[rdr.off])
	rdr.off++
	if lbl != "" {
		fmt.Printf("\t%s: %d\n", lbl, r)
	}
	return r
}

func bunchOfZeroes(v []byte) bool {
	for _, x := range v {
		if x != 0 {
			return false
		}
	}
	return true
}

func printCanonical(data []byte, prefix string) {
	w := bufio.NewWriter(os.Stdout)
	fmt.Fprintf(w, "%s%04x: ", prefix, 0)
	omitting := false
	for i := 0; i < len(data); i++ {
		if i > 0 {
			if i % 16  == 0 {
				if bunchOfZeroes(data[i:i+16]) {
					if !omitting {
						fmt.Fprintf(w, "\n%s*", prefix)
						omitting = true
					}
					i += 15
					continue
				} else {
					fmt.Fprintf(w, "\n%s%04X: ", prefix, i)
					omitting = false
				}
			} else if i % 8 == 0 {
				fmt.Fprintf(w, " ")
			}
		}
		fmt.Fprintf(w, "%02x ", data[i])
	}
	fmt.Fprintf(w, "\n")
	w.Flush()
}

func showElToritoBootCatalog(b []byte, bootoff int) {
	off := bootoff
	first := true
	for {
		data := &reader{ b[off:off+0x20], 0 }
		off += 0x20
		
		headerID := data.int8("")
		
		switch headerID {
		case 0x01:
			fmt.Printf("\tvalidation boot entry\n")
			data.int8("\tPlatform ID")
			data.unused(2)
			data.strA(0x18, "\tID string")
			data.int16_LSB("\tchecksum")
			n1 := data.int8("")
			n2 := data.int8("")
			if n1 != 0x55 || n2 != 0xaa {
				fmt.Printf("\t\tsignature failed\n")
				os.Exit(1)
			}
		case 0x88, 0x00:
			if !first {
				fmt.Printf("\tend of boot catalog at %d\n", off - 0x20)
				return
			}
			fmt.Printf("\tInitial/Default Entry %#x:\n", headerID)
			first = false
			data.int8("\tBoot media type")
			data.int16_LSB("\tLoad segment (0 means 0x7C0)")
			data.int8("\tSystem Type")
			data.unused(1)
			data.int16_LSB("\tSector Count")
			lba := data.int32_LSB("\tLoad RBA") // relative block address?
			fmt.Printf("\t\tLoad byte %#x\n", lba * 2048)
			
			/*
			bootSector := b[lba * 2048 : lba * 2048 + 2048]
			ioutil.WriteFile("bootsector", bootSector, 0666)
			*/
			
		default:
			fmt.Printf("\tunknown boot entry %#x\n", headerID)
			return
		}
	}
}
