#include "include/file.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "libc/usyscalls.h"

static char format_dirent_type(uint8_t type) {
  switch (type) {
  case DT_DIR:
    return 'D';
  case DT_FILE:
    return 'F';
  default:
    return 'X';
  }
}

static void ls(const char *path) {
  char buf[512];
  int fd, read_dirs;

  if ((fd = open(path, O_RDONLY | O_DIR)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  while ((read_dirs = readdir(fd, buf, sizeof(buf))) > 0) {
    const void *temp_buffer = buf;
    for (int i = 0; i < read_dirs; i++) {
      const struct dirent *d = temp_buffer;
      printf("%c\t%u\t%lld\t%s\n", format_dirent_type(d->type), d->size,
             d->creation_date, d->name);
      temp_buffer += sizeof(struct dirent) + strlen(d->name);
    }
  }

  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    ls(".");
    exit(0);
  }

  for (int i = 1; i < argc; i++)
    ls(argv[i]);
  exit(0);
}