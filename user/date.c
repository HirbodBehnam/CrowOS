#include "printf.h"
#include "usyscalls.h"

int main() {
  printf("Current epoch is %llu\n", time());
  exit(0);
}