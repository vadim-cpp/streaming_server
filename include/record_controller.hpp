#pragma once

#include <fstream>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <filesystem>
#include <boost/asio.hpp>

class RecordController 
{
public:
    RecordController(boost::asio::io_context& ioc);
    ~RecordController();
    
    bool start_recording();
    void stop_recording();
    void write_frame(const std::string& frame);
    bool is_recording() const;
    std::string get_current_filename() const;
    
private:
    boost::asio::io_context& ioc_;
    std::ofstream record_file_;
    std::atomic<bool> is_recording_{false};
    std::string filename_;
    std::chrono::steady_clock::time_point start_time_;
    size_t frame_count_{0};
};