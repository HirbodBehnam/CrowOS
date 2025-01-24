#include "stdlib.h"
#include "usyscalls.h"

void _start(int argc, char *argv[]) {
  extern int main(int argc, char *argv[]);
  exit(main(argc, argv));
}

int abs(int a) { return a > 0 ? a : -a; }

int atoi(const char *s) {
  int n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}