#include "record_controller.hpp"
#include "logger.hpp"
#include <iomanip>
#include <sstream>

RecordController::RecordController(boost::asio::io_context& ioc) 
    : ioc_(ioc) 
{
    // Create recordings directory if it doesn't exist
    std::filesystem::create_directories("recordings");
}

RecordController::~RecordController() 
{
    stop_recording();
}

bool RecordController::start_recording() 
{
    auto logger = Logger::get();
    
    if (is_recording_) 
    {
        logger->warn("Recording already in progress");
        return false;
    }
    
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "recordings/ascii_stream_" << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S") << ".asr";
    
    filename_ = ss.str();
    record_file_.open(filename_, std::ios::out | std::ios::binary);
    
    if (!record_file_.is_open()) 
    {
        logger->error("Failed to open recording file: {}", filename_);
        return false;
    }
    
    // Write header
    record_file_ << "ASCII_STREAM_RECORD\n";
    record_file_ << "version:1.0\n";
    record_file_ << "timestamp:" << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << "\n";
    record_file_ << "frames:\n";
    
    is_recording_ = true;
    start_time_ = std::chrono::steady_clock::now();
    frame_count_ = 0;
    
    logger->info("Started recording to file: {}", filename_);
    return true;
}

void RecordController::stop_recording() 
{
    if (is_recording_) 
    {
        auto logger = Logger::get();
        auto duration = std::chrono::steady_clock::now() - start_time_;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        
        // Write footer with metadata
        record_file_ << "end_time:" << seconds << "\n";
        record_file_ << "frame_count:" << frame_count_ << "\n";
        record_file_.close();
        
        is_recording_ = false;
        logger->info("Stopped recording to file: {} (duration: {}s, frames: {})", 
                    filename_, seconds, frame_count_);
    }
}

void RecordController::write_frame(const std::string& frame) 
{
    if (is_recording_ && record_file_.is_open()) 
    {
        auto now = std::chrono::steady_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();
        
        record_file_ << "frame:" << timestamp << ":" << frame.length() << "\n";
        record_file_ << frame << "\n";
        frame_count_++;
    }
}

bool RecordController::is_recording() const 
{
    return is_recording_;
}

std::string RecordController::get_current_filename() const 
{
    return filename_;
}