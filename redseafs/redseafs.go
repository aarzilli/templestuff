package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/aarzilli/templestuff/tosz"
	"rsc.io/rsc/fuse"
)

func must(err error, reason string) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "error %s: %v\n", reason, err)
		os.Exit(1)
	}
}

var FH *os.File
var Mu sync.Mutex

const debug = true

const redSeaBootOffset = 0xb000

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintf(os.Stderr, "wrong number of arguments\n")
		fmt.Fprintf(os.Stderr, "%s <TempleOS.ISO> <mountpoint>\n", os.Args[0])
		os.Exit(1)
	}

	var err error
	FH, err = os.Open(os.Args[1])
	must(err, "opening ISO file")

	_, err = FH.Seek(redSeaBootOffset, io.SeekStart)
	must(err, "seeking boot offset")
	rootDirOffset := readRedSeaBoot(FH)

	_, err = FH.Seek(rootDirOffset, io.SeekStart)
	must(err, "seeking to root directory")
	rootDir := readDirectoryEntry(FH)
	if debug {
		fmt.Printf("root: %#v\n", rootDir)
	}

	c, err := fuse.Mount(os.Args[2])
	must(err, "mounting file system")

	c.Serve(FS{rootDir})
}

const _RS_BLK_SIZE = 512

func readRedSeaBoot(fh io.ReadSeeker) int64 {
	/*
	   U8 jump_and_nop[3];
	   U8 signature,reserved[4]; //MBR_PT_REDSEA=0x88. Distinguish from real FAT32.
	   I64 drv_offset;	//For CD/DVD image copy.
	   I64 sects;
	   I64 root_clus; (root_blk)
	   I64 bitmap_sects;
	   I64 unique_id;
	   U8 code[462];
	   U16 signature2;	//0xAA55
	*/
	buf := make([]byte, _RS_BLK_SIZE)

	_, err := io.ReadFull(fh, buf)
	must(err, "reading RedSea boot sector")

	if sig := buf[3 : 3+4]; !bytes.Equal(sig, []byte{0x88, 0x00, 0x00, 0x00}) {
		fmt.Fprintf(os.Stderr, "signature check on RedSea boot sector failed %#x\n", sig)
		os.Exit(1)
	}

	if sig2 := buf[_RS_BLK_SIZE-2 : _RS_BLK_SIZE]; !bytes.Equal(sig2, []byte{0x55, 0xaa}) {
		fmt.Fprintf(os.Stderr, "signature 2 check on RedSea boot sector failed %#x\n", sig2)
		os.Exit(1)
	}

	rootClus := binary.LittleEndian.Uint64(buf[0x18 : 0x18+8])
	if debug {
		fmt.Printf("rootClus %#x root offset: %#x\n", rootClus, rootClus*_RS_BLK_SIZE)
	}
	return int64(rootClus * _RS_BLK_SIZE)
}

type rsAttr uint16

const (
	_RS_ATTR_READ_ONLY      rsAttr = 0x01 //R
	_RS_ATTR_HIDDEN         rsAttr = 0x02 //H
	_RS_ATTR_SYSTEM         rsAttr = 0x04 //S
	_RS_ATTR_VOL_ID         rsAttr = 0x08 //V
	_RS_ATTR_DIR            rsAttr = 0x10 //D
	_RS_ATTR_ARCHIVE        rsAttr = 0x20 //A
	_RS_ATTR_LONG_NAME      rsAttr = _RS_ATTR_READ_ONLY | _RS_ATTR_HIDDEN | _RS_ATTR_SYSTEM | _RS_ATTR_VOL_ID
	_RS_ATTR_LONG_NAME_MASK rsAttr = _RS_ATTR_LONG_NAME | _RS_ATTR_DIR | _RS_ATTR_ARCHIVE
	_RS_ATTR_DELETED        rsAttr = 0x100  //X
	_RS_ATTR_RESIDENT       rsAttr = 0x200  //T
	_RS_ATTR_COMPRESSED     rsAttr = 0x400  //Z
	_RS_ATTR_CONTIGUOUS     rsAttr = 0x800  //C
	_RS_ATTR_FIXED          rsAttr = 0x1000 //F
)

type dirEntry struct {
	attr rsAttr
	name string
	clus int64

	diskSize, viewSize int64

	time       uint32
	date       int32 // number of days since the birth of Christ
	dirChilds  []*dirEntry
	autoExpand bool
}

const _RS_DIR_ENTRY_SIZE = 64

func readDirectoryEntry(fh io.ReadSeeker) *dirEntry {
	/*
	   U16 attr;	//See $LK,"RS_ATTR_DIR",A="MN:RS_ATTR_DIR"$.  I would like to change these.
	   U8 name[CDIR_FILENAME_LEN]; //See $LK,"char_bmp_filename",A="MN:char_bmp_filename"$, $LK,"FileNameChk",A="MN:FileNameChk"$
	   I64 clus; (blk) //One sector per clus.
	   I64 size;	//In bytes
	   CDate datetime; //See  $LK,"DateTime",A="::/Doc/TimeDate.DD"$, $LK,"Implementation of DateTime",A="FI:::/Kernel/KDate.HC"$
	*/

	buf := make([]byte, _RS_DIR_ENTRY_SIZE)
	_, err := io.ReadFull(fh, buf)
	if err != nil {
		return nil
	}
	var r dirEntry
	r.attr = rsAttr(binary.LittleEndian.Uint16(buf[:2]))
	d := buf[2 : 2+38]
	for len(d) > 0 {
		if d[len(d)-1] != 0 {
			break
		}
		d = d[:len(d)-1]
	}
	r.name = string(d)
	r.clus = int64(binary.LittleEndian.Uint64(buf[2+38 : 2+38+8]))
	r.diskSize = int64(binary.LittleEndian.Uint64(buf[2+38+8 : 2+38+16]))
	r.viewSize = r.diskSize
	r.time = binary.LittleEndian.Uint32(buf[2+38+16 : 2+38+20])
	r.date = int32(binary.LittleEndian.Uint32(buf[2+38+20 : 2+38+24]))
	return &r
}

func readExpandedSizeLocked(clus int64, buf []byte) int64 {
	_, err := FH.Seek(clus*_RS_BLK_SIZE, io.SeekStart)
	if err != nil {
		log.Printf("error seeking to clus %#x: %v", clus, err)
		return 0
	}

	_, err = io.ReadFull(FH, buf)
	if err != nil {
		log.Printf("error reading at clus %#x: %v", clus, err)
		return 0
	}

	return int64(binary.LittleEndian.Uint64(buf[8:]))
}

type FS struct {
	rootDir *dirEntry
}

func (fs FS) Root() (fuse.Node, fuse.Error) {
	return fs.rootDir, nil
}

func (e *dirEntry) Attr() fuse.Attr {
	mode := os.FileMode(0444)
	if e.attr&_RS_ATTR_DIR != 0 {
		mode = os.ModeDir | 0555
	}
	year := e.date / 365
	rest := e.date - year
	t := time.Date(int(year), 0, 0, 0, 0, 0, 0, time.Local) //TODO: is this wrong?
	t.Add(time.Duration(int64(rest*60*60*24)+int64(e.time)) * time.Second)
	sz := uint64(e.viewSize)
	return fuse.Attr{
		Mode:   mode,
		Size:   sz,
		Blocks: sz / 512,
		Atime:  t,
		Mtime:  t,
		Ctime:  t,
		Crtime: t,
	}
}

func (e *dirEntry) Lookup(name string, intr fuse.Intr) (fuse.Node, fuse.Error) {
	dirChilds := e.getDirChilds()

	for _, child := range dirChilds {
		if child.name == name {
			return child, nil
		}
	}
	return nil, fuse.ENOENT
}

func (e *dirEntry) getDirChilds() []*dirEntry {
	if e.attr&_RS_ATTR_DIR == 0 {
		return nil
	}

	Mu.Lock()
	defer Mu.Unlock()

	if e.dirChilds != nil {
		return e.dirChilds
	}

	_, err := FH.Seek(e.clus*_RS_BLK_SIZE, io.SeekStart)
	if err != nil {
		log.Printf("error seeking to clus %#x: %v", e.clus, err)
		return nil
	}

	buf := make([]byte, e.diskSize)
	_, err = io.ReadFull(FH, buf)
	if err != nil {
		log.Printf("error reading directory at clus %#x: %v", e.clus, err)
		return nil
	}

	buf2 := make([]byte, 16)

	rdr := bytes.NewReader(buf)

	for {
		if rdr.Len() == 0 {
			break
		}
		child := readDirectoryEntry(rdr)
		if child == nil || child.name == "" {
			break
		}
		if child.clus == e.clus || (child.attr&_RS_ATTR_DELETED != 0) {
			continue
		}
		e.dirChilds = append(e.dirChilds, child)
		if (child.attr&_RS_ATTR_COMPRESSED != 0) && strings.HasSuffix(child.name, ".Z") {
			echild := &dirEntry{}
			*echild = *child
			echild.name = echild.name[:len(echild.name)-2]
			echild.viewSize = readExpandedSizeLocked(child.clus, buf2)
			echild.autoExpand = true
			e.dirChilds = append(e.dirChilds, echild)
		}

	}

	return e.dirChilds
}

func (e *dirEntry) ReadDir(intr fuse.Intr) ([]fuse.Dirent, fuse.Error) {
	dirChilds := e.getDirChilds()

	r := make([]fuse.Dirent, 0, len(dirChilds))

	for _, child := range dirChilds {
		fdirent := fuse.Dirent{
			Inode: uint64(child.clus),
			Type:  0,
			Name:  child.name,
		}
		if child.autoExpand {
			fdirent.Inode = uint64(-child.clus)
		}
		r = append(r, fdirent)
	}

	return r, nil
}

func (e *dirEntry) ReadAll(intr fuse.Intr) ([]byte, fuse.Error) {
	Mu.Lock()
	defer Mu.Unlock()

	_, err := FH.Seek(e.clus*_RS_BLK_SIZE, io.SeekStart)
	if err != nil {
		log.Printf("error seeking to clus %#x: %v", e.clus, err)
		return nil, fuse.EIO
	}

	b := make([]byte, e.diskSize)
	_, err = io.ReadFull(FH, b)
	if err != nil {
		log.Printf("error reading file %#v: %v", e, err)
		return nil, fuse.EIO
	}

	if e.attr&_RS_ATTR_COMPRESSED != 0 && e.autoExpand {
		if len(b) <= 17 {
			log.Printf("trying to decompress zero-length file %#v %d\n", e, len(b))
			return b, nil
		}
		exp := tosz.ExpandFile(b)
		return exp, nil
	}

	return b, nil
}
