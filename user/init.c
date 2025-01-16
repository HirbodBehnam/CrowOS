#include "printf.h"
#include "ulib.h"
#include "usyscalls.h"

int main() {
  char *args[] = {"/echo", "Hello", "world!", NULL};
  puts("Hello from userspace!");
  exec("/echo", args);
  puts("Ran echo!");
  char buffer[128];
  while (1) {
    gets(buffer, sizeof(buffer));
    printf("%s", buffer);
  }
}