export KDIR ?= /lib/modules/$(shell uname -r)/build

# CLANG ?= clang
# ifeq ($(origin CC),default)
# CC := ${CLANG}
# endif

# obj-m += ktcp.o krdma.o

# all:
# 	$(MAKE) -C $(KDIR) M=$(CURDIR) CC=$(CC) CONFIG_CC_IS_CLANG=y modules

# clean:
# 	$(MAKE) -C $(KDIR) M=$(CURDIR) CC=$(CC) clean

obj-m += ktcp.o krdma.o
MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make -C $(KDIR) M=$(PWD) modules

debug:
	make -C $(KDIR) M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C $(KDIR) M=$(PWD) clean 