#include "playback_controller.hpp"
#include "logger.hpp"

#include <sstream>
#include <iostream>

PlaybackController::PlaybackController(boost::asio::io_context& ioc) 
    : ioc_(ioc), playback_timer_(ioc) 
{
}

PlaybackController::~PlaybackController() 
{
    stop_playback();
}

bool PlaybackController::load_recording(const std::string& filename) 
{
    auto logger = Logger::get();
    
    stop_playback();
    playback_file_.open(filename, std::ios::in | std::ios::binary);
    
    if (!playback_file_.is_open()) 
    {
        logger->error("Failed to open playback file: {}", filename);
        return false;
    }
    
    // Read and validate header
    std::string header;
    std::getline(playback_file_, header);
    if (header != "ASCII_STREAM_RECORD") 
    {
        logger->error("Invalid recording file format");
        playback_file_.close();
        return false;
    }
    
    // Read metadata
    current_recording_info_ = {};
    current_recording_info_.filename = filename;
    
    std::string line;
    while (std::getline(playback_file_, line) && line != "frames:") 
    {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) 
        {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            if (key == "timestamp") 
            {
                current_recording_info_.timestamp = value;
            } 
            else if (key == "end_time") 
            {
                current_recording_info_.duration = std::stoi(value);
            } 
            else if (key == "frame_count") 
            {
                current_recording_info_.frame_count = std::stoi(value);
            }
        }
    }
    
    // Get file size
    playback_file_.seekg(0, std::ios::end);
    current_recording_info_.file_size = playback_file_.tellg();
    playback_file_.seekg(0, std::ios::beg);
    
    // Skip to frames section
    while (std::getline(playback_file_, line) && line != "frames:") 
    {
        // Continue until we find the frames section
    }
    
    logger->info("Loaded recording: {} ({} frames, {}s)", 
                filename, current_recording_info_.frame_count, current_recording_info_.duration);
    
    return true;
}

void PlaybackController::start_playback(std::function<void(const std::string&)> frame_callback) 
{
    auto logger = Logger::get();
    
    if (!playback_file_.is_open()) 
    {
        logger->error("No recording loaded for playback");
        return;
    }
    
    if (is_playing_) 
    {
        logger->warn("Playback already in progress");
        return;
    }
    
    frame_callback_ = frame_callback;
    is_playing_ = true;
    is_paused_ = false;
    current_frame_ = 0;
    next_frame_time_ = 0;
    
    // Start playback
    read_next_frame();
    logger->info("Playback started");
}

void PlaybackController::pause_playback() 
{
    if (is_playing_ && !is_paused_) 
    {
        is_paused_ = true;
        playback_timer_.cancel();
    }
}

void PlaybackController::resume_playback() 
{
    if (is_playing_ && is_paused_) 
    {
        is_paused_ = false;
        read_next_frame();
    }
}

void PlaybackController::stop_playback() 
{
    if (is_playing_) 
    {
        is_playing_ = false;
        is_paused_ = false;
        playback_timer_.cancel();
        
        if (playback_file_.is_open()) 
        {
            playback_file_.close();
        }
    }
}

void PlaybackController::set_playback_speed(double speed) 
{
    playback_speed_ = std::max(0.1, std::min(10.0, speed));
}

bool PlaybackController::is_playing() const 
{
    return is_playing_;
}

bool PlaybackController::is_paused() const 
{
    return is_paused_;
}

PlaybackController::RecordingInfo PlaybackController::get_recording_info() const 
{
    return current_recording_info_;
}

void PlaybackController::read_next_frame() 
{
    if (!is_playing_ || is_paused_ || !playback_file_) 
    {
        return;
    }
    
    std::string frame_header;
    if (!std::getline(playback_file_, frame_header)) 
    {
        // End of file
        stop_playback();
        return;
    }
    
    // Parse frame header: "frame:timestamp:length"
    size_t colon1 = frame_header.find(':');
    size_t colon2 = frame_header.find(':', colon1 + 1);
    
    if (colon1 == std::string::npos || colon2 == std::string::npos) 
    {
        auto logger = Logger::get();
        logger->error("Invalid frame header in playback file");
        stop_playback();
        return;
    }
    
    long long timestamp = std::stoll(frame_header.substr(colon1 + 1, colon2 - colon1 - 1));
    size_t length = std::stoul(frame_header.substr(colon2 + 1));
    
    // Read frame data
    std::string frame_data(length, '\0');
    playback_file_.read(&frame_data[0], length);
    playback_file_.ignore(1); // Skip newline
    
    // Calculate delay until next frame
    long long delay_ms = 0;
    if (current_frame_ > 0) 
    {
        delay_ms = timestamp - next_frame_time_;
    }
    next_frame_time_ = timestamp;
    
    // Call callback with frame data
    if (frame_callback_) 
    {
        frame_callback_(frame_data);
    }
    
    current_frame_++;
    
    // Schedule next frame
    playback_timer_.expires_after(std::chrono::milliseconds(static_cast<int>(delay_ms / playback_speed_)));
    playback_timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec && is_playing_ && !is_paused_) 
        {
            read_next_frame();
        }
    });
}