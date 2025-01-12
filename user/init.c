#include "ulib.h"
#include "usyscalls.h"

int main() {
  char *args[] = {"/echo", "Hello", "world!", NULL};
  puts("Hello from userspace!");
  exec("/echo", args);
  puts("Ran echo!");
  while (1)
    yield();
}