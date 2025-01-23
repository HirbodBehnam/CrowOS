#include "include/exec.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "libc/usyscalls.h"

/**
 * Trims an string from the chars such as \r and \n
 */
static void trim_string(char *string) {
  size_t length = strlen(string);
  while (length > 0 &&
         (string[length - 1] == '\r' || string[length - 1] == '\n')) {
    length--;
    string[length] = '\0';
  }
}

// Defines these in global scope to avoid stack overflow
static char *args[MAX_ARGV];
static char input_buffer[512];

int main() {
  puts("Welcome to CrowOS!");
  while (1) {
    printf("$ ");
    gets(input_buffer, sizeof(input_buffer));
    trim_string(input_buffer);
    if (strlen(input_buffer) == 0) // empty buffer
      continue;
    // Parse the arguments
    char *breaking_point = input_buffer;
    args[0] = breaking_point;
    int i;
    for (i = 1; i < MAX_ARGV - 1; i++) {
      breaking_point = strchr(breaking_point, ' ');
      if (breaking_point == NULL) // reached end of string
        break;
      // Put a null terminator to terminate argument before
      *breaking_point = '\0';
      breaking_point++; // skip the null terminator
      args[i] = breaking_point;
    }
    args[i] = NULL; // terminate the arguments
    int pid = exec(args[0], args);
    if (pid < 0) {
      printf("Cannot execute %s: %d\n", args[0], pid);
      continue;
    }
    // Wait until the process is done
    wait(pid);
  }
}