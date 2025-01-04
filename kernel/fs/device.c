#include "device.h"
#include "common/lib.h"
#include "device/serial_port.h"
#include "userspace/proc.h"
#include <stddef.h>
#include <stdint.h>

/**
 * List of all devices which user can use
 */
static struct device devices[] = {{
    .name = "serial",
    .read = serial_read,
    .write = serial_write,
}};

// Number of devices which we support
#define DEVICES_SIZE (sizeof(devices) / sizeof(devices[0]))

int device_open(const char *name) {
  // Look for this device
  int device_index = -1;
  for (size_t i = 0; i < DEVICES_SIZE; i++) {
    if (strcmp(name, devices[i].name) == 0) {
      device_index = (int)i;
      break;
    }
  }
  if (device_index == -1)
    return -1;
  // Look for an empty file.
  int fd = proc_allocate_fd();
  if (fd == -1)
    return -1;
  struct process *p = my_process(); // p is not null
  // Set the info
  p->open_files[fd].type = FD_DEVICE;
  p->open_files[fd].structures.device = device_index;
  p->open_files[fd].offset = 0;
  p->open_files[fd].readble = devices[device_index].read != NULL;
  p->open_files[fd].writable = devices[device_index].write != NULL;
  return fd;
}

/**
 * Gets a device by its index. Will return NULL if the index
 * is out of bounds.
 */
struct device *device_get(int index) {
  if (index < 0 || index >= (int)DEVICES_SIZE)
    return NULL;
  return &devices[index];
}