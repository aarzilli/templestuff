all: lilith

clean:
	rm lilith

# gcc stack protector writes to the FS segment which we need for TempleOS
# related stuff, hence it must be disabled.

lilith: *.c *.s
	gcc -g -mno-red-zone -o lilith -fno-stack-protector -Wall lilith.c lilith.s -lpthread -lX11 -lXext

static.c: static/*
	go run scripts/generate.go

syscalls.s: syscalls.c scripts/syscall_generate.go
	go run scripts/syscall_generate.go

