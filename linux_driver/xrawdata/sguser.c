/* $Id: sguser.c,v 1.10 2010/04/27 15:08:53 ps Exp $ */
/*******************************************************************************
** © Copyright 2010 - 2011 Xilinx, Inc. All rights reserved.
** This file contains confidential and proprietary information of Xilinx, Inc. and
** is protected under U.S. and international copyright and other intellectual property laws.
*******************************************************************************
**
**
**  Vendor: Xilinx
**
**
**
**
**  Virtex-6 PCIe-10GDMA-DDR3-XAUI-AXI Targeted Reference Design
**
**
**  Device: xc6vlx240t
**  Version: 1.2
**  Reference: UG379
**
*******************************************************************************
**
**  Disclaimer:
**
**    This disclaimer is not a license and does not grant any rights to the materials
**              distributed herewith. Except as otherwise provided in a valid license issued to you
**              by Xilinx, and to the maximum extent permitted by applicable law:
**              (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS,
**              AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
**              INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR
**              FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether in contract
**              or tort, including negligence, or under any other theory of liability) for any loss or damage
**              of any kind or nature related to, arising under or in connection with these materials,
**              including for any direct, or any indirect, special, incidental, or consequential loss
**              or damage (including loss of data, profits, goodwill, or any type of loss or damage suffered
**              as a result of any action brought by a third party) even if such damage or loss was
**              reasonably foreseeable or Xilinx had been advised of the possibility of the same.


**  Critical Applications:
**
**    Xilinx products are not designed or intended to be fail-safe, or for use in any application
**    requiring fail-safe performance, such as life-support or safety devices or systems,
**    Class III medical devices, nuclear facilities, applications related to the deployment of airbags,
**    or any other applications that could lead to death, personal injury, or severe property or
**    environmental damage (individually and collectively, "Critical Applications"). Customer assumes
**    the sole risk and liability of any use of Xilinx products in Critical Applications, subject only
**    to applicable laws and regulations governing limitations on product liability.

**  THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT ALL TIMES.

*******************************************************************************/
/*****************************************************************************/
/**
 *
 * @file xdma_base.c
 *
 * This is the Linux base driver for the DMA engine core. It provides
 * multi-channel DMA capability with the help of the Northwest Logic
 * DMA engine.
 *
 * Author: Xilinx, Inc.
 *
 * 2007-2010 (c) Xilinx, Inc. This file is licensed uner the terms of the GNU
 * General Public License version 2.1. This program is licensed "as is" without
 * any warranty of any kind, whether express or implied.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Date     Changes
 * ----- -------- -------------------------------------------------------
 * 1.0   12/07/09 First release
 * </pre>
 *
 *****************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include "xdma_user.h"
#include "xpmon_be.h"
#include "xdebug.h"
#include "xio.h"
#include "ml605_api.h"

#define USER_DATA
// #define WY_TEST
#define WY_NEW_LOCK
#ifdef WY_NEW_LOCK
static volatile bool is_read_busy = false;
#endif   

#define XDMA_FILENAME    "/dev/xdma_stat"

/* Driver states */
#define UNINITIALIZED   0       /* Not yet come up */
#define INITIALIZED     1       /* But not yet set up for polling */
#define POLLING         2       /* But not yet registered with DMA */
#define REGISTERED      3       /* Registered with DMA */
#define CLOSED          4       /* Driver is being brought down */

/* DMA characteristics */
#define MYBAR           0

#ifdef XAUI
#define MYNAME  "XAUI"
#else
#define MYNAME   "Raw Data"
#endif

#ifdef XAUI

#define TX_CONFIG_ADDRESS       0x9008
#define LINK_STATUS_ADDRESS     0x900C

/* Test start / stop conditions */
#define LOOPBACK            0x0001

/* Link status conditions */
#define RX_LINK_UP          0x00000080  /**< RX link up / down */
#define RX_ALIGNED          0x00000040  /**< RX link aligned */

#else

#define TIMING_STATUS       0x9000	// Reg for HW counter - ms and chip counter
#define RF_CONTROL          0x9008	// Reg for RF command
#define TX_CONFIG_ADDRESS   0x9108  /* Reg for controlling TX data */
#define RX_CONFIG_ADDRESS   0x9100  /* Reg for controlling RX pkt generator */
#define PKT_SIZE_ADDRESS    0x9104  /* Reg for programming packet size */
#define STATUS_ADDRESS      0x910C  /* Reg for checking TX pkt checker status */

/* Test start / stop conditions */
#define LOOPBACK            0x00000002  /* Enable TX data loopback onto RX */
#endif

/* Test start / stop conditions */
#define PKTCHKR             0x00000001  /* Enable TX packet checker */
#define PKTGENR             0x00000001  /* Enable RX packet generator */
#define CHKR_MISMATCH       0x00000001  /* TX checker reported data mismatch */

#ifdef XAUI
#define ENGINE_TX       0
#define ENGINE_RX       32
#else
#define ENGINE_TX       1
#define ENGINE_RX       33
#endif

/* Packet characteristics */
#define BUFSIZE         (PAGE_SIZE)
#ifdef XAUI
#define MAXPKTSIZE      (4*PAGE_SIZE)
#else
#define MAXPKTSIZE      (8*PAGE_SIZE)
#endif
#define MINPKTSIZE      (64)
#define NUM_BUFS        2000
#define BUFALIGN        8
#define BYTEMULTIPLE    8   /**< Lowest sub-multiple of memory path */

// raw data character driver related variables
struct cdev * rawdataCdev = NULL;
static int UserOpen = 0;
static const int kMaxUserOpen = 2;
// static char TestBuf[BUFSIZE];

static int rawdata_dev_open(struct inode * in, struct file * filp);
static int rawdata_dev_release(struct inode * in, struct file * filp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int rawdata_dev_ioctl(struct inode * in, struct file * filp,
                             unsigned int cmd, unsigned long arg);
#else
static long rawdata_dev_ioctl(struct file * filp,
                          unsigned int cmd, unsigned long arg);
#endif                             
static ssize_t rawdata_dev_read(struct file *filp, char __user *buf,
                                size_t count, loff_t *f_pos);
static ssize_t rawdata_dev_write(struct file *filp, const char __user *buf,
                                 size_t count, loff_t *f_pos);

int DriverState = UNINITIALIZED;
struct timer_list poll_timer;
void * handle[4] = {NULL, NULL, NULL, NULL};
unsigned long TXbarbase, RXbarbase;
u32 polldata = 0xaa55;
u32 RawTestMode = TEST_STOP;
u32 RawMinPktSize=MINPKTSIZE, RawMaxPktSize=MAXPKTSIZE;

typedef struct {
    int TotalNum;
    int AllocNum;
    int FirstBuf;
    int LastBuf;
    int FreePtr;
    int AllocPtr;
    unsigned char * origVA[NUM_BUFS];
    // below fields are inserted in order to support user application data Rx
    int RxNum;
    int RxTotalBytes;
    int rxBytes[NUM_BUFS];
} Buffer;

Buffer TxBufs;
Buffer RxBufs;
PktBuf pkts[NUM_BUFS];

/* For exclusion */
spinlock_t RawLock;
// July 6, 2012, WANG Yang. Specific lock for read operation.
spinlock_t RawReadLock;

#ifdef XAUI
#define DRIVER_NAME         "xxaui_driver"
#define DRIVER_DESCRIPTION  "Xilinx XAUI Data Driver"
#else
#define DRIVER_NAME         "xrawdata_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data Driver"
#endif

static void poll_routine(unsigned long __opaque);
static void InitBuffers(Buffer * bptr);

// static void FormatBuffer(unsigned char * buf, int pktsize, int bufsize, int fragment);
#ifdef DATA_VERIFY
static void VerifyBuffer(unsigned char * buf, int size, unsigned long long uinfo);
#endif

int myInit(unsigned long, unsigned int);
int myFreePkt(void *, unsigned int *, int, unsigned int);
// static int DmaSetupTransmit(void *, int);
int myGetRxPkt(void *, PktBuf *, unsigned int, int, unsigned int);
int myPutTxPkt(void *, PktBuf *, int, unsigned int);
int myPutRxPkt(void *, PktBuf *, int, unsigned int);
int mySetState(void * hndl, UserState * ustate, unsigned int privdata);
int myGetState(void * hndl, UserState * ustate, unsigned int privdata);

extern unsigned int CRC(unsigned int * buf, int len);

/* For checking data integrity */
unsigned int TxBufCnt=0;
unsigned int RxBufCnt=0;
unsigned int ErrCnt=0;

unsigned short TxSeqNo = 0;
unsigned short RxSeqNo = 0;

/* Simplistic buffer management algorithm. Buffers must be freed in the
 * order in which they have been allocated. Out of order buffer frees will
 * result in the wrong buffer being freed and may cause a system hang during
 * the next buffer/DMA access.
 */
static void InitBuffers(Buffer * bptr)
{
    unsigned char * bufVA;
    int i;
    
    printk("InitBuffers() is invoked\n");

    /* Initialise */
    bptr->TotalNum = bptr->AllocNum = 0;
    bptr->FirstBuf = 0;
    bptr->LastBuf = 0;
    bptr->FreePtr = 0;
    bptr->AllocPtr = 0;
    bptr->RxNum = 0;
    bptr->RxTotalBytes = 0;

    /* Allocate for TX buffer pool - have not taken care of alignment */
    for(i = 0; i < NUM_BUFS; i++)
    {
        if((bufVA = (unsigned char *)__get_free_pages(GFP_KERNEL, get_order(BUFSIZE))) == NULL)
        {
            printk("InitBuffers: Unable to allocate [%d] TX buffer for data\n", i);
            break;
        }
        bptr->origVA[i] = bufVA;
        bptr->rxBytes[i] = 0;
    }
    printk("Allocated %d buffers from %p\n", i, (void *)bufVA);
    printk("Buffer size = %d\n", BUFSIZE);

#if 0
    /* Do the buffer alignment adjustment, if required */
    if((u32)bufVA % BUFALIGN)
        bufVA += (BUFALIGN - ((u32)bufVA & 0xFF));
#endif

    if(i)
    {
        bptr->FirstBuf = 0;
        bptr->LastBuf = i;      // One more than last valid buffer.
        bptr->FreePtr = 0;
        bptr->AllocPtr = 0;
        bptr->TotalNum = i;
        bptr->AllocNum = 0;
        bptr->RxNum = 0;
        bptr->RxTotalBytes = 0;
    }
    //printk("Buffers allocated first %x last %x, number %d\n",
    //                (u32)(bptr->origVA[0]), (u32)(bptr->origVA[i-1]), i);
}

static inline unsigned char * AllocBuf(Buffer * bptr)
{
    unsigned char * cptr;
    int freeptr;

    if(bptr->AllocNum == bptr->TotalNum)
    {
//        printk("No buffers available to allocate\n");
        return NULL;
    }

    freeptr = bptr->FreePtr;

    //printk("Before allocating:\n");
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);

    cptr = bptr->origVA[freeptr];
    bptr->FreePtr ++;
    if(bptr->FreePtr == bptr->LastBuf)
        bptr->FreePtr = 0;
    bptr->AllocNum++;

    //printk("After allocating:\n");
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);

    return cptr;
}

static inline void FreeUsedBuf(Buffer * bptr, int num)
{
    /* This version differs from FreeUnusedBuf() in that this frees used
     * buffers by moving the AllocPtr, while the other frees unused buffers
     * by moving the FreePtr.
     */
    int i;

    //printk("Trying to free %d buffers\n", num);
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);

    for(i=0; i<num; i++)
    {
        if(bptr->AllocNum == 0)
        {
            printk("No more buffers allocated, cannot free anything\n");
            break;
        }

        bptr->AllocPtr ++;
        if(bptr->AllocPtr == bptr->LastBuf)
            bptr->AllocPtr = 0;
        bptr->AllocNum--;
    }
    //printk("After freeing:\n");
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);
}

static inline void FreeUnusedBuf(Buffer * bptr, int num)
{
    /* This version differs from FreeUsedBuf() in that this frees unused buffers
     * by moving the FreePtr, while the other frees used buffers by moving
     * the AllocPtr.
     */
    int i;

    //printk("Trying to free %d unused buffers\n", num);
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);

    for(i=0; i<num; i++)
    {
        if(bptr->AllocNum == 0)
        {
            printk("No more buffers allocated, cannot free anything. bptr = 0x%x\n", bptr);
            break;
        }

        if(bptr->FreePtr == 0)
            bptr->FreePtr = bptr->LastBuf;
        bptr->FreePtr --;
        bptr->AllocNum--;
    }
    //printk("After freeing:\n");
    //printk("Allocated %d buffers\n", bptr->AllocNum);
    //printk("FreePtr %d AllocPtr %d\n", bptr->FreePtr, bptr->AllocPtr);
}

static inline void PrintSummary(void)
{
#ifndef XAUI
    u32 val;
#endif

    printk("---------------------------------------------------\n");
    printk("%s Driver results Summary:-\n", MYNAME);
    printk("Current Run Min Packet Size = %d, Max Packet Size = %d\n",
                            RawMinPktSize, RawMaxPktSize);
    printk("Buffers Transmitted = %u, Buffers Received = %u, Error Count = %u\n", TxBufCnt, RxBufCnt, ErrCnt);
    printk("TxSeqNo = %u, RxSeqNo = %u\n", TxSeqNo, RxSeqNo);

#ifndef XAUI
    val = XIo_In32(TXbarbase+STATUS_ADDRESS);
    printk("Data Mismatch Status = %x\n", val);
#endif

    printk("---------------------------------------------------\n");
}

#ifndef USER_DATA
static void FormatBuffer(unsigned char * buf, int pktsize, int bufsize, int fragment)
{
    int i;

    /* Apply data pattern in the buffer */
    for(i = 0; i < bufsize; i = i+2)
        *(unsigned short *)(buf + i) = TxSeqNo;

    /* Update header information for the first fragment in packet */
    if(!fragment)
    {
        /* Apply packet length and sequence number */
        *(unsigned short *)(buf + 0) = pktsize;
        *(unsigned short *)(buf + 2) = TxSeqNo;
    }

#ifdef DEBUG_VERBOSE
    printk("TX Buffer has:\n");
    for(i=0; i<bufsize; i++)
    {
        if(!(i % 32)) printk("\n");
        printk("%02x ", buf[i]);
    }
    printk("\n");
#endif
}
#endif

#ifdef DATA_VERIFY
static void VerifyBuffer(unsigned char * buf, int size, unsigned long long uinfo)
{
    unsigned int check4;
    unsigned short check2, check6;
    unsigned char * bptr;

#ifdef DEBUG_VERBOSE
    int i;

    printk("%s: RX packet len %d uinfo %llx\n", MYNAME, size, uinfo);
    for(i=0; i<size; i++)
    {
        if(i && !(i % 16)) printk("\n");
        printk("%02x ", buf[i]);
    }
    printk("\n");
#endif

    bptr = buf+size;
    check6 = *(unsigned short *)(bptr-6);
    check4 = *(unsigned int *)(bptr-4);
    check2 = *(unsigned short *)(bptr-2);
#ifdef XAUI
    if(check4 != (unsigned int)uinfo)
#else
    if(check2 != RxSeqNo)
#endif
    {
        ErrCnt++;
        printk("Mismatch: Size %x SeqNo %x uinfo %x, buf has %x\n",
                        size, (*(unsigned short *)(buf+2)),
                        (unsigned int)uinfo, check4);
        printk("RxSeqNo %x\n", RxSeqNo);

        {
            int i;
            for(i=0; i<8; i++) printk("%02x ", buf[i]);
            printk("...");
            for(i=0; i<8; i++) printk("%02x ", buf[size-8+i]);
            printk("\n");
        }

        /* Update RxSeqNo */
        RxSeqNo = check6;
    }
}
#endif

// Character driver related operations
static int rawdata_dev_open(struct inode * in, struct file * filp)
{
  if (DriverState != REGISTERED)
  {
    printk("Driver rawdata not yet ready!\n");
    return -1;
  }

  if (UserOpen >= kMaxUserOpen)
  {
    printk("Device already in use\n");
    return -EACCES;
  }

  spin_lock_bh(&RawLock);
  ++UserOpen;   // To prevent more than one user application
  spin_unlock_bh(&RawLock);

  return 0;
}

static int rawdata_dev_release(struct inode * in, struct file * filp)
{
  if (!UserOpen)
  {
    /* Should not come here */
    printk("Device rawdata not in use\n");
    return -EFAULT;
  }

  spin_lock_bh(&RawLock);
  --UserOpen;
  spin_unlock_bh(&RawLock);

  return 0;
}

static ssize_t rawdata_dev_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos)
{
  PktBuf *pbuf;
  unsigned char *bufVA;
  int result;
  int origseqno;
  int avail;
//	int i;
#ifdef WY_NEW_LOCK
  static volatile bool is_write_busy = false;
#endif    

  // Return value used if there is *not* enough buffer space
  ssize_t retval = -ENOMEM;

	bufVA = NULL;

  origseqno = TxSeqNo;

  if (DriverState != REGISTERED)
  {
    return -EPERM;
  }
  else if (count <= BUFSIZE)
  {
#ifndef WY_NEW_LOCK  
    spin_lock_bh(&RawLock);
#else
    // The following operation (copy_from_user) might sleep, so we don't want to keep the spinlock that long time.
    // If device is busy, we simply return EBUSY error to API.
    // API could retry write operation after sleeping a short while.
    if (spin_trylock_bh(&RawLock) == 0) {
      return -EBUSY;
    }
    if (is_write_busy) {
      spin_unlock_bh(&RawLock);
      return -EBUSY;
    }
    is_write_busy = true;
    spin_unlock_bh(&RawLock);
#endif
//    /* Only if the test mode is set, and only for TX direction */
//    if((RawTestMode & TEST_START) &&
//       (RawTestMode & (ENABLE_PKTCHK|ENABLE_LOOPBACK)))
//    {
//      /* First change the state */
//      RawTestMode &= ~TEST_START;
//      RawTestMode |= TEST_IN_PROGRESS;
//    }

    avail = (TxBufs.TotalNum - TxBufs.AllocNum);
    if (avail > 0)
    {
      pbuf = &(pkts[0]);

      /* Allocate a buffer. DMA driver will map to PCI space. */
      bufVA = AllocBuf(&TxBufs);

      log_verbose(KERN_INFO "TX: The buffer after alloc is at address %x size %d\n",
                          (unsigned long) bufVA, (u32) BUFSIZE);
      if (bufVA != NULL)
      {
        pbuf->pktBuf = bufVA;
        pbuf->bufInfo = bufVA;
        pbuf->size = count;
        pbuf->userInfo = TxSeqNo;
        pbuf->flags = PKT_ALL | PKT_SOP | PKT_EOP;

        // copy from user to Tx buffer
        if (copy_from_user(bufVA, buf, count))
        {
          printk("Copy_from_user failed\n");
          FreeUnusedBuf(&TxBufs, 1);
        }
        else
        {
//					for (i = 0; i < 8; ++i) {
//						printk("%2x\t", bufVA[i]);
//					}
//					printk("\n");
          retval = count;
        }

        ++TxSeqNo;
      }
    }

#ifndef WY_NEW_LOCK
  	spin_unlock_bh(&RawLock);
#else
//    spin_lock_bh(&RawLock);
    is_write_busy = false;
//    spin_unlock_bh(&RawLock);
#endif

		if (bufVA != NULL)
		{
		  result = DmaSendPkt(handle[0], pkts, 1);
	//    printk("DmaSendPkt result = %d\n", result);
		  TxBufCnt += result;
		  if(result != 1)
		  {
		      log_normal(KERN_ERR "Tried to send %d pkts in %d buffers, sent only %d\n",
		                                  1, 1, result);
		      //printk("[s%d-%d,%d-%d]", bufindex, result, TxSeqNo, origseqno);
		      if(result) TxSeqNo = pkts[result].userInfo;
		      else TxSeqNo = origseqno;
		      //printk("-%u-", TxSeqNo);
		      //lastno = TxSeqNo;

//		      spin_lock_bh(&RawLock);
		      FreeUnusedBuf(&TxBufs, (1-result));
//		      spin_unlock_bh(&RawLock);

		      return -98;
		  }
		}
  }

//	printk("retval=%d\n", (int)retval);
  return retval;
}

static ssize_t rawdata_dev_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *f_pos)
{
  ssize_t retval = 0;
  int num_copied_bytes = 0;
  int num_copied_pkts = 0;
	int num_pkt_index = RxBufs.AllocPtr;

#ifdef WY_NEW_LOCK  
//  spin_lock_bh(&RawLock);
  spin_lock_bh(&RawReadLock);
#else
  // The following operation (copy_from_user) might sleep, so we don't want to keep the spinlock that long time.
  // If device is busy, we simply return EBUSY error to API.
  // API could retry write operation after sleeping a short while.
  if (spin_trylock_bh(&RawReadLock) == 0) {
    return -EBUSY;
  }
  if (is_read_busy) {
    spin_unlock_bh(&RawReadLock);
    return -EBUSY;
  }
  is_read_busy = true;
  spin_unlock_bh(&RawReadLock);
#endif
//	printk("InState: AllocNum=%d, RxNum=%d, RxTotalBytes=%d\n",
//				 RxBufs.AllocNum, RxBufs.RxNum, RxBufs.RxTotalBytes);

  if (count <= BUFSIZE)
  {
    if (RxBufs.RxTotalBytes >= count)
    {
      while (num_copied_bytes + RxBufs.rxBytes[num_pkt_index] <= count)
      {
//				printk("AllocPtr=%d, num_copied_pkts=%d, num_copied_bytes=%d, rxBytes[]=%d, count=%d\n",
//							 RxBufs.AllocPtr, num_copied_pkts, num_copied_bytes,
//							 RxBufs.rxBytes[RxBufs.AllocPtr + num_copied_pkts], (unsigned)count);
        if ((retval =
							copy_to_user(buf + num_copied_bytes, RxBufs.origVA[num_pkt_index], RxBufs.rxBytes[num_pkt_index])))
        {
          printk("copy_to_user failed. retval=%d\n", (int)retval);
//					printk("buf=0x%lx, num_copied_bytes=%d, num_copied_pkts=%d\n",
//								 (long unsigned)buf, num_copied_bytes, num_copied_pkts);
//					printk("RxTotalBytes=%d, rxBytes[%d]=%d\n",
//								 RxBufs.RxTotalBytes, num_pkt_index, RxBufs.rxBytes[num_pkt_index]);
          retval = -EFAULT;
          break;
        }
        num_copied_bytes += RxBufs.rxBytes[num_pkt_index];
		    RxBufs.RxTotalBytes -= RxBufs.rxBytes[num_pkt_index];
        RxBufs.rxBytes[num_pkt_index] = 0;
				++num_pkt_index;
				if (num_pkt_index == RxBufs.TotalNum)
				{
					num_pkt_index = 0;
				}
        ++num_copied_pkts;
		    RxBufs.RxNum--;
        retval = num_copied_bytes;

				if (num_copied_bytes == count)
				{
					break;
				}
      } // while loop
    }
  }

  if (num_copied_pkts)
  {
//		printk("Try to free %d Rx pkts\n", num_copied_pkts);
//		printk("AllocNum = %d\n", RxBufs.AllocNum);
    FreeUsedBuf(&RxBufs, num_copied_pkts);
  }

//	printk("OutState: AllocNum=%d, RxNum=%d, RxTotalBytes=%d\n",
//				 RxBufs.AllocNum, RxBufs.RxNum, RxBufs.RxTotalBytes);
#ifdef WY_NEW_LOCK
//	spin_unlock_bh(&RawLock);
	spin_unlock_bh(&RawReadLock);
#else
  is_read_busy = false;
#endif

  return retval;
  //return PAGE_SIZE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int rawdata_dev_ioctl(struct inode * in, struct file * filp,
                             unsigned int cmd, unsigned long arg)
#else
static long rawdata_dev_ioctl(struct file * filp,
                          unsigned int cmd, unsigned long arg)
#endif 
{
  int retval = 0;
  int val = 0;
#ifdef WY_NEW_LOCK
  static volatile bool is_ioctl_busy = false;
#endif    
  
  if(DriverState != REGISTERED)
  {
      /* Should not come here */
      printk("Driver not yet ready!\n");
      return -EPERM;
  }

  /* Check cmd type and value */
  if(_IOC_TYPE(cmd) != ML605_MAGIC) return -ENOTTY;
  if(_IOC_NR(cmd) > ML605_MAX_CMD) return -ENOTTY;

  /* Check read/write and corresponding argument */
  if(_IOC_DIR(cmd) & _IOC_READ)
    if(!access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd)))
      return -EFAULT;
  if(_IOC_DIR(cmd) & _IOC_WRITE)
    if(!access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd)))
      return -EFAULT;

#ifndef WY_NEW_LOCK
	spin_lock_bh(&RawLock);
#else
  // The following operation (copy_from_user, copy_to_user) might sleep, so we don't want to keep the spinlock that long time.
  // If device is busy, we simply return EBUSY error to API.
  // API could retry ioctl operation after sleeping a short while.
  if (spin_trylock_bh(&RawLock) == 0) {
    return -EBUSY;
  }
  if (is_ioctl_busy) {
    spin_unlock_bh(&RawLock);
    return -EBUSY;
  }
  is_ioctl_busy = true;
  spin_unlock_bh(&RawLock);
#endif

  switch (cmd)
  {
  case RD_CMD_QUERY_TX_BUF:
    val = (TxBufs.TotalNum - TxBufs.AllocNum) * BUFSIZE;
//		printk("TxBuf=%d\n", val);
    if(copy_to_user((int *)arg, &val, sizeof(int)))
    {
      printk("copy_to_user failed\n");
      retval = -EFAULT;
    }
    break;
  case RD_CMD_QUERY_RX_BUF:
    val = RxBufs.RxTotalBytes;
    if(copy_to_user((int *)arg, &val, sizeof(int)))
    {
      printk("copy_to_user failed\n");
      retval = -EFAULT;
    }
    break;
  case RD_CMD_GET_COUNTER:
    val = XIo_In32(TXbarbase+TIMING_STATUS);
    if(copy_to_user((int *)arg, &val, sizeof(int)))
    {
      printk("copy_to_user failed\n");
      retval = -EFAULT;
    }
    break;
  case RD_CMD_SET_RF_CMD:
//		printk("Hello from rawdata_dev_ioctl\n");
    // clear RF cmd send signal
    XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, 0);

    // write RF cmd
    if(copy_from_user(&val, (int *)arg, sizeof(int)))
    {
      printk("copy_from_user failed\n");
      retval = -EFAULT;
    }
    XIo_Out32(TXbarbase+RF_CONTROL, val);

    // set RF cmd send signal
    XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, PKTGENR);
		udelay(40);
    // clear RF cmd send signal
    XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, 0);
		break;
  default:
    printk("Invalid command %d\n", cmd);
    retval = -EINVAL;
  }

#ifndef WY_NEW_LOCK
	spin_unlock_bh(&RawLock);
#else
//  spin_lock_bh(&RawLock);
  is_ioctl_busy = false;
//  spin_unlock_bh(&RawLock);
#endif

  return retval;
}

static void poll_routine(unsigned long __opaque)
{
//    struct timer_list *timer = &poll_timer;
    UserPtrs ufuncs;
//    int offset = (2*HZ);

    //printk("Came to poll_routine with %x\n", (u32)(__opaque));

    /* Register with DMA incase not already done so */
    if(DriverState < POLLING)
    {
        spin_lock_bh(&RawLock);
        printk("Calling DmaRegister on engine %d and %d\n",
                            ENGINE_TX, ENGINE_RX);
        DriverState = REGISTERED;

        ufuncs.UserInit = myInit;
        ufuncs.UserPutPkt = myPutTxPkt;
        ufuncs.UserSetState = mySetState;
        ufuncs.UserGetState = myGetState;
        ufuncs.privData = 0x54545454;
        spin_unlock_bh(&RawLock);

        if((handle[0]=DmaRegister(ENGINE_TX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
        {
            printk("Register for engine %d failed. Stopping.\n", ENGINE_TX);
            spin_lock_bh(&RawLock);
            DriverState = UNINITIALIZED;
            spin_unlock_bh(&RawLock);
            return;     // This polling will not happen again.
        }
        printk("Handle for engine %d is %p\n", ENGINE_TX, handle[0]);

        spin_lock_bh(&RawLock);
        ufuncs.UserInit = myInit;
        ufuncs.UserPutPkt = myPutRxPkt;
        ufuncs.UserGetPkt = myGetRxPkt;
        ufuncs.UserSetState = mySetState;
        ufuncs.UserGetState = myGetState;
        ufuncs.privData = 0x54545456;
        spin_unlock_bh(&RawLock);

        if((handle[2]=DmaRegister(ENGINE_RX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
        {
            printk("Register for engine %d failed. Stopping.\n", ENGINE_RX);
            spin_lock_bh(&RawLock);
            DriverState = UNINITIALIZED;
            spin_unlock_bh(&RawLock);
            return;     // This polling will not happen again.
        }
        printk("Handle for engine %d is %p\n", ENGINE_RX, handle[2]);

        /* Reschedule poll routine */
//        timer->expires = jiffies + offset;
//        add_timer(timer);

//        ssleep(2);   // sleep 2 seconds
    }
}

void CheckBuffer(unsigned char *ptrBuf, unsigned int len)
{
  int i;
  printk("Rx packet:\n");
  for (i = 0; i < len; ++i)
  {
    printk("%d\t", ptrBuf[i]);
    if (15 == (i%16))
    {
      printk("\n");
    }
  }
}


int myInit(unsigned long barbase, unsigned int privdata)
{
    log_normal("Reached myInit with barbase %x and privdata %x\n",
                barbase, privdata);

    spin_lock_bh(&RawLock);
    if(privdata == 0x54545454)  // So that this is done only once
    {
        TXbarbase = barbase;
    }
    else if(privdata == 0x54545456)  // So that this is done only once
    {
        RXbarbase = barbase;
    }
    TxBufCnt = 0; RxBufCnt = 0;
    ErrCnt = 0;
    TxSeqNo = RxSeqNo = 0;

    /* Stop any running tests. The driver could have been unloaded without
     * stopping running tests the last time. Hence, good to reset everything.
     */
    XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, 0);
#ifndef XAUI
    XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, 0);
#endif

    spin_unlock_bh(&RawLock);

    return 0;
}

int myPutRxPkt(void * hndl, PktBuf * vaddr, int numpkts, unsigned int privdata)
{
    int i, unused=0;
    unsigned int flags;
		int num_buf_index;

    //printk("Reached myPutRxPkt with handle %p, VA %x, size %d, privdata %x\n",
    //            hndl, (u32)vaddr, size, privdata);

    /* Check driver state */
    if(DriverState != REGISTERED)
    {
        printk("Driver does not seem to be ready\n");
        return -1;
    }

    /* Check handle value */
    if(hndl != handle[2])
    {
        log_normal("PutRxPkt: Came with wrong handle %x\n", (u32)hndl);
        return -1;
    }
#ifdef WY_TEST
		else
		{
				log_normal("PutRxPkt: Came with correct handle %x\n", (u32)hndl);
		}
#endif

#ifdef WY_NEW_LOCK
//    spin_lock_bh(&RawLock);
    spin_lock_bh(&RawReadLock);
#else
    while (spin_trylock_bh(&RawReadLock) == 0) {
      udelay(10);
    }
    while (is_read_busy) {
      udelay(10);
    }
    is_read_busy = true;
    spin_unlock_bh(&RawReadLock);
#endif    

    for(i=0; i<numpkts; i++)
    {
        flags = vaddr->flags;
        //printk("RX pkt flags %x\n", flags);
        if(flags & PKT_UNUSED)
        {
            unused = 1;
            break;
        }
        if(flags & PKT_EOP)
        {
#ifdef DATA_VERIFY
            VerifyBuffer((unsigned char *)(vaddr->bufInfo), (vaddr->size), (vaddr->userInfo));
#endif
//            CheckBuffer(vaddr->pktBuf, vaddr->size);
            RxSeqNo++;
        }

				num_buf_index = RxBufs.AllocPtr + RxBufs.RxNum;
				if (num_buf_index >= RxBufs.TotalNum)
				{
					num_buf_index -= RxBufs.TotalNum;
				}
        RxBufs.RxTotalBytes += vaddr->size;
        RxBufs.rxBytes[num_buf_index] = vaddr->size;
        RxBufs.RxNum++;
        RxBufCnt++;
        vaddr++;
    }

    /* Return packet buffers to free pool */

    //printk("PutRxPkt: Freeing %d packets unused %d\n", numpkts, unused);
    if(unused)
        FreeUnusedBuf(&RxBufs, numpkts);
//    else
//        FreeUsedBuf(&RxBufs, numpkts);

#ifdef WY_NEW_LOCK
//  	spin_unlock_bh(&RawLock);
  	spin_unlock_bh(&RawReadLock);
#else
    is_read_busy = false;
#endif

    return 0;
}

int myGetRxPkt(void * hndl, PktBuf * vaddr, unsigned int size, int numpkts, unsigned int privdata)
{
    unsigned char * bufVA;
    PktBuf * pbuf;
    int i;

    //printk(KERN_INFO "myGetRxPkt: Came with handle %p size %d privdata %x\n",
    //                        hndl, size, privdata);

    /* Check driver state */
    if(DriverState != REGISTERED)
    {
        printk("Driver does not seem to be ready\n");
        return 0;
    }

    /* Check handle value */
    if(hndl != handle[2])
    {
        printk("GetRxPkt: Came with wrong handle %lx\n", (unsigned long)hndl);
        printk("Should be %lx\n", (unsigned long)handle[2]);
        return 0;
    }
#ifdef WY_TEST
    else
    {
        printk("GetRxPkt: Came with correct handle %lx\n", (unsigned long)hndl);
    }
#endif

    /* Check size value */
    if(size != BUFSIZE)
        printk("myGetRxPkt: Requested size %d does not match mine %d\n",
                                size, (u32)BUFSIZE);

#ifdef WY_NEW_LOCK
//    spin_lock_bh(&RawLock);
    spin_lock_bh(&RawReadLock);
#else
    while (spin_trylock_bh(&RawReadLock) == 0) {
      udelay(10);
    }
    while (is_read_busy) {
      udelay(10);
    }
    is_read_busy = true;
    spin_unlock_bh(&RawReadLock);
#endif    

    for(i=0; i<numpkts; i++)
    {
        pbuf = &(vaddr[i]);
        /* Allocate a buffer. DMA driver will map to PCI space. */
        bufVA = AllocBuf(&RxBufs);
        log_verbose(KERN_INFO "myGetRxPkt: The buffer after alloc is at address %x size %d\n",
                            (u32) bufVA, (u32) BUFSIZE);
        if (bufVA == NULL)
        {
            log_normal(KERN_ERR "RX: AllocBuf failed\n");
            break;
        }

        pbuf->pktBuf = bufVA;
        pbuf->bufInfo = bufVA;
        pbuf->size = BUFSIZE;
    }

#ifdef WY_NEW_LOCK
//  	spin_unlock_bh(&RawLock);
  	spin_unlock_bh(&RawReadLock);
#else
    is_read_busy = false;
#endif

    log_verbose(KERN_INFO "Requested %d, allocated %d buffers\n", numpkts, i);
    return i;
}

int myPutTxPkt(void * hndl, PktBuf * vaddr, int numpkts, unsigned int privdata)
{
    int nomore=0;
    int i;
    unsigned int flags;

    log_verbose(KERN_INFO "Reached myPutTxPkt with handle %p, numpkts %d, privdata %x\n",
                hndl, numpkts, privdata);

    /* Check driver state */
    if(DriverState != REGISTERED)
    {
        printk("Driver does not seem to be ready\n");
        return -1;
    }

    /* Check handle value */
    if(hndl != handle[0])
    {
        printk("PutTxPkt: Came with wrong handle\n");
        return -1;
    }
#ifdef WY_TEST
		else
		{
        printk("PutTxPkt: Came with correct handle\n");
		}
#endif

    /* Just check if we are on the way out */
    for(i=0; i<numpkts; i++)
    {
        flags = vaddr->flags;
        //printk("TX pkt flags %x\n", flags);
        if(flags & PKT_UNUSED)
        {
            nomore = 1;
            break;
        }
        vaddr++;
    }

    spin_lock_bh(&RawLock);

    /* Return packet buffer to free pool */
    //printk("PutTxPkt: Freed %d packets nomore %d\n", numpkts, nomore);
    if(nomore)
        FreeUnusedBuf(&TxBufs, numpkts);
    else
        FreeUsedBuf(&TxBufs, numpkts);
    spin_unlock_bh(&RawLock);

    return 0;
}

int mySetState(void * hndl, UserState * ustate, unsigned int privdata)
{
    int val;
    static unsigned int testmode;

    log_verbose(KERN_INFO "Reached mySetState with privdata %x\n", privdata);

    /* Check driver state */
    if(DriverState != REGISTERED)
    {
        printk("Driver does not seem to be ready\n");
        return EFAULT;
    }

    /* Check handle value */
    if((hndl != handle[0]) && (hndl != handle[2]))
    {
        printk("SetState: Came with wrong handle\n");
        return EBADF;
    }

    /* Valid only for TX engine */
    if(privdata == 0x54545454)
    {
        spin_lock_bh(&RawLock);

        /* Set up the value to be written into the register */
        RawTestMode = ustate->TestMode;

        if(RawTestMode & TEST_START)
        {
            testmode = 0;
            if(RawTestMode & ENABLE_LOOPBACK) testmode |= LOOPBACK;
#ifndef XAUI
            if(RawTestMode & ENABLE_PKTCHK) testmode |= PKTCHKR;
            if(RawTestMode & ENABLE_PKTGEN) testmode |= PKTGENR;
#endif
        }
        else
        {
            /* Deliberately not clearing the loopback bit, incase a
             * loopback test was going on - allows the loopback path
             * to drain off packets. Just stopping the source of packets.
             */
#ifndef XAUI
            if(RawTestMode & ENABLE_PKTCHK) testmode &= ~PKTCHKR;
            if(RawTestMode & ENABLE_PKTGEN) testmode &= ~PKTGENR;
#endif
        }

        printk("SetState TX with RawTestMode %x, reg value %x\n",
                                                    RawTestMode, testmode);

        /* Now write the registers */
        if(RawTestMode & TEST_START)
        {
#if 0        
#ifndef XAUI
            if(!(RawTestMode & (ENABLE_PKTCHK|ENABLE_PKTGEN|ENABLE_LOOPBACK)))
            {
                printk("%s Driver: TX Test Start with wrong mode %x\n",
                                                MYNAME, testmode);
                RawTestMode = 0;
                spin_unlock_bh(&RawLock);
                return EBADRQC;
            }
#endif
#endif

            printk("%s Driver: Starting the test - mode %x, reg %x\n",
                                            MYNAME, RawTestMode, testmode);

            /* Next, set packet sizes. Ensure they don't exceed PKTSIZEs */
            RawMinPktSize = ustate->MinPktSize;
            RawMaxPktSize = ustate->MaxPktSize;

#ifndef XAUI
            /* Set RX packet size for memory path */
            val = RawMaxPktSize;
            if(val % BYTEMULTIPLE)
                val -= (val % BYTEMULTIPLE);
            printk("Reg %x = %x\n", PKT_SIZE_ADDRESS, val);
            RawMinPktSize = RawMaxPktSize = val;

            /* Now ensure the sizes remain within bounds */
            if(RawMaxPktSize > MAXPKTSIZE)
                RawMinPktSize = RawMaxPktSize = MAXPKTSIZE;
            if(RawMinPktSize < MINPKTSIZE)
                RawMinPktSize = RawMaxPktSize = MINPKTSIZE;
            if(RawMinPktSize > RawMaxPktSize)
                RawMinPktSize = RawMaxPktSize;
            val = RawMaxPktSize;

            printk("========Reg %x = %d\n", PKT_SIZE_ADDRESS, val);
            XIo_Out32(TXbarbase+PKT_SIZE_ADDRESS, val);
            printk("RxPktSize %d\n", val);
#else
            /* Now ensure the sizes remain within bounds */
            if(RawMaxPktSize > MAXPKTSIZE)
                RawMaxPktSize = MAXPKTSIZE;
            if(RawMinPktSize < MINPKTSIZE)
                RawMinPktSize = MINPKTSIZE;
            if(RawMinPktSize > RawMaxPktSize)
                RawMinPktSize = RawMaxPktSize;

            printk("MinPktSize %d MaxPktSize %d\n",
                                RawMinPktSize, RawMaxPktSize);
#endif

/* Incase the last test was a loopback test, that bit may not be cleared. */
            XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, 0);
            if(RawTestMode & (ENABLE_PKTCHK|ENABLE_LOOPBACK))
            {
                TxSeqNo = 0;
#ifdef XAUI
                RxSeqNo = 0;
#endif
                if(RawTestMode & ENABLE_LOOPBACK)
                    RxSeqNo = 0;
                printk("========Reg %x = %x\n", TX_CONFIG_ADDRESS, testmode);
                XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, testmode);
            }
#ifndef XAUI
            if(RawTestMode & ENABLE_PKTGEN)
            {
                RxSeqNo = 0;
                printk("========Reg %x = %x\n", RX_CONFIG_ADDRESS, testmode);
                XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, testmode);
            }
#endif

#ifdef XAUI
            /* Wait for the link status to be established */
            mdelay(300);

            /* Now, check if the link status is fine */
            val = XIo_In32(TXbarbase+LINK_STATUS_ADDRESS);
            printk("Link status is %x\n", val);
            if(!(val & (RX_LINK_UP|RX_ALIGNED)))
            {
                printk(KERN_ERR "Link status is down %x\n", val);
                XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, 0);
                RawTestMode = 0;
                spin_unlock_bh(&RawLock);
                return ENOLINK;
            }
#endif
        }
        /* Else, stop the test. Do not remove any loopback here because
         * the DMA queues and hardware FIFOs must drain first.
         */
        else
        {
            printk("%s Driver: Stopping the test, mode %x\n", MYNAME, testmode);
            printk("========Reg %x = %x\n", TX_CONFIG_ADDRESS, testmode);
            XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, testmode);
#ifndef XAUI
            printk("========Reg %x = %x\n", RX_CONFIG_ADDRESS, testmode);
            XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, testmode);
#endif

            /* Not resetting sequence numbers here - causes problems
             * in debugging. Instead, reset the sequence numbers when
             * starting a test.
             */
        }

        PrintSummary();
        spin_unlock_bh(&RawLock);
    }

    return 0;
}

int myGetState(void * hndl, UserState * ustate, unsigned int privdata)
{
    static int iter=0;

    log_verbose("Reached myGetState with privdata %x\n", privdata);

    /* Same state is being returned for both engines */

    ustate->LinkState = LINK_UP;
    ustate->Errors = 0;
    ustate->MinPktSize = RawMinPktSize;
    ustate->MaxPktSize = RawMaxPktSize;
    ustate->TestMode = RawTestMode;
    if(privdata == 0x54545454)
        ustate->Buffers = TxBufs.TotalNum;
    else
        ustate->Buffers = RxBufs.TotalNum;

    if(iter++ >= 4)
    {
        PrintSummary();

        iter = 0;
    }

    return 0;
}

#ifndef USER_DATA
static int DmaSetupTransmit(void * hndl, int num)
{
    int i, result;
    static int pktsize=0;
    int total, bufsize, fragment;
    int bufindex;
    unsigned char * bufVA;
    PktBuf * pbuf;
    int origseqno;
    //static unsigned short lastno=0;

    log_verbose("Reached DmaSetupTransmit with handle %p, num %d\n", hndl, num);

    /* Check driver state */
    if(DriverState != REGISTERED)
    {
        printk("Driver does not seem to be ready\n");
        return 0;
    }

    /* Check handle value */
    if(hndl != handle[0])
    {
        printk("Came with wrong handle\n");
        return 0;
    }

    /* Check number of packets */
    if(!num)
    {
        printk("Came with 0 packets for sending\n");
        return 0;
    }

    /* Hold the spinlock only when calling the buffer management APIs. */
    spin_lock_bh(&RawLock);
    origseqno = TxSeqNo;
    for(i=0, bufindex=0; i<num; i++)            /* Total packets loop */
    {
        //printk("i %d bufindex %d\n", i, bufindex);

#ifdef XAUI
        /* Generate a random number in-between min and max */
        if(RawMinPktSize != RawMaxPktSize)
        {
            pktsize += BYTEMULTIPLE;
            if(pktsize % BYTEMULTIPLE)
                pktsize -= (pktsize % BYTEMULTIPLE);
            if(pktsize < RawMinPktSize) pktsize = RawMinPktSize;
            if(pktsize > RawMaxPktSize) pktsize = RawMaxPktSize;
        }
        else
#endif
            /* Fix the packet size to be the maximum entered in GUI */
            pktsize = RawMaxPktSize;

        //printk("pktsize is %d\n", pktsize);

        total = 0;
        fragment = 0;
        while(total < pktsize)      /* Packet fragments loop */
        {
            //printk("Buf loop total %d bufindex %d\n", total, bufindex);

            pbuf = &(pkts[bufindex]);

            /* Allocate a buffer. DMA driver will map to PCI space. */
            bufVA = AllocBuf(&TxBufs);

            log_verbose(KERN_INFO "TX: The buffer after alloc is at address %x size %d\n",
                                (u32) bufVA, (u32) BUFSIZE);
            if (bufVA == NULL)
            {
                //printk("TX: AllocBuf failed\n");
                //printk("[%d]", (num-i-1));
                break;
            }
            pbuf->pktBuf = bufVA;
            pbuf->bufInfo = bufVA;
            bufsize = ((total + BUFSIZE) > pktsize) ?
                                        (pktsize - total) : BUFSIZE ;
            total += bufsize;

            //printk("bufsize %d total %d\n", bufsize, total);

            log_verbose(KERN_INFO "Calling FormatBuffer pktsize %d bufsize %d fragment %d\n",
                                pktsize, bufsize, fragment);
            FormatBuffer(bufVA, pktsize, bufsize, fragment);

            pbuf->size = bufsize;
            pbuf->userInfo = TxSeqNo;
            pbuf->flags = PKT_ALL;
            if(!fragment)
                pbuf->flags |= PKT_SOP;
            if(total == pktsize)
            {
                pbuf->flags |= PKT_EOP;
#ifdef XAUI
                pbuf->size = bufsize - 4;
#endif
            }

            //printk("flags %x\n", pbuf->flags);
            //printk("TxSeqNo %u\n", TxSeqNo);

            bufindex++;
            fragment++;
        }
        if(total < pktsize)
        {
            /* There must have been some error in the middle of the packet */

            if(fragment)
            {
                /* First, adjust the number of buffers to queue up or else
                 * partial packets will get transmitted, which will cause a
                 * problem later.
                 */
                bufindex -= fragment;

                /* Now, free any unused buffers from the partial packet, so
                 * that buffers are not lost.
                 */
                log_normal(KERN_ERR "Tried to send pkt of size %d, only %d fragments possible\n",
                                                pktsize, fragment);
                FreeUnusedBuf(&TxBufs, fragment);
            }
            break;
        }

#ifdef XAUI
        /* Reset size so that next time it starts from a low value */
        if(pktsize >= RawMaxPktSize) pktsize = 0;
#endif

        /* Increment packet sequence number */
        //if(lastno != TxSeqNo) printk(" %u-%u.", lastno, TxSeqNo);
        TxSeqNo++;
        //lastno = TxSeqNo;
    }
    spin_unlock_bh(&RawLock);

    //printk("[p%d-%d-%d] ", num, i, bufindex);

    if(i == 0)
        /* No buffers available */
        return 0;

    log_verbose("%s: Sending packet length %d seqno %d\n",
                                        MYNAME, pktsize, TxSeqNo);
    result = DmaSendPkt(hndl, pkts, bufindex);
    TxBufCnt += result;
    if(result != bufindex)
    {
        log_normal(KERN_ERR "Tried to send %d pkts in %d buffers, sent only %d\n",
                                    num, bufindex, result);
        //printk("[s%d-%d,%d-%d]", bufindex, result, TxSeqNo, origseqno);
        if(result) TxSeqNo = pkts[result].userInfo;
        else TxSeqNo = origseqno;
        //printk("-%u-", TxSeqNo);
        //lastno = TxSeqNo;

        spin_lock_bh(&RawLock);
        FreeUnusedBuf(&TxBufs, (bufindex-result));
        spin_unlock_bh(&RawLock);
        return 0;
    }
    else return 1;
}
#endif

static int __init rawdata_init(void)
{
    dev_t rawdataDev;  /* Just register the driver. No kernel boot options used. */
    static struct file_operations rawdataDevFileOps;
    int chrRet;

    printk(KERN_INFO "%s Init: Inserting Xilinx driver in kernel.\n",
                                        MYNAME);

    DriverState = INITIALIZED;
    spin_lock_init(&RawLock);

    /* First allocate the buffer pool and set the driver state
     * because GetPkt routine can potentially be called immediately
     * after Register is done.
     */
    printk("PAGE_SIZE is %ld\n", PAGE_SIZE);
    msleep(5);

    InitBuffers(&TxBufs);
    InitBuffers(&RxBufs);

    rawdataDev = 0;
    // Register a char device number
    chrRet = alloc_chrdev_region(&rawdataDev, 0, 1, "ml605_raw_data");
    if (chrRet < 0)
    {
      log_normal(KERN_ERR "Error allocating ml605_raw_data char device region\n");
    }
    else
    {
      // Allocate a cdev structure
      rawdataCdev = cdev_alloc();
      if (IS_ERR(rawdataCdev))
      {
        log_normal(KERN_ERR "Alloc error registering ml605_raw_data device driver\n");
        // Return the device number
        unregister_chrdev_region(rawdataDev, 1);
        chrRet = -1;
      }
      else
      {
        rawdataDevFileOps.owner = THIS_MODULE;
        rawdataDevFileOps.read = rawdata_dev_read;
        rawdataDevFileOps.write = rawdata_dev_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
        rawdataDevFileOps.ioctl = rawdata_dev_ioctl;
#else
        rawdataDevFileOps.unlocked_ioctl = rawdata_dev_ioctl;
#endif
        rawdataDevFileOps.open = rawdata_dev_open;
        rawdataDevFileOps.release = rawdata_dev_release;

        rawdataCdev->owner = THIS_MODULE;
        rawdataCdev->ops = &rawdataDevFileOps;
        rawdataCdev->dev = rawdataDev;

        // Add the char device (rawdata) to system
        chrRet = cdev_add(rawdataCdev, rawdataDev, 1);
        if (chrRet < 0)
        {
          log_normal(KERN_ERR "Add error registering ml605_raw_data device driver\n");
          unregister_chrdev_region(rawdataDev, 1);
        }
      }
    }

    /* Start polling routine - change later !!!! */
    printk(KERN_INFO "%s Init: Starting poll routine with %x\n",
                                        MYNAME, polldata);
    init_timer(&poll_timer);
    poll_timer.expires=jiffies+(HZ/5);
    poll_timer.data=(unsigned long) polldata;
    poll_timer.function = poll_routine;
    add_timer(&poll_timer);

    return 0;
}

static void __exit rawdata_cleanup(void)
{
    int i;

    /* Stop the polling routine */
    del_timer_sync(&poll_timer);
    //DriverState = CLOSED;

    /* Stop any running tests, else the hardware's packet checker &
     * generator will continue to run.
     */
    XIo_Out32(TXbarbase+TX_CONFIG_ADDRESS, 0);

#ifndef XAUI
    XIo_Out32(TXbarbase+RX_CONFIG_ADDRESS, 0);
#endif

    printk(KERN_INFO "%s: Unregistering Xilinx driver from kernel.\n", MYNAME);
    if (TxBufCnt != RxBufCnt)
    {
        printk("%s: Buffers Transmitted %u Received %u\n", MYNAME, TxBufCnt, RxBufCnt);
        printk("TxSeqNo = %u, RxSeqNo = %u\n", TxSeqNo, RxSeqNo);
        mdelay(1);
    }
    DmaUnregister(handle[0]);
    DmaUnregister(handle[2]);

    PrintSummary();

    mdelay(1000);

    /* Not sure if free_page() sleeps or not. */
    spin_lock_bh(&RawLock);
    printk("Freeing user buffers\n");
    for(i=0; i<TxBufs.TotalNum; i++)
        //kfree(TxBufs.origVA[i]);
        free_page((unsigned long)(TxBufs.origVA[i]));
    for(i=0; i<RxBufs.TotalNum; i++)
        //kfree(RxBufs.origVA[i]);
        free_page((unsigned long)(RxBufs.origVA[i]));
    spin_unlock_bh(&RawLock);

    if(rawdataCdev != NULL)
    {
        printk("Unregistering rawdata char device driver\n");
        cdev_del(rawdataCdev);
        unregister_chrdev_region(rawdataCdev->dev, 1);
    }
}

module_init(rawdata_init);
module_exit(rawdata_cleanup);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");

