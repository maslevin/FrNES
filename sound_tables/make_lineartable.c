#include <stdio.h>
#include <math.h>
#include "tbl_param.h"

static unsigned int lineartbl[lineartable_len];

int
main()
{
  int i;
  double a;
  unsigned int ua;
  
  memset(lineartbl, 0, sizeof(lineartbl));
  
  lineartbl[0] = LOG_LIN_BITS << LOG_BITS;

  for (i = 1; i < lineartable_len; i++)
    lineartbl[i] = 
      (unsigned int)((LIN_BITS - log(i)/log(2)) * (1 << LOG_BITS)) << 1;
  
  for (i = 0; i < lineartable_len; ++i)
    printf("%d,\n", lineartbl[i]);
}


