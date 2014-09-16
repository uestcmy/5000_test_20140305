#
#	Makefile for Xilinx Virtex-6 PCIe-10GDMA-DDR3-XAUI targeted reference design
#

MKNOD = `awk '/xdma_stat/ {print $$1}' /proc/devices`
MKNOD2 = `awk '/ml605_raw_data/ {print $$1}' /proc/devices`

all::
		$(MAKE) -C xdma
#		$(MAKE) -C xaui
		$(MAKE) -C xrawdata
		@echo "***** Driver Compiled *****"

clean::
		$(MAKE) -C xdma clean
#		$(MAKE) -C xaui clean
		$(MAKE) -C xrawdata clean

insert:: xdma/xdma_v6.ko xrawdata/xrawdata_v6.ko
		/sbin/insmod xdma/xdma_v6.ko; sleep 1
		/bin/mknod /dev/xdma_stat c $(MKNOD) 0
#		/sbin/insmod xaui/xaui.ko; sleep 1
		/sbin/insmod xrawdata/xrawdata_v6.ko; sleep 1
		/bin/mknod /dev/ml605_raw_data c $(MKNOD2) 0
		@echo "***** Driver Loaded *****"

remove::
		rm -f /dev/ml605_raw_data
		/sbin/rmmod xrawdata_v6.ko; sleep 1
		/sbin/rmmod xaui.ko; sleep 1; rm -f /dev/xdma_stat
		/sbin/rmmod xdma_v6.ko
		@echo "***** Driver Unloaded *****"

