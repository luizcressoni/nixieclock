/*! \file proc_find.cpp */
#include "proc_find.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>

/*! \brief Tests is a process with this pid exists
    \param pid Process ID
    \return true if the process exists, false otherwise
    \note This function is not thread-safe. It should be used with caution in multi-threaded applications.
    \note This function uses the /proc filesystem to check for the existence of a process.
*/
bool test_pid(pid_t pid) {
    if(pid <= 0)
        return false;
    
    char path[40];
    snprintf(path, sizeof(path), "/proc/%d", pid);
     DIR* dir;
    if ((dir = opendir(path)) == nullptr) {
        return false;
    }
    closedir(dir);
    return true;
}

/*! \brief Finds a process by its name
    \param name Process name
    \return Process ID if found, -1 otherwise
    \note This function uses the /proc filesystem to find the process.
*/
pid_t proc_find(const char* name)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!((dir = opendir("/proc"))))
        return -1;

    while((ent = readdir(dir)) != nullptr) {
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }

        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);

        if (FILE* fp = fopen(buf, "r")) {
            if (fgets(buf, sizeof(buf), fp) != nullptr) {
                //Exact program name, not a substring: "nixie" is part of /home/pi/nixiepi/camera
                //and of nixie.cgi, so a substring match could hand the clock's signals to either.
                const char* first = strtok(buf, " ");
                const char* base = first ? strrchr(first, '/') : nullptr;
                base = base ? base + 1 : first;
                if(base != nullptr && strcmp(base, name) == 0){
                    fclose(fp);
                    closedir(dir);
                    return static_cast<pid_t>(lpid);
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);
    return -1;
}


// /*! \brief Finds a process by its name, using a known PID if available
//     \param name Process name
//     \param _knownpid Known process ID
//     \return Process ID if found, -1 otherwise
//     \note This function uses the /proc filesystem to find the process.
// */
// pid_t proc_find(const char* name, pid_t _knownpid)
// {
//     if(_knownpid != -1 && test_pid(_knownpid))
//         return _knownpid;
//     return proc_find(name);
// }


//eof proc_find.cpp