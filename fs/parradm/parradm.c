#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "pmem_ar.h"

int main(int argc, char *argv[]) {
  int fd = 0;
  if (argc < 3) {
    fprintf(stderr,
            "Usage: %s create/delete <target_disk_path> <pm_config_file>\n",
            argv[0]);

    exit(1);
  }

  /* allows shorthand cr */
  if (strncmp(argv[1], "cr", 2) == 0) {
    fd = open(argv[2], O_RDWR);
    if (fd == -1)
      perror("open");

    FILE *config_fp = fopen(argv[3], "r");
    if (config_fp == NULL)
      perror("open");

    struct pmem_arg_info pmem_arg_info = {0};
    int i = 0;
    char line[1024];

    while (fgets(line, sizeof(line), config_fp)) {

      if (line[0] == '\n' || line[0] == '#')
        continue;

      pmem_arg_info.paths[i] = malloc(256);

      if (sscanf(line, "%255s %d", pmem_arg_info.paths[i],
                 &pmem_arg_info.numa_node[i]) != 2) {
        fprintf(stderr, "config file format error: %s", line);
        fclose(config_fp);
        close(fd);
        exit(-1);
      }

      i++;
    }
    pmem_arg_info.num = i;

    fclose(config_fp);

    if (ioctl(fd, PMEM_AR_CMD_CREATE, &pmem_arg_info) == -1) {
      perror("ioctl: create");
      close(fd);
      exit(-1);
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