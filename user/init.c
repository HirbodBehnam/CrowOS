#include "ulib.h"
#include "usyscalls.h"

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    write(serial_fd, argv[i], strlen(argv[i]));
    if (i + 1 < argc) {
      write(serial_fd, " ", 1);
    } else {
      write(serial_fd, "\n", 1);
    }
  }

  puts("Hello from userspace!");
  while (1)
    yield();
}