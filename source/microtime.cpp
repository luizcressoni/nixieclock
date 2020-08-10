/*! \file microtime.cpp */
#include "microtime.h"
#include <unistd.h>
#include <pthread.h>

__u32 gu32microtime = 0;
pthread_t   mthread_time = 0;
bool gmicrotimerunning = false;

void *gMicrotimeThread(void *_data)
{
    bool *running = (bool *)_data;

    while(running)
    {
        gu32microtime++;
        usleep(9);
    }
    return NULL;
}



void start_microtime()
{
    if(!gmicrotimerunning)
    {
        gmicrotimerunning = true;
        if(pthread_create(&mthread_time, NULL, gMicrotimeThread, &gmicrotimerunning) != 0)
        {
            gmicrotimerunning = false;
            mthread_time = 0;
        }
    }
}

void stop_microtime()
{
    if(mthread_time != 0)
    {
        gmicrotimerunning = false;
        pthread_join(mthread_time, NULL);
        mthread_time = 0;
    }
}

__u32   milis()
{
    return gu32microtime;
}
