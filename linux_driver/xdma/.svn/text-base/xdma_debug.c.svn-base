
#ifdef DEBUG_VERBOSE

void disp_bd_ring(Dma_BdRing *bd_ring)
{
    int num_bds = bd_ring->AllCnt;
    u32 *dptr ;
    int idx;

    /*
      printk("ChanBase: %p\n", (void *) bd_ring->ChanBase);
      printk("FirstBdPhysAddr: %p\n", (void *) bd_ring->FirstBdPhysAddr);
      printk("FirstBdAddr: %p\n", (void *) bd_ring->FirstBdAddr);
      printk("LastBdAddr: %p\n", (void *) bd_ring->LastBdAddr);
      printk("Length: %d (0x%0x)\n", bd_ring->Length, bd_ring->Length);
      printk("RunState: %d (0x%0x)\n", bd_ring->RunState, bd_ring->RunState);
      printk("Separation: %d (0x%0x)\n", bd_ring->Separation,
             bd_ring->Separation);
      printk("BD Count: %d\n", bd_ring->AllCnt);

      printk("\n");

      printk("FreeHead: %p\n", (void *) bd_ring->FreeHead);
      printk("PreHead: %p\n", (void *) bd_ring->PreHead);
      printk("HwHead: %p\n", (void *) bd_ring->HwHead);
      printk("HwTail: %p\n", (void *) bd_ring->HwTail);
      printk("PostHead: %p\n", (void *) bd_ring->PostHead);
      printk("BdaRestart: %p\n", (void *) bd_ring->BdaRestart);
    */
    printk("Ring %p Contents:\n", bd_ring);
    printk("Idx Status / UStatusL UStatusH  CAddrL  Control/ SysAddrL SysAddrH NextBD\n");
    printk("      BC                               CAddrH/BC \n");
    printk("--- -------- -------- -------- -------- -------- -------- -------- --------\n");

    dptr = (u32 *)bd_ring->FirstBdAddr;
    for (idx = 0; idx < num_bds; idx++)
    {
        int i;
        printk("%3d ", idx);
        for(i=0; i<8; i++)
        {
            printk("%08x ", *dptr);
            dptr++;
        }

        printk("\n");
        printk("    ");

        for(i=0; i<8; i++)
        {
            printk("%08x ", *dptr);
            dptr++;
        }

        printk("\n");
    }

    printk("--------------------------------------- Done ---------------------------------------\n");
}

#endif

#if defined(DEBUG_VERBOSE) || defined(DEBUG_NORMAL)
static void ReadConfig(struct pci_dev * pdev)
{
    int i;
    u8 valb;
    u16 valw;
    u32 valdw;
    unsigned long reg_base, reg_len;

    /* Read PCI configuration space */
    printk(KERN_INFO "PCI Configuration Space:\n");
    for(i=0; i<0x40; i++)
    {
        pci_read_config_byte(pdev, i, &valb);
        printk("0x%x ", valb);
        if((i % 0x10) == 0xf)
            printk("\n");
    }
    printk("\n");

    /* Now read each element - one at a time */

    /* Read Vendor ID */
    pci_read_config_word(pdev, PCI_VENDOR_ID, &valw);
    printk("Vendor ID: 0x%x, ", valw);

    /* Read Device ID */
    pci_read_config_word(pdev, PCI_DEVICE_ID, &valw);
    printk("Device ID: 0x%x, ", valw);

    /* Read Command Register */
    pci_read_config_word(pdev, PCI_COMMAND, &valw);
    printk("Cmd Reg: 0x%x, ", valw);

    /* Read Status Register */
    pci_read_config_word(pdev, PCI_STATUS, &valw);
    printk("Stat Reg: 0x%x, ", valw);

    /* Read Revision ID */
    pci_read_config_byte(pdev, PCI_REVISION_ID, &valb);
    printk("Revision ID: 0x%x, ", valb);

    /* Read Class Code */
    /*
      pci_read_config_dword(pdev, PCI_CLASS_PROG, &valdw);
      printk("Class Code: 0x%lx, ", valdw);
      valdw &= 0x00ffffff;
      printk("Class Code: 0x%lx, ", valdw);
    */
    /* Read Reg-level Programming Interface */
    pci_read_config_byte(pdev, PCI_CLASS_PROG, &valb);
    printk("Class Prog: 0x%x, ", valb);

    /* Read Device Class */
    pci_read_config_word(pdev, PCI_CLASS_DEVICE, &valw);
    printk("Device Class: 0x%x, ", valw);

    /* Read Cache Line */
    pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &valb);
    printk("Cache Line Size: 0x%x, ", valb);

    /* Read Latency Timer */
    pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &valb);
    printk("Latency Timer: 0x%x, ", valb);

    /* Read Header Type */
    pci_read_config_byte(pdev, PCI_HEADER_TYPE, &valb);
    printk("Header Type: 0x%x, ", valb);

    /* Read BIST */
    pci_read_config_byte(pdev, PCI_BIST, &valb);
    printk("BIST: 0x%x\n", valb);

    /* Read all 6 BAR registers */
    for(i = 0; i <= 5; i++)
    {
        /* Physical address & length */
        reg_base  =  pci_resource_start (pdev, i);
        reg_len   =  pci_resource_len   (pdev, i);
        printk("BAR%d: Addr:0x%lx Len:0x%lx,  ", i, reg_base, reg_len);

        /* Flags */
        if((pci_resource_flags(pdev, i) & IORESOURCE_MEM))
            printk("Region is for memory\n");
        else if((pci_resource_flags(pdev, i) & IORESOURCE_IO))
            printk("Region is for I/O\n");
    }
    printk("\n");

    /* Read CIS Pointer */
    pci_read_config_dword(pdev, PCI_CARDBUS_CIS, &valdw);
    printk("CardBus CIS Pointer: 0x%x, ", valdw);

    /* Read Subsystem Vendor ID */
    pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &valw);
    printk("Subsystem Vendor ID: 0x%x, ", valw);

    /* Read Subsystem Device ID */
    pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &valw);
    printk("Subsystem Device ID: 0x%x\n", valw);

    /* Read Expansion ROM Base Address */
    pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &valdw);
    printk("Expansion ROM Base Address: 0x%x\n", valdw);

    /* Read IRQ Line */
    pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &valb);
    printk("IRQ Line: 0x%x, ", valb);

    /* Read IRQ Pin */
    pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &valb);
    printk("IRQ Pin: 0x%x, ", valb);

    /* Read Min Gnt */
    pci_read_config_byte(pdev, PCI_MIN_GNT, &valb);
    printk("Min Gnt: 0x%x, ", valb);

    /* Read Max Lat */
    pci_read_config_byte(pdev, PCI_MAX_LAT, &valb);
    printk("Max Lat: 0x%x\n", valb);
}
#endif

#ifdef DEBUG_VERBOSE
static void ReadRoot(struct pci_dev * pdev)
{
    int i;
    u8 valb;
    struct pci_bus * parent;
    struct pci_bus * me;

    /* Read PCI configuration space for all devices on this bus */
    parent = pdev->bus->parent;
    for(i=0; i<256; i++)
    {
        pci_bus_read_config_byte(parent, 8, i, &valb);
        printk("%02x ", valb);
        if(!((i+1) % 16)) printk("\n");
    }

    printk("Device %p details:\n", pdev);
    printk("Bus_list %p\n", &(pdev->bus_list));
    printk("Bus %p\n", pdev->bus);
    printk("Subordinate %p\n", pdev->subordinate);
    printk("Sysdata %p\n", pdev->sysdata);
    printk("Procent %p\n", pdev->procent);
    printk("Devfn %d\n", pdev->devfn);
    printk("Vendor %x\n", pdev->vendor);
    printk("Device %x\n", pdev->device);
    printk("Subsystem_vendor %x\n", pdev->subsystem_vendor);
    printk("Subsystem_device %x\n", pdev->subsystem_device);
    printk("Class %d\n", pdev->class);
    printk("Hdr_type %d\n", pdev->hdr_type);
    printk("Rom_base_reg %d\n", pdev->rom_base_reg);
    printk("Pin %d\n", pdev->pin);
    printk("Driver %p\n", pdev->driver);
    printk("Dma_mask %lx\n", (unsigned long)(pdev->dma_mask));
    printk("Vendor_compatible: ");
    //for(i=0; i<DEVICE_COUNT_COMPATIBLE; i++)
    //  printk("%x ", pdev->vendor_compatible[i]);
    //printk("\n");
    //printk("Device_compatible: ");
    //for(i=0; i<DEVICE_COUNT_COMPATIBLE; i++)
    //  printk("%x ", pdev->device_compatible[i]);
    //printk("\n");
    printk("Cfg_size %d\n", pdev->cfg_size);
    printk("Irq %d\n", pdev->irq);
    printk("Transparent %d\n", pdev->transparent);
    printk("Multifunction %d\n", pdev->multifunction);
    //printk("Is_enabled %d\n", pdev->is_enabled);
    printk("Is_busmaster %d\n", pdev->is_busmaster);
    printk("No_msi %d\n", pdev->no_msi);
    printk("No_dld2 %d\n", pdev->no_d1d2);
    printk("Block_ucfg_access %d\n", pdev->block_ucfg_access);
    printk("Broken_parity_status %d\n", pdev->broken_parity_status);
    printk("Msi_enabled %d\n", pdev->msi_enabled);
    printk("Msix_enabled %d\n", pdev->msix_enabled);
    printk("Rom_attr_enabled %d\n", pdev->rom_attr_enabled);

    me = pdev->bus;
    printk("Bus details:\n");
    printk("Parent %p\n", me->parent);
    printk("Children %p\n", &(me->children));
    printk("Devices %p\n", &(me->devices));
    printk("Self %p\n", me->self);
    printk("Sysdata %p\n", me->sysdata);
    printk("Procdir %p\n", me->procdir);
    printk("Number %d\n", me->number);
    printk("Primary %d\n", me->primary);
    printk("Secondary %d\n", me->secondary);
    printk("Subordinate %d\n", me->subordinate);
    printk("Name %s\n", me->name);
    printk("Bridge_ctl %d\n", me->bridge_ctl);
    printk("Bridge %p\n", me->bridge);
}
#endif

