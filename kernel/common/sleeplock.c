#include <stddef.h>
#include <stdint.h>
#include "spinlock.h"

struct sleeplock {
  uint32_t locked;
};