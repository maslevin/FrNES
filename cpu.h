#ifndef _CPU_H
#define _CPU_H

void set_nmi(bool v);
void set_irq(bool v);
// int dmc_read(void*, cpu_addr_t addr);
void power();
void run_frame();

#endif