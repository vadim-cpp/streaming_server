#include "logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init() 
{
    try 
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/server.log", 1024 * 1024 * 5, 3);
        
        spdlog::sinks_init_list sinks = {console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("main", sinks);
        
        #ifdef DEBUG
        logger_->set_level(spdlog::level::debug);
        #else
        logger_->set_level(spdlog::level::info);
        #endif
        
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    } 
    catch (const spdlog::spdlog_ex& ex) 
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger> Logger::get() 
{
    return logger_;
}
