#include "include/file.h"
#include "printf.h"
#include "ulib.h"
#include "usyscalls.h"

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
  // Allocate a buffer on heap to avoid stack overflow
  char *buffer = sbrk(BUFFER_SIZE);

  for (int i = 1; i < argc; i++) { // For each argument...
    // Open the file...
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "cannot open file %s: %d\n", argv[i], fd);
      exit(1);
    }
    // Read it in chunks
    int n;
    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
      if (write(stdout, buffer, n) != n) {
        fprintf(stderr, "write error to stdout\n");
        exit(1);
      }
    }
    if (n < 0) {
      fprintf(2, "read error on file %s: %d\n", argv[i], n);
      exit(1);
    }
    // Close it
    close(fd);
  }
  exit(0);
}