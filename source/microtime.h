/*! \file microtime.h */

#include <linux/types.h>
#include <pthread.h>

void start_microtime();
void stop_microtime();

__u32   milis();
