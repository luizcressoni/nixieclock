/*! \file logger.h */
#pragma once

#include "spdlog/spdlog.h"

extern spdlog::logger *glogger;

void init_logger(const char *filename);

#define LOGGER_ERROR(A)     if(glogger) glogger->error(   "<{:s}> {:s}\r", __func__, A)
#define LOGGER_DEBUG(A)     if(glogger) glogger->debug(   "<{:s}> {:s}\r", __func__, A)
#define LOGGER_INFO(A)      if(glogger) glogger->info(    "<{:s}> {:s}\r", __func__, A)
#define LOGGER_WARN(A)      if(glogger) glogger->warn(    "<{:s}> {:s}\r", __func__, A)
#define LOGGER_TRACE(A)     if(glogger) glogger->trace(   "<{:s}> {:s}\r", __func__, A)
#define LOGGER_CRITICAL(A)  if(glogger) glogger->critical("<{:s}> {:s}\r", __func__, A)