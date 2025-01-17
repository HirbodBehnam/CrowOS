#include "include/exec.h"
#include "printf.h"
#include "ulib.h"
#include "usyscalls.h"

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

int main() {
  char *args[MAX_ARGV];
  char input_buffer[512];
  puts("Welcome to CrowOS!");
  while (1) {
    int i;
    printf("$ ");
    gets(input_buffer, sizeof(input_buffer));
    trim_string(input_buffer);
    args[0] = input_buffer;
    for (i = 1; i < MAX_ARGV - 1; i++) {
      char *breaking_point = strchr(input_buffer, ' ');
      if (breaking_point == NULL) // reached end of string
        break;
      // Put a null terminator to terminate argument before
      *breaking_point = '\0';
      args[i] = breaking_point + 1; // Next argument starts from char after
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