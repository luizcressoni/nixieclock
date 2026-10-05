/*! \file signals.h */
#pragma once
#include <signal.h>

//The clock used to talk to itself over SIGUSR1. SIGUSR1 is a standard signal, and standard
//signals are not queued: if one was already pending, a second sigqueue() was dropped on the
//floor together with its sival_int. Face, motion and brightness all share this one channel,
//so under any kind of burst events really were being lost before they were ever seen.
//Real time signals do queue, one delivery per send, each keeping its own value.
//SIGRTMIN is a function call on glibc rather than a constant, so it can be used in a call but
//never in a case label or a static initialiser.
#define NIXIE_SIGNAL    (SIGRTMIN)

/*! \brief Blocks the signals handled by the signal thread in the calling thread.
    \note Call this as the very first thing in main(), before any thread exists and before
    \note pigpio is initialised. The mask is inherited by every thread created afterwards,
    \note which is what makes sigwaitinfo() the only consumer of these signals in the process.
*/
void block_signals();
void start_signal_thread();
int wait_for_signal();
bool poll_signal(int *_value);


class cSignal
{
protected:
    pid_t m_knownpid = -1;
    char process[64] = {0};
public:
    cSignal(const char *_process);
    ~cSignal();    
    void Send(int _signal, int _parameter);
    
};

//eof signals.h
