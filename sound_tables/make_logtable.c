#include <stdio.h>
#include <math.h>
#include "tbl_param.h"

static unsigned int logtbl[logtable_len];

int
main()
{
  int i;
  double a;
  
  memset(logtbl, 0, sizeof(logtbl));
  
  for (i = 0; i < logtable_len; i++)
    logtbl[i] = (unsigned int)(pow (2, (LOG_LIN_BITS - (i / (double)logtable_len))));
  
  for (i = 0; i < logtable_len; ++i)
    printf("%d,\n", logtbl[i]);
}

