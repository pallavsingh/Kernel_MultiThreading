# Name of the module
obj-m := simple_char_driver.o

# Use the current kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Default target: build the module
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean module build files
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


