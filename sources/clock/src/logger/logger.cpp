/*! \file logger.cpp */

#include "logger.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
spdlog::logger *glogger = nullptr;
std::shared_ptr<spdlog::sinks::ansicolor_stdout_sink<spdlog::details::console_mutex>> console_sink;
std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink;

/*!\ brief initializes the logger.
 * <br> We are supporting both console and circular files
 * <br> and all levels of log are enabled by default
 */
void init_logger(const char *filename)
{
    char file[64];
    sprintf(file, "/tmp/%s.txt", filename);
    console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 1048576, 1);
    glogger = new spdlog::logger(filename, {console_sink, file_sink});

    LOGGER_INFO("***** Log start *****");
}
