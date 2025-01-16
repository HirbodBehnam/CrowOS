#include "ulib.h"
#include "usyscalls.h"

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    write(stdout, argv[i], strlen(argv[i]));
    if (i + 1 < argc) {
      write(stdout, " ", 1);
    } else {
      write(stdout, "\n", 1);
    }
  }
  exit(0);
}