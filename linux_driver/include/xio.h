#ifndef XIO_H           /* prevent circular inclusions */
#define XIO_H           /* by using protection macros */

#include <linux/types.h>
#include <asm/io.h>



#ifndef TRUE
#  define TRUE    1
#endif

#ifndef FALSE
#  define FALSE   0
#endif

#ifndef __LP64__
  #define Xaddr u32
#else
  #define Xaddr u64
#endif

#define XIo_In32(addr)          (readl((unsigned int*)(addr)))
#define XIo_Out32(addr, data)   (writel((data), (unsigned int*)(addr)))


#endif          /* end of protection macro */

