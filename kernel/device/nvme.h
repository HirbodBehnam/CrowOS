#include <stdint.h>

void nvme_init(void);
void nvme_write(uint64_t lba, uint32_t block_count, const char *buffer);
void nvme_read(uint64_t lba, uint32_t block_count, char *buffer);