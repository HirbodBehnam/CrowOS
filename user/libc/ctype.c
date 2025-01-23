#include "ctype.h"

int isspace(int c) { return c == ' '; }

int toupper(int c) {
  if (c >= 'a' && c <= 'z')
    c += 'A' - 'a';
  return c;
}
