ccflags-y += -DEXTRA_CREDIT
obj-m += sys_xconcat.o

all: xhw1 xconcat

xhw1: xhw1.c
	gcc -Wall -Werror -I/lib/modules/$(shell uname -r)/build/arch/x86/include xhw1.c -o xhw1

xconcat:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xhw1
