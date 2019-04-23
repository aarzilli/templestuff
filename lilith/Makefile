all: lilith

clean:
	rm lilith

# gcc stack protector writes to the FS segment which we need for TempleOS
# related stuff, hence it must be disabled.

lilith: *.c
	gcc -g -o lilith -fno-stack-protector -Wall lilith.c
