#ifndef DMA_BDRING_H    /* prevent circular inclusions */
#define DMA_BDRING_H    /* by using protection macros */

#ifdef __cplusplus
extern "C" {
#endif

#include "xdma_bd.h"

    /**************************** Type Definitions *******************************/

    /** Container structure for buffer descriptor (BD) storage. All addresses
     * and pointers excluding FirstBdPhysAddr are expressed in terms of the
     * virtual address.
     *
     * The BD ring is not arranged as a conventional linked list. Instead,
     * a separation value is added to navigate to the next BD, since all the
     * BDs are contiguously placed, and the ring is placed from lower memory
     * to higher memory and back.
     */
    typedef struct {
        Xaddr ChanBase;        //guodebug
        u32 IsRxChannel;        /**< Is this a receive channel ? */
        u32 RunState;         /**< Flag to indicate state of engine/ring */
        Xaddr FirstBdPhysAddr;    /**< Physical address of 1st BD in list, guodebug */
        Xaddr FirstBdAddr;        /**< Virtual address of 1st BD in list, guodebug  */
        Xaddr LastBdAddr;         /**< Virtual address of last BD in the list, guodebug  */
        u32 Length;             /**< Total size of ring in bytes */
        u32 Separation;         /**< Number of bytes between the starting
                               address of adjacent BDs */
        Dma_Bd *FreeHead;       /**< First BD in the free group */
        Dma_Bd *PreHead;        /**< First BD in the pre-work group */
        Dma_Bd *HwHead;         /**< First BD in the work group */
        Dma_Bd *HwTail;         /**< Last BD in the work group */
        Dma_Bd *PostHead;       /**< First BD in the post-work group */
        Dma_Bd *BdaRestart;     /**< BD to load when channel is started */
        u32 FreeCnt;          /**< Number of allocatable BDs in free group */
        u32 PreCnt;             /**< Number of BDs in pre-work group */
        u32 HwCnt;              /**< Number of BDs in work group */
        u32 PostCnt;          /**< Number of BDs in post-work group */
        u32 AllCnt;             /**< Total Number of BDs for channel */

        u32 BDerrs;             /**< Total BD errors reported by DMA */
        u32 BDSerrs;            /**< Total TX BD short errors reported by DMA */
    } Dma_BdRing;


    /***************** Macros (Inline Functions) Definitions *********************/

    /****************************************************************************/
    /**
    * Retrieve the ring object. This object can be used in the various Ring
    * API functions. This could be a C2S or S2C ring.
    *
    * @param  InstancePtr is the DMA engine to operate on.
    *
    * @return BdRing object
    *
    * @note
    * C-style signature:
    *    Dma_BdRing Dma_mGetRing(Dma_Engine * InstancePtr)
    *
    *****************************************************************************/
#define Dma_mGetRing(InstancePtr) ((InstancePtr)->BdRing)


    /****************************************************************************/
    /**
    * Return the total number of BDs allocated by this channel with
    * Dma_BdRingCreate().
    *
    * @param  RingPtr is the BD ring to operate on.
    *
    * @return The total number of BDs allocated for this channel.
    *
    * @note
    * C-style signature:
    *    u32 Dma_mBdRingGetCnt(Dma_BdRing* RingPtr)
    *
    *****************************************************************************/
#define Dma_mBdRingGetCnt(RingPtr) ((RingPtr)->AllCnt)


    /****************************************************************************/
    /**
    * Return the number of BDs allocatable with Dma_BdRingAlloc() for pre-
    * processing.
    *
    * @param  RingPtr is the BD ring to operate on.
    *
    * @return The number of BDs currently allocatable.
    *
    * @note
    * C-style signature:
    *    u32 Dma_mBdRingGetFreeCnt(Dma_BdRing* RingPtr)
    *
    *****************************************************************************/
#define Dma_mBdRingGetFreeCnt(RingPtr)  ((RingPtr)->FreeCnt)

    /* Maintain a separation between start and end of BD ring. This is
     * required because DMA will stall if the two pointers coincide -
     * this will happen whether ring is full or empty.
     */
    /*
    #define Dma_mBdRingGetFreeCnt(RingPtr)  \
        (((RingPtr)->FreeCnt - 2) < 0 ? 0 : ((RingPtr)->FreeCnt - 2))
    */

    /****************************************************************************/
    /**
    * Snap shot the latest BD that a BD ring is processing.
    *
    * @param  RingPtr is the BD ring to operate on.
    *
    * @return None
    *
    * @note
    * C-style signature:
    *    void Dma_mBdRingSnapShotCurrBd(Dma_BdRing* RingPtr)
    *
    *****************************************************************************/
#define Dma_mBdRingSnapShotCurrBd(RingPtr)            \
{                                           \
    (RingPtr)->BdaRestart =                         \
        (Dma_Bd *)Dma_mReadReg((RingPtr)->ChanBase, REG_DMA_ENG_NEXT_BD);   \
}


    /****************************************************************************/
    /**
    * Return the next BD in the ring.
    *
    * @param  RingPtr is the BD ring to operate on.
    * @param  BdPtr is the current BD.
    *
    * @return The next BD in the ring relative to the BdPtr parameter.
    *
    * @note
    * C-style signature:
    *    Dma_Bd *Dma_mBdRingNext(Dma_BdRing* RingPtr, Dma_Bd *BdPtr)
    *
    *****************************************************************************/
#define Dma_mBdRingNext(RingPtr, BdPtr)                 \
    (((Xaddr)(BdPtr) >= (RingPtr)->LastBdAddr) ?          \
      (Dma_Bd*)(RingPtr)->FirstBdAddr :                 \
      (Dma_Bd*)((Xaddr)(BdPtr) + (RingPtr)->Separation))
//guodebug: remove u32


    /****************************************************************************/
    /**
    * Return the previous BD in the ring.
    *
    * @param  InstancePtr is the DMA channel to operate on.
    * @param  BdPtr is the current BD.
    *
    * @return The previous BD in the ring relative to the BdPtr parameter.
    *
    * @note
    * C-style signature:
    *    Dma_Bd *Dma_mBdRingPrev(Dma_BdRing* RingPtr, Dma_Bd *BdPtr)
    *
    *****************************************************************************/
#define Dma_mBdRingPrev(RingPtr, BdPtr)               \
    (((Xaddr)(BdPtr) <= (RingPtr)->FirstBdAddr) ?       \
      (Dma_Bd*)(RingPtr)->LastBdAddr :                \
      (Dma_Bd*)((Xaddr)(BdPtr) - (RingPtr)->Separation))


    /******************************************************************************
     * Move the BdPtr argument ahead an arbitrary number of BDs wrapping around
     * to the beginning of the ring if needed.
     *
     * We know that a wraparound should occur if the new BdPtr is greater than
     * the high address in the ring OR if the new BdPtr crosses the 0xFFFFFFFF
     * to 0 boundary.
     *
     * @param RingPtr is the ring BdPtr appears in
     * @param BdPtr on input is the starting BD position and on output is the
     *        final BD position
     * @param NumBd is the number of BD spaces to increment
     *
     *****************************************************************************/

//guodebug: u32 -> u64
#define Dma_mRingSeekahead(RingPtr, BdPtr, NumBd)         \
  {                   \
    Xaddr Addr = (Xaddr)(BdPtr);            \
                      \
    Addr += ((RingPtr)->Separation * (NumBd));        \
    if ((Addr > (RingPtr)->LastBdAddr) || ((Xaddr)(BdPtr) > Addr))\
    {                 \
      Addr -= (RingPtr)->Length;          \
    }                 \
                      \
    (BdPtr) = (Dma_Bd*)Addr;            \
  }


    /******************************************************************************
     * Move the BdPtr argument backwards an arbitrary number of BDs wrapping
     * around to the end of the ring if needed.
     *
     * We know that a wraparound should occur if the new BdPtr is less than
     * the base address in the ring OR if the new BdPtr crosses the 0xFFFFFFFF
     * to 0 boundary.
     *
     * @param RingPtr is the ring BdPtr appears in
     * @param BdPtr on input is the starting BD position and on output is the
     *        final BD position
     * @param NumBd is the number of BD spaces to increment
     *
     *****************************************************************************/
#define Dma_mRingSeekback(RingPtr, BdPtr, NumBd)            \
  {                                                                     \
    Xaddr Addr = (Xaddr)(BdPtr);              \
                        \
    Addr -= ((RingPtr)->Separation * (NumBd));          \
    if ((Addr < (RingPtr)->FirstBdAddr) || ((Xaddr)(BdPtr) < Addr)) \
    {                   \
      Addr += (RingPtr)->Length;            \
    }                   \
                        \
    (BdPtr) = (Dma_Bd*)Addr;              \
  }


    /************************* Function Prototypes ******************************/

    /*
     * Descriptor ring functions xdma_bdring.c
     */
//guodebug: u32 -> u64
    int Dma_BdRingCreate(Dma_BdRing * RingPtr, Xaddr PhysAddr, Xaddr VirtAddr, u32 Alignment, unsigned BdCount);
    int Dma_BdRingStart(Dma_BdRing * RingPtr);
    int Dma_BdRingAlloc(Dma_BdRing * RingPtr, unsigned NumBd, Dma_Bd ** BdSetPtr);
    int Dma_BdRingUnAlloc(Dma_BdRing * RingPtr, unsigned NumBd, Dma_Bd * BdSetPtr);
    int Dma_BdRingToHw(Dma_BdRing * RingPtr, unsigned NumBd, Dma_Bd * BdSetPtr);
    unsigned Dma_BdRingFromHw(Dma_BdRing * RingPtr, unsigned BdLimit, Dma_Bd ** BdSetPtr);
    unsigned Dma_BdRingForceFromHw(Dma_BdRing * RingPtr, unsigned BdLimit, Dma_Bd ** BdSetPtr);
    int Dma_BdRingFree(Dma_BdRing * RingPtr, unsigned NumBd, Dma_Bd * BdSetPtr);
    int Dma_BdRingCheck(Dma_BdRing * RingPtr);
    u32 Dma_BdRingAlign(Xaddr AllocPtr, u32 Size, u32 Align, u32 * Delta);

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */

