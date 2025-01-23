#include "libc/stdio.h"
#include "libc/usyscalls.h"

int main() {
  printf("Current epoch is %llu\n", time());
  exit(0);
}