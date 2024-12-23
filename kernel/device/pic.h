#include <stdint.h>
#include <stddef.h>

void ioapic_init(void);
void ioapic_enable(int irq, int cpunum);
void lapic_init(void);
void lapic_send_eoi(void);