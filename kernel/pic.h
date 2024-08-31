#include <stdint.h>
#include <stddef.h>
void picinit(void);
void ioapicinit(void);
void ioapicenable(int irq, int cpunum);
void lapicinit(void);
void send_eoi(void);