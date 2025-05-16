#include <stdio.h>
#include "common.h"

#define IO_DEPTH 128
#define DATA_SZ 4096UL
#define META_SZ 64UL
#define DEV_PATH "/dev/nvme0n1p1"
#define LOOPS (1024 * 1024)

int register_device(struct thread_data *td, bool unit_test)
{
    int ret;
    int sq_poll_cpu = -1;

    io_register(unit_test);

    ret = thread_data_init(td, IO_DEPTH, DATA_SZ,
                           DEV_PATH, &sq_poll_cpu);
    ret = io_open(td, "uring");

    return ret;
}

int unregister_device(struct thread_data *td)
{
    int ret;

    io_drain(td);
    io_fence(td);
    ret = io_close(td);
    thread_data_cleanup(td);

    io_unregister();

    return ret;
}

int main(int argc, char *argv[])
{
    struct thread_data td;
    int ret;
    char buf[DATA_SZ] = {0};
    u64 dev_addr = 0;
    unsigned long i;
    struct timespec start, end;
    int order_elimination = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <enable_order_elimination>\n", argv[0]);
        return -1;
    }

    order_elimination = atoi(argv[1]);

    register_device(&td, false);

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < LOOPS; i++)
    {
        io_write(&td, dev_addr + i * DATA_SZ, buf, DATA_SZ,
                 O_IO_DROP);
        if (!order_elimination)
            io_fence(&td);
        io_write(&td, dev_addr + LOOPS * DATA_SZ + i * DATA_SZ, buf, META_SZ,
                 O_IO_DROP);
        io_fence(&td);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("Time taken: %lf seconds\n",
           (end.tv_sec - start.tv_sec) +
               (end.tv_nsec - start.tv_nsec) / 1000000000.0);
    printf("Bandwidth: %lf MB/s\n",
           (double)((double)(LOOPS * DATA_SZ) / 1024 / 1024) /
               ((end.tv_sec - start.tv_sec) +
                (end.tv_nsec - start.tv_nsec) / 1000000000.0));

    unregister_device(&td);
}