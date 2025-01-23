#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * Defines a single device attached to system
 */
struct device {
  // Common name of device. User uses the open syscall to open the device.
  const char *name;
  // What we should do on the read from this device
  int (*read)(char *, size_t);
  // What we should do on the write to this device
  int (*write)(const char *, size_t);
  // Seek while reading or writing to this device
  int (*lseek)(int64_t, int);
  // Control the options for this device. Might get or set values.
  int (*control)(int, void *);
};

int device_index(const char *name);
int device_open(const char *name);
struct device *device_get(int index);
