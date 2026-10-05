/*! \file main.cpp*/
#include <thread>
#include "actions.h"
#include "fsm.h"
#include "config.h"
#include "hardware/hardware.h"
#include "../utils/json_parser.h"
#include "../utils/signals.h"
#include "../utils/defines.h"
#include "../logger/logger.h"
#include "timethread.h"
#include <cstdio>

/*! \brief Initializes the system resources*/
void init()
{
    init_logger("nixie");
    glogger->set_level(spdlog::level::debug);
    load_config_file();
    gNixieFsm = new cNixieFsm();

}

/*! \brief */
void deinit()
{
    delete gNixieFsm;
    free_jsonfile();
}


void signal_processor_thread()
{
    start_signal_thread();
    while (true)
    {
        int value = wait_for_signal();
        add_action(value);
        if(value == SIG_CGI_EXIT)
        break;
    }
}


int main()
{
    //Before init(), because init() brings up pigpio and pigpio starts threads of its own: a
    //thread only inherits the mask that existed when it was created. Any thread that does not
    //block NIXIE_SIGNAL is a thread the kernel may deliver it to instead of our sigwaitinfo().
    block_signals();
    init();
    std::thread signal_thread(signal_processor_thread);
    TimeWatcher time_watcher;
    time_watcher.start();

    gNixieFsm->ProcessAction(SIG_NONE);
    
    while (true)
    {
        int action = get_action(true);
        if(action == SIG_TIME_CHANGED)
            glogger->flush();

        if (action == -1)
            continue;
        if(action == SIG_CGI_EXIT)
            break;
        if(action & SIG_CGI)
        {
            load_config_file();
        }
        if(gNixieFsm != nullptr)
        {
            gNixieFsm->ProcessAction(action);
        }
    }

    LOGGER_DEBUG("Exiting...");
    signal_thread.join();
    time_watcher.stop();
    deinit();
    return 0;
}




//eof main.cpp
