static void IntrBH(unsigned long unused)
{
    struct pci_dev *pdev;
    struct privData *lp;
    Dma_Engine * eptr;
    unsigned long flags;
    int i;

    pdev = dmaData->pdev;
    lp = pci_get_drvdata(pdev);

    log_verbose("IntrBH with PendingMask %llx\n", PendingMask);

    //while(PendingMask)
    for(i=0; PendingMask && i<MAX_DMA_ENGINES; i++)
    {
        if(!(PendingMask & (1LL << i))) continue;
        spin_lock_irqsave(&IntrLock, flags);

        /* At this point, we have engine identified. */

        /* First, reset mask bit */
        PendingMask &= ~(1LL << i);

        spin_unlock_irqrestore(&IntrLock, flags);

        eptr = &(lp->Dma[i]);
        if(eptr->EngineState != USER_ASSIGNED)      // Should not happen
            continue;

        /* The spinlocks need to be handled within this function, so
         * don't do them here.
         */

        PktHandler(i, eptr);

        spin_lock_irqsave(&IntrLock, flags);
        Dma_mEngIntEnable(eptr);

        /* Update flag to synchronise between ISR and poll_routine */
        LastIntr[i] = jiffies;
        spin_unlock_irqrestore(&IntrLock, flags);
    }
}



u32 Acks(u32 dirqval)
{
    u32 retval=0;

    retval |= (dirqval & DMA_ENG_ENABLE_MASK) ? DMA_ENG_ENABLE : 0;
    retval |= (dirqval & DMA_ENG_INT_ACTIVE_MASK) ? DMA_ENG_INT_ACK : 0;
    retval |= (dirqval & DMA_ENG_INT_BDCOMP) ? DMA_ENG_INT_BDCOMP_ACK : 0;
    retval |= (dirqval & DMA_ENG_INT_ALERR) ? DMA_ENG_INT_ALERR_ACK : 0;
    retval |= (dirqval & DMA_ENG_INT_FETERR) ? DMA_ENG_INT_FETERR_ACK : 0;
    retval |= (dirqval & DMA_ENG_INT_ABORTERR) ? DMA_ENG_INT_ABORTERR_ACK : 0;
    retval |= (dirqval & DMA_ENG_INT_CHAINEND) ? DMA_ENG_INT_CHAINEND_ACK : 0;

    if(dirqval & DMA_ENG_INT_ACTIVE_MASK)
        retval &= ~(DMA_ENG_INT_ENABLE);
    \
    log_verbose(KERN_INFO "Acking %x with %x\n", dirqval, retval);
    return retval;
}

/* This function serves to handle the initial interrupt, as well as to
 * check again on pending interrupts, from the BH. If this is not done,
 * interrupts can stall.
 */
int IntrCheck(struct pci_dev * dev) //guodebug: error prone
{
    u32 girqval, dirqval;
    struct privData *lp;
    u32 base, imask;
    Dma_Engine * eptr;
    int i, retval=XST_FAILURE;
    static int count0=0, count1=0, count2=0, count3=0;

    lp = pci_get_drvdata(dev);
    log_verbose(KERN_INFO "IntrCheck: device %x\n", (u32) dev);

    base = (u32)(lp->barInfo[0].baseVAddr); //error
    girqval = Dma_mReadReg(base, REG_DMA_CTRL_STATUS);
    //if(!(girqval & (DMA_INT_ACTIVE_MASK|DMA_INT_PENDING_MASK|DMA_USER_INT_ACTIVE_MASK)))
    //if(!(girqval & (DMA_INT_ACTIVE_MASK|DMA_USER_INT_ACTIVE_MASK)))
    //    return;

    /* Now, check each S2C DMA engine (0 to 7) */
    imask = (girqval & DMA_S2C_ENG_INT_VAL) >> 16;
    for(i=0; i<7; i++)
    {
        if(!imask) break;
        if(!(imask & (0x01<<i))) continue;

        if(!((lp->engineMask) & (1LL << i)))
            continue;

        eptr = &(lp->Dma[i]);

        dirqval = Dma_mGetCrSr(eptr);
        log_verbose("Eng %d: dirqval is %x\n", i, dirqval);

        /* Check whether interrupt is enabled, otherwise it could be a
         * re-check of the last checked engine before its bottom half has run.
         */
        if((dirqval & DMA_ENG_INT_ACTIVE_MASK) &&
                (dirqval & DMA_ENG_INT_ENABLE))
        {
            spin_lock(&IntrLock);
            //Dma_mEngIntAck(eptr, Acks(dirqval));
            Dma_mSetCrSr(eptr, Acks(dirqval));
            \

            if(dirqval & (DMA_ENG_INT_ALERR|DMA_ENG_INT_FETERR|DMA_ENG_INT_ABORTERR))
                printk("Eng %d: Came with error dirqval %x\n", i, dirqval);
            if(dirqval & DMA_ENG_INT_BDCOMP)
            {
                //Dma_mEngIntDisable(eptr); // Already disabled
                PendingMask |= (1LL << i);
            }
            spin_unlock(&IntrLock);

            if(i==0) count0++;
            else if(i==1) count1++;
            retval = XST_SUCCESS;
        }
        else if(dirqval & DMA_ENG_INT_ACTIVE_MASK) log_normal("1");
    }

    /* Now, check each C2S DMA engine (32 to 39) */
    imask = (girqval & DMA_C2S_ENG_INT_VAL) >> 24;
    for(i=32; i<39; i++)
    {
        if(!imask) break;
        if(!(imask & (0x01<<(i-32)))) continue;

        if(!((lp->engineMask) & (1LL << i)))
            continue;

        eptr = &(lp->Dma[i]);

        dirqval = Dma_mGetCrSr(eptr);
        log_verbose("Eng %d: dirqval is %x\n", i, dirqval);

        /* Check whether interrupt is enabled, otherwise it could be a
         * re-check of the last checked engine before its bottom half has run.
         */
        if((dirqval & DMA_ENG_INT_ACTIVE_MASK) &&
                (dirqval & DMA_ENG_INT_ENABLE))
        {
            spin_lock(&IntrLock);
            Dma_mEngIntAck(eptr, Acks(dirqval));

            if(dirqval & (DMA_ENG_INT_ALERR|DMA_ENG_INT_FETERR|DMA_ENG_INT_ABORTERR))
                printk("Eng %d: Came with error dirqval %x\n", i, dirqval);
            if(dirqval & DMA_ENG_INT_BDCOMP)
            {
                Dma_mEngIntDisable(eptr);
                PendingMask |= (1LL << i);
            }
            spin_unlock(&IntrLock);

            if(i==32) count2++;
            else if(i==33) count3++;
            retval = XST_SUCCESS;
        }
        else if(dirqval & DMA_ENG_INT_ACTIVE_MASK) log_normal("~");
    }

    spin_lock(&IntrLock);
    if(PendingMask && (retval == XST_SUCCESS))
    {
        tasklet_schedule(&DmaBH);
    }
    spin_unlock(&IntrLock);

    return retval;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t DmaInterrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t DmaInterrupt(int irq, void *dev_id)
#endif
{
    struct pci_dev *dev = dev_id;

    /* Handle DMA and any user interrupts */
    if(IntrCheck(dev) == XST_SUCCESS)
        return IRQ_HANDLED;
    else
        return IRQ_NONE;
}



