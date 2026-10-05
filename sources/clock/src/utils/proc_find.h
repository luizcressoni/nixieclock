/*! \file proc_find.h*/
#pragma once

#include <stdlib.h>
bool test_pid(pid_t pid);
pid_t proc_find(const char* name);
//pid_t proc_find(const char* name, pid_t _knownpid);

//eof proc_find.h