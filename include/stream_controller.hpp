#pragma once

#include "video_source_interface.hpp"
#include "ascii_converter_interface.hpp"
#include "record_controller.hpp"
#include "playback_controller.hpp"
#include "subtitle_receiver.hpp"
#include "audio_capture.hpp"
#include "subtitle_client.hpp"

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace net = boost::asio;
using net::as_tuple;
using namespace net::experimental::awaitable_operators;

class WebSocketSession;

class StreamController : public std::enable_shared_from_this<StreamController> 
{
public:
    StreamController(
        net::io_context& ioc,
        std::shared_ptr<IVideoSource> video_source,
        std::shared_ptr<IAsciiConverter> ascii_converter
    );
    ~StreamController();
    
    net::awaitable<void> start_streaming(int camera_index, const std::string& resolution, int fps);
    net::awaitable<void> stop_streaming();
    bool is_streaming() const;
    
    void add_viewer(std::shared_ptr<WebSocketSession> viewer);
    net::awaitable<void> remove_viewer(std::shared_ptr<WebSocketSession> viewer);
    net::awaitable<void> remove_viewer_by_id(uint64_t session_id);
    
    std::string get_status() const;

    void start_recording();
    void stop_recording();
    bool is_recording() const;

    net::awaitable<void> start_playback(const std::string& filename, 
                                       std::shared_ptr<WebSocketSession> session);
    net::awaitable<void> pause_playback();
    net::awaitable<void> resume_playback();
    net::awaitable<void> stop_playback();
    net::awaitable<void> set_playback_speed(double speed);

    void enable_subtitles(const std::string& host, const std::string& port);
    void disable_subtitles();

    // Аудио методы
    std::vector<AudioCapture::MicrophoneInfo> list_microphones();
    void start_audio_capture(int device_index);
    void stop_audio_capture();
    bool is_audio_capturing() const;
    
    // Субтитры
    void set_subtitle_server(const std::string& host, const std::string& port);
    void set_current_subtitle(const std::string& subtitle);
    std::string get_current_subtitle();

private:
    net::awaitable<void> capture_loop();
    net::awaitable<void> broadcast_frame(const std::string& frame);
    void cleanup();

    net::io_context& ioc_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<IVideoSource> video_source_;
    std::shared_ptr<IAsciiConverter> ascii_converter_;
    std::vector<std::weak_ptr<WebSocketSession>> viewers_;
    
    std::atomic<int> frame_width_{120};
    std::atomic<int> frame_height_{90};
    std::atomic<int> fps_{10};
    std::atomic<bool> is_streaming_{false};
    std::atomic<bool> stop_requested_{false};
    
    net::steady_timer frame_timer_;
    net::cancellation_signal capture_cancel_;

    std::shared_ptr<RecordController> record_controller_;

    std::shared_ptr<PlaybackController> playback_controller_;
    std::shared_ptr<WebSocketSession> playback_session_;

    std::shared_ptr<SubtitleReceiver> subtitle_receiver_;
    std::string current_subtitle_;
    std::mutex subtitle_mutex_;

    std::shared_ptr<AudioCapture> audio_capture_;
    std::shared_ptr<SubtitleClient> subtitle_client_;
    bool subtitles_enabled_ = false;

    // Конфигурация сервера субтитров
    std::string subtitle_host_ = "localhost";
    std::string subtitle_port_ = "9001";
};