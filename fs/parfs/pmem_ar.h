#ifndef ___PMEM_AR_H_
#define ___PMEM_AR_H_

#define PMEM_MAJOR 486
#define PMEM_DEV_NAME "parfs_pmem_ar"
#define PMEM_DEV_INSTANCE_NAME "parfs_pmem_ar0"
#define PMEM_AR_NUM 1

#define PMEM_AR_MAX_DEVICE 8

struct pmem_arg_info {
	/* number of devices to add*/
	int num;

	/* path to these devices */
	char *paths[PMEM_AR_MAX_DEVICE];
};

#define PMEM_AR_CMD_CREATE 0
#define PMEM_AR_CMD_DELETE 1

#endif /* SPMEM_PMEM_AR_H_ */
