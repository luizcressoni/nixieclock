/*! \file actions.cpp */
#include "actions.h"
#include <condition_variable>
#include <deque>
#include <mutex>

std::condition_variable cv;
std::deque<int> gActions;
std::mutex mtx;

//Same reasoning as the signal queue: a consumer that has stopped draining must not turn into
//a slow memory leak. At the rate the clock actually processes actions this is unreachable.
static constexpr size_t MAX_QUEUED = 256;

/*! \brief Add an action to the queue.
 *  \param action The action to add.
 *
 *  \note This function adds an action to the global actions queue and notifies
 *  \note any waiting threads that an action is available.
 */
void add_action(const int action)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (gActions.size() >= MAX_QUEUED)
        gActions.pop_front();       //the oldest is the least interesting one to keep
    gActions.push_back(action);
    cv.notify_one();
}

/*! \brief Get an action from the queue.
 *  \param _wait If true, wait for an action to be available.
 *  \return The action if available, or -1 if no action is available and _wait is false.
 *
 *  \note This function retrieves an action from the global actions queue. If
 *  \note _wait is true, it will block until an action is available.
 *
 *  \note First in, first out. It used to be a stack -- push_back paired with back() and
 *  \note pop_back() -- so whenever more than one action was waiting, the state machine saw
 *  \note them in the reverse of the order they happened in. The camera sends motion before
 *  \note face within a pass for a reason, and the queue was undoing that. Nothing was lost,
 *  \note but with two actions whose effects do not commute, the order decides the outcome.
 */
int get_action(bool _wait)
{
    std::unique_lock<std::mutex> lock(mtx);
    
    if (_wait)
        cv.wait(lock, []{ return !gActions.empty(); });
    else
    {
        if (gActions.empty())
            return -1;
    }

    const int ret = gActions.front();
    gActions.pop_front();
    return ret;
}


//eof actions.cpp
