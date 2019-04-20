package tosz

import (
	"encoding/binary"
	"fmt"
)

/*
#include <stdlib.h>
#include <string.h>

#pragma pack(1)

#define TRUE	1
#define FALSE	0

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD; typedef unsigned char BOOL;

#define ARC_MAX_BITS 12

#define MAX_INT 0xFFFFFFFFl

#define CT_NONE 	1
#define CT_7_BIT	2
#define CT_8_BIT	3

typedef struct _CArcEntry
{
    struct _CArcEntry *next;
    WORD basecode;
    BYTE ch,pad;
} CArcEntry;

typedef struct _CArcCtrl //control structure
{
    DWORD src_pos,src_size,
          dst_pos,dst_size;
    BYTE *src_buf,*dst_buf;
    DWORD min_bits,min_table_entry;
    CArcEntry *cur_entry,*next_entry;
    DWORD cur_bits_in_use,next_bits_in_use;
    BYTE *stk_ptr,*stk_base;
    DWORD free_index,free_limit,
          saved_basecode,
          entry_used,
          last_ch;
    CArcEntry compress[1<<ARC_MAX_BITS],
              *hash[1<<ARC_MAX_BITS];
} CArcCtrl;

// Returns the bit within bit_field at bit_num (assuming it's stored as little-endian). Whole bunch of finicky stuff because of bytes
int Bt(int bit_num, BYTE *bit_field)
{
    bit_field+=bit_num>>3; // bit_field now points to the appropriate byte in src byte-array
    bit_num&=7; // Get the last 3 bits of bit_num ( 7 is 111 in binary). Basically bit_num % 8. Signifies which bit in the byte now specified by bit_num we are looking at.
    return (*bit_field & (1<<bit_num)) ? 1:0;
}

int Bts(int bit_num, BYTE *bit_field)
{
    int result;
    bit_field+=bit_num>>3; //Get the relevant byte (now in the dest bitfield)
    bit_num&=7; // Get the right bit in that byte

    result=*bit_field & (1<<bit_num); // Only used for return value

    *bit_field|=(1<<bit_num); //Make a byte with the relevant bit switched on and OR it on the relevant byte
    return (result) ? 1:0;
}

DWORD BFieldExtU32(BYTE *src,DWORD pos,DWORD bits)
{
    DWORD i,result=0;
    for (i=0;i<bits;i++)
        if (Bt(pos+i,src))
            Bts(i,(BYTE *)&result);
    return result;
}

void ArcEntryGet(CArcCtrl *c)
{
    DWORD i;
    CArcEntry *temp,*temp1;

    if (c->entry_used) {
        i=c->free_index;

        c->entry_used=FALSE;
        c->cur_entry=c->next_entry;
        c->cur_bits_in_use=c->next_bits_in_use;
        if (c->next_bits_in_use<ARC_MAX_BITS) {
            c->next_entry = &c->compress[i++];
            if (i==c->free_limit) {
                c->next_bits_in_use++;
                c->free_limit=1<<c->next_bits_in_use;
            }
        } else {
            do if (++i==c->free_limit) i=c->min_table_entry;
            while (c->hash[i]);
            temp=&c->compress[i];
            c->next_entry=temp;
            temp1=(CArcEntry *)&c->hash[temp->basecode];
            while (temp1 && temp1->next!=temp)
                temp1=temp1->next;
            if (temp1)
                temp1->next=temp->next;
        }
        c->free_index=i;
    }
}

void ArcExpandBuf(CArcCtrl *c)
{
    BYTE *dst_ptr,*dst_limit;
    DWORD basecode,lastcode,code;
    CArcEntry *temp,*temp1;

    dst_ptr=c->dst_buf+c->dst_pos;
    dst_limit=c->dst_buf+c->dst_size;

    while (dst_ptr<dst_limit && c->stk_ptr!=c->stk_base)
        *dst_ptr++ = * -- c->stk_ptr;

    if (c->stk_ptr==c->stk_base && dst_ptr<dst_limit) {
        if (c->saved_basecode==0xFFFFFFFFl) {
            lastcode=BFieldExtU32(c->src_buf,c->src_pos,
                    c->next_bits_in_use);
            c->src_pos=c->src_pos+c->next_bits_in_use;
            *dst_ptr++=lastcode;
            ArcEntryGet(c);
            c->last_ch=lastcode;
        } else
            lastcode=c->saved_basecode;
        while (dst_ptr<dst_limit && c->src_pos+c->next_bits_in_use<=c->src_size) {
            basecode=BFieldExtU32(c->src_buf,c->src_pos,
                    c->next_bits_in_use);
            c->src_pos=c->src_pos+c->next_bits_in_use;
            if (c->cur_entry==&c->compress[basecode]) {
                *c->stk_ptr++=c->last_ch;
                code=lastcode;
            } else
                code=basecode;
            while (code>=c->min_table_entry) {
                *c->stk_ptr++=c->compress[code].ch;
                code=c->compress[code].basecode;
            }
            *c->stk_ptr++=code;
            c->last_ch=code;

            c->entry_used=TRUE;
            temp=c->cur_entry;
            temp->basecode=lastcode;
            temp->ch=c->last_ch;
            temp1=(CArcEntry *)&c->hash[lastcode];
            temp->next=temp1->next;
            temp1->next=temp;

            ArcEntryGet(c);
            while (dst_ptr<dst_limit && c->stk_ptr!=c->stk_base)
                *dst_ptr++ = * -- c->stk_ptr;
            lastcode=basecode;
        }
        c->saved_basecode=lastcode;
    }
    c->dst_pos=dst_ptr-c->dst_buf;
}

CArcCtrl *ArcCtrlNew(DWORD expand,DWORD compression_type)
{
    CArcCtrl *c;
    c=(CArcCtrl *)malloc(sizeof(CArcCtrl));
    memset(c,0,sizeof(CArcCtrl)); // Couldn't you just do calloc here?
    if (expand) {
        c->stk_base=(BYTE *)malloc(1<<ARC_MAX_BITS);
        c->stk_ptr=c->stk_base;
    }
    if (compression_type==CT_7_BIT)
        c->min_bits=7;
    else
        c->min_bits=8;
    c->min_table_entry=1<<c->min_bits;
    c->free_index=c->min_table_entry;
    c->next_bits_in_use=c->min_bits+1;
    c->free_limit=1<<c->next_bits_in_use;
    c->saved_basecode=0xFFFFFFFFl;
    c->entry_used=TRUE;
    ArcEntryGet(c);
    c->entry_used=TRUE;
    return c;
}
*/
import "C"

type CompressionType uint8

const (
	CT_NONE CompressionType = iota + 1
	CT_7_BIT
	CT_8_BIT
)

const debug = false

func ExpandFile(b []byte) []byte {
	compressedSize := binary.LittleEndian.Uint64(b[:8])
	expandedSize := binary.LittleEndian.Uint64(b[8:16])
	compressionType := CompressionType(b[16])
	body := b[17:]
	if debug {
		fmt.Printf("compressed size: %x\n", compressedSize)
		fmt.Printf("expanded size: %x\n", expandedSize)
		fmt.Printf("compression type: %d\n", compressionType)
		fmt.Printf("size without header: %#x\n", len(body))
		fmt.Printf("result:\n")
	}
	return ExpandBuf(int64(compressedSize), int64(expandedSize), compressionType, body)
}

func ExpandBuf(compressedSize, expandedSize int64, compressionType CompressionType, body []byte) []byte {
	res := make([]byte, expandedSize)

	switch compressionType {
	case CT_NONE:
		return body
	case CT_7_BIT, CT_8_BIT:
		c := C.ArcCtrlNew(C.TRUE, C.uint(compressionType))
		c.src_size = C.uint(compressedSize << 3)
		c.src_pos = 0
		c.src_buf = (*C.uchar)(&body[0])
		c.dst_size = C.uint(expandedSize)
		c.dst_buf = (*C.uchar)(&res[0])
		c.dst_pos = 0
		C.ArcExpandBuf(c)
	}

	return res
}
