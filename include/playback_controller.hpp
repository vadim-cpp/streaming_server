#pragma once

#include <fstream>
#include <string>
#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <boost/asio.hpp>

class PlaybackController 
{
public:
    PlaybackController(boost::asio::io_context& ioc);
    ~PlaybackController();
    
    struct RecordingInfo {
        std::string filename;
        std::string timestamp;
        int duration;
        int frame_count;
        size_t file_size;
    };
    
    bool load_recording(const std::string& filename);
    void start_playback(std::function<void(const std::string&)> frame_callback);
    void pause_playback();
    void resume_playback();
    void stop_playback();
    void set_playback_speed(double speed);
    
    bool is_playing() const;
    bool is_paused() const;
    RecordingInfo get_recording_info() const;
    
private:
    void read_next_frame();
    
    boost::asio::io_context& ioc_;
    boost::asio::steady_timer playback_timer_;
    std::ifstream playback_file_;
    std::atomic<bool> is_playing_{false};
    std::atomic<bool> is_paused_{false};
    std::string filename_;
    std::function<void(const std::string&)> frame_callback_;
    double playback_speed_{1.0};
    std::chrono::milliseconds frame_interval_{100};
    RecordingInfo current_recording_info_;
    size_t current_frame_{0};
    long long next_frame_time_{0};
};