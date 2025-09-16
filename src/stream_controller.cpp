#include "stream_controller.hpp"
#include "websocket_session.hpp"
#include "logger.hpp"
#include <opencv2/opencv.hpp>

StreamController::StreamController(
    net::io_context& ioc,
    std::shared_ptr<IVideoSource> video_source,
    std::shared_ptr<IAsciiConverter> ascii_converter
)
    : ioc_(ioc),
      strand_(net::make_strand(ioc)),
      frame_timer_(ioc),
      video_source_(std::move(video_source)),
      ascii_converter_(std::move(ascii_converter)),
      record_controller_(std::make_shared<RecordController>(ioc))
{}

StreamController::~StreamController() 
{
    cleanup();
}

net::awaitable<void> StreamController::start_streaming(int camera_index, const std::string& resolution, int fps) 
{
    auto logger = Logger::get();
    
    co_await net::dispatch(strand_, net::use_awaitable);
    
    if (is_streaming_) 
    {
        logger->warn("Streaming already in progress");
        co_return;
    }
    
    try 
    {
        size_t pos = resolution.find('x');
        frame_width_ = std::stoi(resolution.substr(0, pos));
        frame_height_ = std::stoi(resolution.substr(pos + 1));
        fps_ = fps;
        
        video_source_->open(camera_index);
        video_source_->set_resolution(frame_width_ * 2, frame_height_ * 2);
        ascii_converter_->set_ascii_chars("@%#*+=-:. ");
        
        is_streaming_ = true;
        stop_requested_ = false;
        
        logger->info("Starting streaming from camera {}", camera_index);
        
        net::co_spawn(strand_, 
            [self = shared_from_this()] { return self->capture_loop(); },
            net::detached);
            
    } 
    catch (const std::exception& e) 
    {
        logger->error("Failed to start streaming: {}", e.what());
        is_streaming_ = false;
        throw;
    }
}

net::awaitable<void> StreamController::stop_streaming() 
{
    auto logger = Logger::get();
    
    co_await net::dispatch(strand_, net::use_awaitable);
    
    if (!is_streaming_) 
    {
        co_return;
    }
    
    stop_requested_ = true;
    capture_cancel_.emit(net::cancellation_type::all);
    frame_timer_.cancel();
    
    while (is_streaming_) 
    {
        co_await net::steady_timer(ioc_, std::chrono::milliseconds(10)).async_wait(
            as_tuple(net::use_awaitable));
    }
    
    cleanup();
    logger->info("Streaming stopped");
}

bool StreamController::is_streaming() const 
{
    return is_streaming_.load();
}

void StreamController::add_viewer(std::shared_ptr<WebSocketSession> viewer) 
{
    net::post(strand_, 
        [self = shared_from_this(), viewer] {
            self->viewers_.push_back(viewer);
        });
}

net::awaitable<void> StreamController::remove_viewer(std::shared_ptr<WebSocketSession> viewer) 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    
    viewers_.erase(
        std::remove_if(viewers_.begin(), viewers_.end(),
            [&viewer](const std::weak_ptr<WebSocketSession>& wp) {
                return wp.expired() || wp.lock() == viewer;
            }),
        viewers_.end());
}

net::awaitable<void> StreamController::remove_viewer_by_id(uint64_t session_id) 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    
    viewers_.erase(
        std::remove_if(viewers_.begin(), viewers_.end(),
            [session_id](const std::weak_ptr<WebSocketSession>& wp) {
                if (auto viewer = wp.lock()) {
                    return viewer->session_id() == session_id;
                }
                return true;
            }),
        viewers_.end());
}

net::awaitable<void> StreamController::capture_loop() 
{
    auto logger = Logger::get();
    
    try 
    {
        while (!stop_requested_ && is_streaming_) 
        {
            co_await net::steady_timer(ioc_, std::chrono::milliseconds(1000 / fps_)).async_wait(as_tuple(net::use_awaitable));

            cv::Mat frame = video_source_->capture_frame();
            if (frame.empty()) 
            {
                logger->warn("Empty frame captured");
                continue;
            }
            
            std::string ascii_frame = ascii_converter_->convert(frame, frame_width_, frame_height_);
            
            if (record_controller_->is_recording()) 
            {
                record_controller_->write_frame(ascii_frame);
            }
            
            co_await broadcast_frame(ascii_frame);
        }
    } 
    catch (const std::exception& e) 
    {
        logger->error("Error in capture loop: {}", e.what());
    }
    
    is_streaming_ = false;
}

net::awaitable<void> StreamController::broadcast_frame(const std::string& frame) 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    
    for (auto it = viewers_.begin(); it != viewers_.end(); ) 
    {
        if (auto viewer = it->lock()) 
        {
            viewer->send_frame(frame);
            ++it;
        } 
        else 
        {
            it = viewers_.erase(it);
        }
    }
}

void StreamController::cleanup() 
{
    if (video_source_) 
    {
        video_source_->close();
    }
    
    viewers_.clear();
    is_streaming_ = false;
}

std::string StreamController::get_status() const 
{
    return is_streaming_ ? "active" : "inactive";
}

void StreamController::start_recording() 
{
    auto logger = Logger::get();
    
    if (record_controller_->is_recording()) 
    {
        logger->warn("Recording already in progress");
        return;
    }
    
    if (record_controller_->start_recording()) 
    {
        logger->info("Recording started");
    } 
    else 
    {
        logger->error("Failed to start recording");
    }
}

void StreamController::stop_recording() 
{
    if (record_controller_->is_recording()) 
    {
        record_controller_->stop_recording();
        auto logger = Logger::get();
        logger->info("Recording stopped");
    }
}

bool StreamController::is_recording() const 
{
    return record_controller_->is_recording();
}

net::awaitable<void> StreamController::start_playback(const std::string& filename, 
                                                     std::shared_ptr<WebSocketSession> session) 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    
    // Stop any current playback
    co_await stop_playback();
    
    // Load the recording
    if (playback_controller_->load_recording("recordings/" + filename)) 
    {
        playback_session_ = session;
        
        // Start playback
        playback_controller_->start_playback([self = shared_from_this()](const std::string& frame) {
            // Send frame to playback session
            if (self->playback_session_) 
            {
                self->playback_session_->send_frame(frame);
            }
        });
    }
}

net::awaitable<void> StreamController::pause_playback() 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    playback_controller_->pause_playback();
}

net::awaitable<void> StreamController::resume_playback() 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    playback_controller_->resume_playback();
}

net::awaitable<void> StreamController::stop_playback() 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    playback_controller_->stop_playback();
    playback_session_.reset();
}

net::awaitable<void> StreamController::set_playback_speed(double speed) 
{
    co_await net::dispatch(strand_, net::use_awaitable);
    playback_controller_->set_playback_speed(speed);
}