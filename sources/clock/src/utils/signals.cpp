/*! \file signals.cpp */
#include "signals.h"

#include <sys/types.h>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include "proc_find.h"
#include "defines.h"

namespace {

//Nothing below runs in a signal handler any more, so a plain mutex is safe here. The previous
//version took this same lock and called notify_one() from inside the handler, and neither is
//async signal safe: when the signal happened to land on the thread that already held the lock,
//that thread deadlocked against itself and the clock stopped answering the camera until it was
//restarted. Blocking the signals and reading them with sigwaitinfo() removes the handler
//entirely, so there is no longer any code running at an arbitrary point in another thread.
std::mutex              mtx_signal;
std::condition_variable cond_var;
std::deque<int>         signal_queue;

//A sender can only outrun the display for so long before the backlog is stale anyway, and an
//unbounded queue would turn a stuck consumer into a slow memory leak.
constexpr size_t MAX_QUEUED = 256;

sigset_t handled_set()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, NIXIE_SIGNAL);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTERM);
    return set;
}

void push_signal(int _value)
{
    {
        std::lock_guard<std::mutex> lock(mtx_signal);
        if(signal_queue.size() >= MAX_QUEUED)
            signal_queue.pop_front();   //the oldest is the least interesting one to keep
        signal_queue.push_back(_value);
    }
    cond_var.notify_one();
}

/*! \brief Reads signals synchronously, one at a time, with their queued values intact. */
void signal_thread()
{
    const sigset_t set = handled_set();

    while(true)
    {
        siginfo_t info{};
        if(sigwaitinfo(&set, &info) == -1)
        {
            if(errno == EINTR)
                continue;
            break;
        }

        if(info.si_signo == NIXIE_SIGNAL)
            push_signal(info.si_value.sival_int);
        else
        {
            push_signal(SIG_CGI_EXIT);
            break;
        }
    }
}

} //namespace

/*! \brief Blocks the handled signals in the calling thread, and so in every thread it spawns. */
void block_signals()
{
    const sigset_t set = handled_set();
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

/*! \brief Starts the thread that consumes signals
    \note block_signals() must already have run in main(), or the signals will be delivered to
    \note whichever thread happens not to block them instead of reaching sigwaitinfo().
*/
void start_signal_thread() {
    std::thread t(signal_thread);
    t.detach();
}   

/*! \brief Waits for the next signal value
    \return Signal value
    \note Blocks until one is available. Values are queued, so a burst is drained one by one
    \note instead of the consumer only ever seeing whichever arrived last.
*/
int wait_for_signal() {
    std::unique_lock<std::mutex> lock(mtx_signal);
    cond_var.wait(lock, [] { return !signal_queue.empty(); });
    const int value = signal_queue.front();
    signal_queue.pop_front();
    return value;
}

/*! \brief Takes the next signal value if there is one, without waiting
    \param _value Receives the value.
    \return False when the queue is empty.
    \note For loops that have their own pace, such as the camera's frame loop.
*/
bool poll_signal(int *_value) {
    std::lock_guard<std::mutex> lock(mtx_signal);
    if(signal_queue.empty())
        return false;
    *_value = signal_queue.front();
    signal_queue.pop_front();
    return true;
}


/*! \brief Constructor
    \param _process Process name
    \note Initializes the process name and sets the known PID to -1.
*/
cSignal::cSignal(const char *_process)
{
    if (_process) {
        strncpy(process, _process, sizeof(process) - 1);
        process[sizeof(process) - 1] = '\0';
    } else {
        process[0] = '\0';
    }
    m_knownpid = -1;
}

cSignal::~cSignal() = default;

/*! \brief Sends a signal with parameters to the known pid process
    \param _signal Signal to send
    \param _parameter Parameter to send with the signal
    \note If the known PID is invalid, it will search for the process by name.
    \note This function uses sigqueue to send the signal with a value.
*/
void cSignal::Send(int _signal, int _parameter)
{
    if(!test_pid(m_knownpid))
        m_knownpid = proc_find(process);

    if (m_knownpid != -1){
        sigval value{};
        value.sival_int = _parameter;
        sigqueue(m_knownpid, _signal, value);
    }
    else
        printf("Process '%s' not found\n", process);
}


//eof signals.cpp
