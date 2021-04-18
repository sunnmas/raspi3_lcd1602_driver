TARGET = lcd1602
obj-m	:= $(TARGET).o

KERNELDIR := ../linux-rpi-5.4.y/
PWD       := $(shell pwd)
CC = gcc
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions