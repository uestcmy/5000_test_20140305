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

#include <xdma_user.h>
#include "xdebug.h"
#include "xio.h"

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

int DriverState = UNINITIALIZED;
struct timer_list poll_timer;
void * handle[4] = {NULL, NULL, NULL, NULL};
Xaddr TXbarbase, RXbarbase;
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
} Buffer;

Buffer TxBufs;
Buffer RxBufs;
PktBuf pkts[NUM_BUFS];

/* For exclusion */
spinlock_t RawLock;

#ifdef XAUI
#define DRIVER_NAME         "xxaui_driver"
#define DRIVER_DESCRIPTION  "Xilinx XAUI Data Driver"
#else
#define DRIVER_NAME         "xrawdata_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data Driver"
#endif

static void poll_routine(unsigned long __opaque);
static void InitBuffers(Buffer * bptr);

static void FormatBuffer(unsigned char * buf, int pktsize, int bufsize, int fragment);
#ifdef DATA_VERIFY
static void VerifyBuffer(unsigned char * buf, int size, unsigned long long uinfo);
#endif

int myInit(Xaddr, unsigned int);
int myFreePkt(void *, unsigned int *, int, unsigned int);
static int DmaSetupTransmit(void *, int);
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

    /* Initialise */
    bptr->TotalNum = bptr->AllocNum = 0;
    bptr->FirstBuf = 0;
    bptr->LastBuf = 0;
    bptr->FreePtr = 0;
    bptr->AllocPtr = 0;

    /* Allocate for TX buffer pool - have not taken care of alignment */
    for(i = 0; i < NUM_BUFS; i++)
    {
        if((bufVA = (unsigned char *)__get_free_pages(GFP_KERNEL, get_order(BUFSIZE))) == NULL)
        {
            printk("InitBuffers: Unable to allocate [%d] TX buffer for data\n", i);
            break;
        }
        bptr->origVA[i] = bufVA;
    }
    printk("Allocated %d buffers from %p\n", i, (void *)bufVA);

    // guodebug if 0
    /* Do the buffer alignment adjustment, if required */
#if 0
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
        //printk("No buffers available to allocate\n");
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
            printk("No more buffers allocated, cannot free anything\n");
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

static void poll_routine(unsigned long __opaque)
{
    struct timer_list *timer = &poll_timer;
    UserPtrs ufuncs;
    int offset = (2*HZ);


    //printk("Came to poll_routine with %x\n", (u32)(__opaque));

    /* Register with DMA incase not already done so */
    if(DriverState < POLLING)
    {
        spin_lock_bh(&RawLock);
        printk("Calling DmaRegister on engine %d and %d\n", ENGINE_TX, ENGINE_RX);
        DriverState = REGISTERED;

        ufuncs.UserInit = myInit;
        ufuncs.UserPutPkt = myPutTxPkt;
        ufuncs.UserSetState = mySetState;
        ufuncs.UserGetState = myGetState;
        ufuncs.privData = 0x54545454;
        spin_unlock_bh(&RawLock);

        if((handle[0] = DmaRegister(ENGINE_TX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
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

        if((handle[2] = DmaRegister(ENGINE_RX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
        {
            printk("Register for engine %d failed. Stopping.\n", ENGINE_RX);
            spin_lock_bh(&RawLock);
            DriverState = UNINITIALIZED;
            spin_unlock_bh(&RawLock);
            return;     // This polling will not happen again.
        }
        printk("Handle for engine %d is %p\n", ENGINE_RX, handle[2]);

        /* Reschedule poll routine */
        timer->expires = jiffies + offset;
        add_timer(timer); //guodebug: add_timer
    }
    else if(DriverState == REGISTERED)
    {
        /* Only if the test mode is set, and only for TX direction */
        if((RawTestMode & TEST_START) && 
           (RawTestMode & (ENABLE_PKTCHK|ENABLE_LOOPBACK)))
        {
            spin_lock_bh(&RawLock);
            /* First change the state */
            RawTestMode &= ~TEST_START;
            RawTestMode |= TEST_IN_PROGRESS;
            spin_unlock_bh(&RawLock);

            /* Now, queue up one packet to start the transmission */
            DmaSetupTransmit(handle[0], 1);

            /* For the first packet, give some gap before the next TX */
            offset = HZ;
        }
        else if(RawTestMode & TEST_IN_PROGRESS)
        {
            int avail;
            int times;

            for(times = 0; times < 9; times++)
            {
                avail = (TxBufs.TotalNum - TxBufs.AllocNum);
                if(avail <= 0) break;

                /* Queue up many packets to continue the transmission */
                if(DmaSetupTransmit(handle[0], 100) == 0) break;
            }

            /* Do the next TX as soon as possible */
            offset = 0;
        }

        /* Reschedule poll routine */
        timer->expires = jiffies + offset;
        add_timer(timer); //guodebug: add_timer
    }
}

int myInit(Xaddr barbase, unsigned int privdata)
{
    log_normal("Reached myInit with barbase %p and privdata %x\n", barbase, privdata);

    spin_lock_bh(&RawLock);
    if(privdata == 0x54545454)  // So that this is done only once
    {
        TXbarbase = barbase;
        log_verbose("guodebug TX: barbase   = %p\n", barbase);
        log_verbose("guodebug TX: TXbarbase = %p\n", TXbarbase);
    }
    else if(privdata == 0x54545456)  // So that this is done only once
    {
        RXbarbase = barbase;
        log_verbose("guodebug RX: barbase   = %x\n", barbase);
        log_verbose("guodebug RX: RXbarbase = %x\n", RXbarbase);
    }
    else
    {
        log_verbose("guodebug: myInit error!\n");
    }

    TxBufCnt = 0; 
    RxBufCnt = 0;
    ErrCnt   = 0;
    TxSeqNo  = RxSeqNo = 0;


    log_verbose("guodebug TXbarbase+TX_CONFIG_ADDRESS = %p\n", TXbarbase+TX_CONFIG_ADDRESS);
    //log_verbose("guodebug TXbarbase+RX_CONFIG_ADDRESS = %p\n", TXbarbase+RX_CONFIG_ADDRESS);

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
        log_normal("Came with wrong handle %x\n", (u32)hndl);
        return -1;
    }

    spin_lock_bh(&RawLock);

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
            RxSeqNo++;
        }
        RxBufCnt++;
        vaddr++;
    }

    /* Return packet buffers to free pool */

    //printk("PutRxPkt: Freeing %d packets unused %d\n", numpkts, unused);
    if(unused)
        FreeUnusedBuf(&RxBufs, numpkts);
    else
        FreeUsedBuf(&RxBufs, numpkts);

    spin_unlock_bh(&RawLock);

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
        printk("Came with wrong handle\n");
        return 0;
    }

    /* Check size value */
    if(size != BUFSIZE)
        printk("myGetRxPkt: Requested size %d does not match mine %d\n",
                                size, (u32)BUFSIZE);

    spin_lock_bh(&RawLock);

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
    spin_unlock_bh(&RawLock);

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
        printk("Came with wrong handle\n");
        return -1;
    }

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
        printk("Came with wrong handle\n");
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

            log_verbose(KERN_INFO "TX: The buffer after alloc is at address %x size %d\n", (u32) bufVA, (u32) BUFSIZE);
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

static int __init rawdata_init(void)
{
  /* Just register the driver. No kernel boot options used. */
  printk(KERN_INFO "%s Init: Inserting Xilinx driver in kernel.\n", MYNAME);

    DriverState = INITIALIZED;
    spin_lock_init(&RawLock);

    /* First allocate the buffer pool and set the driver state
     * because GetPkt routine can potentially be called immediately 
     * after Register is done.
     */
    //printk("PAGE_SIZE is %ld\n", PAGE_SIZE);
    msleep(5);

    InitBuffers(&TxBufs);
    InitBuffers(&RxBufs);

    /* Start polling routine - change later !!!! */

    printk(KERN_INFO "%s Init: Starting poll routine with %x\n", MYNAME, polldata);
    init_timer(&poll_timer);
    poll_timer.expires=jiffies+(HZ/5);
    poll_timer.data = (unsigned long) polldata;
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

    log_verbose("TXbarbase = %p\n", TXbarbase);

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
}

module_init(rawdata_init);
module_exit(rawdata_cleanup);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");


