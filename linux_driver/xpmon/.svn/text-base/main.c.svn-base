int main(int argc, char *argv[])
{
    int i, k, Max;
    char str[16];
    char * str1;
    char * token;
    unsigned int testmode;
    int state;

    /* Set up the GUI's main label */
    strcpy(mainlabel, MAINLABEL);

    /* First find the absolute path of the executable. Required for
     * locating the logo files.
     */
    strcpy(xlnx_logo, argv[0]);
    str1 = strrchr(xlnx_logo, '/');
    if(str1 == NULL)                            // If no slash is found.
        str1 = xlnx_logo;
    else
        str1++;                                 // To include slash.
    *str1 = '\0';
    strcpy(dev_logo, xlnx_logo);
    strcat(xlnx_logo, XLNX_LOGO);
    strcat(dev_logo, DEV_LOGO);
    printf("Logos are: %p %s %p %s\n", xlnx_logo, xlnx_logo, dev_logo, dev_logo);

    /* Open driver device file */
#ifdef HARDWARE
    if((statsfd = open(FILENAME, O_RDONLY)) < 0)
    {
        printf("Failed to open statistics file %s\n", FILENAME);
        return FALSE;
    }
#endif
    get_engstate();
    get_PCIstate();

    /* Based on the hardware version number, the GUI's main label changes */
    if((PCIInfo.Version & 0x0000000F) == 1)
        strcat(mainlabel, " - AXI TRD");

    /* Incase a test is going on, then enable the buttons to be correct */
    testmode = EngStats[0].TestMode;
    if(testmode & TEST_IN_PROGRESS)
    {
        TestStartFlag1 |= TEST_START;
        if(testmode & ENABLE_PKTCHK) TestStartFlag1 |= ENABLE_PKTCHK;
        if(testmode & ENABLE_LOOPBACK) TestStartFlag1 |= ENABLE_LOOPBACK;
    }
    if(testmode & ENABLE_PKTGEN) TestStartFlag1 |= ENABLE_PKTGEN;
    printf("TestStartFlag1 is %x\n", TestStartFlag1);

    testmode = EngStats[2].TestMode;
    if(testmode & TEST_IN_PROGRESS)
    {
        TestStartFlag2 |= TEST_START;
        if(testmode & ENABLE_PKTCHK) TestStartFlag2 |= ENABLE_PKTCHK;
        if(testmode & ENABLE_LOOPBACK) TestStartFlag2 |= ENABLE_LOOPBACK;
    }
    if(testmode & ENABLE_PKTGEN) TestStartFlag2 |= ENABLE_PKTGEN;
    printf("TestStartFlag2 is %x\n", TestStartFlag2);

    /* Initialise arrays of graph readings. */
    for(k=0; k<MAX_ENGS; k++)
    {
        for(i=0; i<MAX_STATS; i++)
            DMAarray[k][i] = SWarray[k][i] = 0;
        dsRead[k] = dsWrite[k] = dsNum[k] = 0;
        ssRead[k] = ssWrite[k] = ssNum[k] = 0;
    }
    for(k=0; k<2; k++)
    {
        for(i=0; i<MAX_STATS; i++)
            TRNarray[k][i] = 0;
        tsRead[k] = tsWrite[k] = tsNum[k] = 0;
    }



    gtk_init(&argc, &argv);
    g_set_print_handler (CreateText);

    Max = sizeof(GuiScreen) / sizeof(GuiScreen[0]) ;
  
    for(i = 0; i < Max; i++) 
    {
        DrawWidget(GuiScreen,GuiWidgets,i);
        if(GuiScreen[i].Callback !=NULL)
            g_signal_connect(GuiWidgets[i], Signal[GuiScreen[i].SignalIndex],
                             (GtkSignalFunc)GuiScreen[i].Callback,GINT_TO_POINTER(GuiScreen[i].Data));
        //ConnectWidget(GuiScreen,GuiWidgets,i)
    }

    /* Display the test settings */ //guodebug %lu -> %u
    sprintf(str,"%u", EngStats[0].MinPkt);
    gtk_entry_set_text(GTK_ENTRY(GuiWidgets[MIN_PKT_ENTRY1_INDEX]), str);
    sprintf(str,"%u", EngStats[0].MaxPkt);
    gtk_entry_set_text(GTK_ENTRY(GuiWidgets[MAX_PKT_ENTRY1_INDEX]), str);
    sprintf(str,"%u", EngStats[2].MaxPkt);
    gtk_entry_set_text(GTK_ENTRY(GuiWidgets[PKT_ENTRY2_INDEX]), str);

    PutTestSettings();
    PutTestButtons1();
    PutTestButtons2();

#ifdef S6_TRD
    /* Grey out the test settings for path 1 */
    gtk_widget_set_sensitive(GuiWidgets[LABEL1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[ENABLE_LOOPBACK_LABEL1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[ENABLE_LOOPBACK_BUTTON1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[MIN_PKT_LABEL1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[MIN_PKT_ENTRY1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[MAX_PKT_LABEL1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[MAX_PKT_ENTRY1_INDEX], FALSE);
    gtk_widget_set_sensitive(GuiWidgets[START_BUTTON1_INDEX], FALSE);
#endif

    gtk_widget_show_all(GuiWidgets[0]);
    gtk_main();

#ifdef HARDWARE
    close(statsfd);
#endif

    return TRUE;
}


