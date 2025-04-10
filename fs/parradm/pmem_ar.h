/*
 * pmem_ar.h
 *
 *  Created on: Jul 13, 2021
 *      Author: dzhou
 */

#ifndef _PMEM_AR_H_
#define _PMEM_AR_H_

#define PMEM_AR_NUM 1

#define PMEM_AR_MAX_DEVICE 8

struct pmem_arg_info {
  /* number of devices to add*/
  int num;

  /* path to these devices */
  char *paths[PMEM_AR_MAX_DEVICE];
  int numa_node[PMEM_AR_MAX_DEVICE];
};

#define PMEM_AR_CMD_CREATE 0
#define PMEM_AR_CMD_DELETE 1

#endif /* PMEM_PMEM_AR_H_ */
