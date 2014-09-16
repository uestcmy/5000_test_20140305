#ifndef TYPES_H
#define TYPES_H

#include <gtk/gtk.h>
#include <sys/types.h>

/* Types */
typedef unsigned char  uchar;   /* 8-bit value on Windows */
//typedef unsigned short ushort;  /* 16-bit value on Windows */
//typedef unsigned int   uint;    /* 32-bit value on Windows */

/* Various printfs
 * The following should ideally be properly enclosed in curly braces as
 * a good programming practice. But, due to the necessity for handling
 * variable argument lists, this has not been done.
 * Also, comma is being used as the separator, incase the printf is from
 * an if-else branch.
 */
#define msg_error       display=MSG_ERROR, g_print
#define msg_warning     display=MSG_WARNING, g_print
#define msg_info        display=MSG_INFO, g_print
#define msg_debug       display=MSG_DEBUG, g_print

enum print
{
    MSG_ERROR = 0,
    MSG_WARNING,
    MSG_INFO,
    MSG_DEBUG
};
extern enum print display;

#endif
