#pragma once

#include <portaudio.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

class AudioCapture 
{
public:
    struct MicrophoneInfo 
    {
        int index;
        std::string name;
        std::string id;
    };

    using AudioCallback = std::function<void(const std::vector<int16_t>& audio_data)>;

    AudioCapture();
    ~AudioCapture();

    std::vector<MicrophoneInfo> list_microphones();
    bool start_capture(int device_index, AudioCallback callback);
    void stop_capture();
    bool is_capturing() const { return is_capturing_; }

private:
    static int audio_callback(const void* input_buffer, void* output_buffer,
                             unsigned long frames_per_buffer,
                             const PaStreamCallbackTimeInfo* time_info,
                             PaStreamCallbackFlags status_flags,
                             void* user_data);

    PaStream* stream_ = nullptr;
    std::atomic<bool> is_capturing_{false};
    AudioCallback audio_callback_;
    std::mutex callback_mutex_;
};