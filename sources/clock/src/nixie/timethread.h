/*! \file timethread.h */
#pragma once

#include <thread>
#include <atomic>

/*! \class TimeWatcher
 *  \brief A class that monitors time in a separate thread.
 *
 *  \note This class provides functionality to start and stop a time-watching thread.
 *  \note It can be used to perform periodic tasks or monitor elapsed time in a non-blocking manner.
*/
class TimeWatcher {
private:
    std::atomic<bool> running;
    std::thread thread;

    void watch_loop() const;
public:
    TimeWatcher();
    ~TimeWatcher();

    void start();
    void stop();
};

//eof timethread.h