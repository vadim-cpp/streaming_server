#pragma once

#include "video_source_interface.hpp"
#include "ascii_converter_interface.hpp"

#include <memory>
#include <string>
#include <vector>
#include <atomic>
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
};