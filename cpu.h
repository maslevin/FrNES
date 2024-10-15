#ifndef _CPU_H
#define _CPU_H

void set_nmi(bool v = true);
void set_irq(bool v = true);
// int dmc_read(void*, cpu_addr_t addr);
void power();
void run_frame();

#endif