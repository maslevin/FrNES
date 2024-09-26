#ifndef __NES_APU_TABLES_PARAM_H
#define __NES_APU_TABLES_PARAM_H

#define LOG_BITS 12
#define LIN_BITS 6
#define LOG_LIN_BITS 30

#define lineartable_len ((1 << LIN_BITS) + 1)
#define logtable_len (1 << LOG_BITS)

#endif
