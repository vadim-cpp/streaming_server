#include "audio_capture.hpp"
#include "logger.hpp"
#include <stdexcept>
#include <thread>

AudioCapture::AudioCapture() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("PortAudio initialization failed: " + std::string(Pa_GetErrorText(err)));
    }
}

AudioCapture::~AudioCapture() {
    stop_capture();
    Pa_Terminate();
}

std::vector<AudioCapture::MicrophoneInfo> AudioCapture::list_microphones() {
    std::vector<MicrophoneInfo> mics;
    int num_devices = Pa_GetDeviceCount();
    
    auto logger = Logger::get();
    logger->info("Scanning {} audio devices", num_devices);
    
    for (int i = 0; i < num_devices; i++) {
        const PaDeviceInfo* device_info = Pa_GetDeviceInfo(i);
        if (device_info && device_info->maxInputChannels > 0) {
            mics.push_back({
                i,
                device_info->name,
                std::to_string(i)
            });
            logger->debug("Found microphone: {} (index: {})", device_info->name, i);
        }
    }
    
    logger->info("Found {} microphones", mics.size());
    return mics;
}

bool AudioCapture::start_capture(int device_index, AudioCallback callback) {
    auto logger = Logger::get();
    
    if (is_capturing_) {
        logger->warn("Audio capture already running");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        audio_callback_ = std::move(callback);
    }

    PaStreamParameters input_parameters;
    input_parameters.device = device_index;
    input_parameters.channelCount = 1;
    input_parameters.sampleFormat = paInt16;
    input_parameters.suggestedLatency = Pa_GetDeviceInfo(device_index)->defaultLowInputLatency;
    input_parameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream_,
        &input_parameters,
        nullptr,
        16000,  // sample rate
        512,    // frames per buffer
        paClipOff,
        &AudioCapture::audio_callback,
        this
    );

    if (err != paNoError) {
        logger->error("Failed to open audio stream: {}", Pa_GetErrorText(err));
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        logger->error("Failed to start audio stream: {}", Pa_GetErrorText(err));
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    is_capturing_ = true;
    logger->info("Audio capture started on device index {}", device_index);
    return true;
}

void AudioCapture::stop_capture() {
    if (stream_ && is_capturing_) {
        auto logger = Logger::get();
        
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        is_capturing_ = false;
        
        std::lock_guard<std::mutex> lock(callback_mutex_);
        audio_callback_ = nullptr;
        
        logger->info("Audio capture stopped");
    }
}

int AudioCapture::audio_callback(const void* input_buffer, void* output_buffer,
                                unsigned long frames_per_buffer,
                                const PaStreamCallbackTimeInfo* time_info,
                                PaStreamCallbackFlags status_flags,
                                void* user_data) {
    AudioCapture* self = static_cast<AudioCapture*>(user_data);
    
    std::lock_guard<std::mutex> lock(self->callback_mutex_);
    if (self->audio_callback_) {
        const int16_t* samples = static_cast<const int16_t*>(input_buffer);
        std::vector<int16_t> audio_data(samples, samples + frames_per_buffer);
        self->audio_callback_(audio_data);
    }
    
    return paContinue;
}