APP_EXTRA_FLAGS:= -O2 -ansi -pedantic
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build

all: clean modules app

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

  app: userapp.c userapp.h
	gcc -o userapp userapp.c -g

clean:
	rm -f userapp *~ *.ko *.o *.mod.c Module.symvers modules.order
