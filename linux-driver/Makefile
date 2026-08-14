obj-m+=stm_driver.o

path=/lib/modules/$(shell uname -r)/build

pwd =$(shell pwd)


all:
	make -C $(path) M=$(pwd) modules


clean :
	make -C $(path) M=$(pwd) clean

