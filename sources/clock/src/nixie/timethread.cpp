/*! \file timethread.cpp */
#include "timethread.h"

#include "actions.h"
#include "../utils/defines.h"


TimeWatcher::TimeWatcher() : running(false) {
}

TimeWatcher::~TimeWatcher() {
    stop(); 
}

/*! \brief Start the time-watching thread.
    \note: This function starts a thread that will periodically check the time and trigger actions.
*/
void TimeWatcher::start() {
    if (running) return;
    running = true;
    thread = std::thread(&TimeWatcher::watch_loop, this);
}

/*! \brief Stop the time-watching thread.
    \note: This function stops the time-watching thread and waits for it to finish.
*/
void TimeWatcher::stop() {
    if (!running) return;
    running = false;
    if (thread.joinable()) {
        thread.join();
    }
}

/*! \brief loop to wait 1 second to update display 
    \note: we use steady_clock to avoid system_clock problems when clock goes backwards or jumps.
*/
void TimeWatcher::watch_loop() const {
    while (running) {
        auto now = std::chrono::steady_clock::now();
        auto next = std::chrono::time_point_cast<std::chrono::seconds>(now) + std::chrono::seconds(1);
        std::this_thread::sleep_until(next);

        if (running) {
            add_action(SIG_TIME_CHANGED);
        }
    }
}


//eof timethread.cpp