/*! \file mmap.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define BCM_PERI_BASE           0x20000000      //!< Raspi 1 & Zero
//#define BCM_PERI_BASE		    0x3F000000      //!< Raspi 2 & 3

#define GPIO_BASE               (BCM_PERI_BASE + 0x200000)	// GPIO controller

// IO Acces
struct bcm2835_peripheral
{
    unsigned long addr_p;
    int mem_fd;
    void *map;
    volatile unsigned int *addr;
};


int map_peripheral(struct bcm2835_peripheral *p);
void unmap_peripheral(struct bcm2835_peripheral *p);

//eof mmap.h
