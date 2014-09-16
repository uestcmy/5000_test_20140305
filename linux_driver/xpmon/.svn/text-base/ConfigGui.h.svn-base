#ifndef CONFIGGUI_H
#define CONFIGGUI_H
/* Enumerated data type to hold widgets type */
enum
{
    WINDOW=0,   VPANED,         TABLE,          NOTEBOOK,   BUTTON, FRAME,
    GRAPH,      COMBO,          GLIST_START,    GLIST_END,  GLIST,  LABEL,
    ENTRY,      DRAWING_AREA,   SCROLLED_WINDOW,TEXTVIEW,   ALIGN,  IMAGE,
    CHECK_BUTTON,TOGGLE_BUTTON, HSEPARATOR,     RCHECK_BUTTON,
    RADIO_BUTTON, RADIO_GROUP,  VSEPARATOR
};

struct screen
{
    char * Name;        /* used to store name of widget*/
    unsigned int Type;  /* Type of widget*/
    unsigned int Left;  /* Typically used for information related to attaching
                         * in a table. Overloaded to inform margin in case of
                         * Main Window
                         */
    unsigned int Right;
    unsigned int Top;
    unsigned int Bottom;
    int Parent;         /* index of parent widget : -1 if no parent */
    unsigned int row;   /* Rows and column of a table. Overloaded to inform */
    unsigned int col;   /* widget size */
    int active;         /* store the sensitive state of widget. In case of
                         * table it is used for storing homogeneous state
                         */
    gboolean (*Callback)(GtkWidget* ,void *);
    /* function pointer for callback */
    long SignalIndex;    /* signal to result in callback */ //guodebug: int->long
    gpointer Data;      /* Argument passed to callback */
};

#endif
