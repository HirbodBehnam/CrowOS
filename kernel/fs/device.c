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
                                      .name = SERIAL_DEVICE_NAME,
                                      .read = serial_read,
                                      .write = serial_write,
                                      .lseek = NULL, // no seek
                                  },
                                  {
                                      .name = SERIAL_ASYNC_DEVICE_NAME,
                                      .read = serial_read_async,
                                      .write = serial_write,
                                      .lseek = NULL, // no seek
                                  }};

// Number of devices which we support
#define DEVICES_SIZE (sizeof(devices) / sizeof(devices[0]))

/**
 * Gets the device index based on the given name.
 * Returns -1 if the device does not exists.
 */
int device_index(const char *name) {
  for (size_t i = 0; i < DEVICES_SIZE; i++)
    if (strcmp(name, devices[i].name) == 0)
      return (int)i;
  return -1;
}

int device_open(const char *name) {
  // Look for this device
  int device_index_result = device_index(name);
  if (device_index_result == -1)
    return -1;
  // Look for an empty file.
  int fd = proc_allocate_fd();
  if (fd == -1)
    return -1;
  struct process *p = my_process(); // p is not null
  // Set the info
  p->open_files[fd].type = FD_DEVICE;
  p->open_files[fd].structures.device = device_index_result;
  p->open_files[fd].offset = 0;
  p->open_files[fd].readble = devices[device_index_result].read != NULL;
  p->open_files[fd].writable = devices[device_index_result].write != NULL;
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