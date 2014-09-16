obj-m := sguser.c
obj-m := xaui.o
xaui-objs :=  sguser.o 

KDIR= /lib/modules/$(shell uname -r)/build
ROOTDIR = $(PWD)
#vdebug = -DDEBUG_VERBOSE
EXTRA_CFLAGS += $(vdebug) $(ndebug) -DV6_TRD -DX86_PC -DUSE_IO_MACROS -DNWLDMA -DXAUI -I $(ROOTDIR)/xaui/ -I $(ROOTDIR)/include/ 

all:
	$(MAKE) $(CFLAGS) -C $(KDIR) SUBDIRS=$(PWD)/xaui 
	#$(MAKE) $(CFLAGS) -C $(KDIR) SUBDIRS=$(PWD)/xaui modules
clean: 
	   rm -rf *.o *.ko *.mod.* .c* .t* *.order *.symvers .*.cmd *.markers xaui Makefile~ *.c~ *.h~

# LOGGING: ---------------------------------------------------------------
# Add to EXTRA_CFLAGS, based on the logging level desired:
# -DDEBUG_NORMAL: To enable normal logging levels 
# -DDEBUG_VERBOSE: To enable verbose logging levels 
#
