#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pmem_ar.h"

int main(int argc, char *argv[]) {
  int fd = 0;
  if (argc < 3) {
    fprintf(stderr,
            "Usage: %s create/delete <target_disk_path> <path_to_disk1> "
            "<path_to_disk2> ...\n",
            argv[0]);

    exit(1);
  }

  /* allows shorthand cr */
  if (strncmp(argv[1], "cr", 2) == 0) {
    fd = open(argv[2], O_RDWR);
    if (fd == -1)
      perror("open");

    struct pmem_arg_info pmem_arg_info;
    int i = 0;

    pmem_arg_info.num = argc - 3;

    for (i = 0; i < pmem_arg_info.num; i++)
      pmem_arg_info.paths[i] = argv[i + 3];

    if (ioctl(fd, PMEM_AR_CMD_CREATE, &pmem_arg_info) == -1) {
      perror("ioctl: create");
      exit(1);
    }
  }
  /* allows shorthand de */
  else if (strncmp(argv[1], "de", 2) == 0) {
    fd = open(argv[2], O_RDWR);
    if (fd == -1)
      perror("open");
    if (ioctl(fd, PMEM_AR_CMD_DELETE) == -1) {
      perror("ioctl: delete");
      exit(1);
    }
  } else {
    fprintf(stderr, "Unknown cmd: %s\n", argv[1]);
    exit(1);
  }

  return 0;
}
