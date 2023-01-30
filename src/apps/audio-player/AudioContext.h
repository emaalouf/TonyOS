#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "AudioTrack.h"

#define AUDIOCONTEXT_NUM_SAMPLE_BUFFERS 16

class AudioContext {
    friend void PlayAudio(AudioContext*);

public:
    struct SampleBuffer {
        uint8_t* data;
        int samples;

        float timestamp;
    };

    AudioContext();
    ~AudioContext();

    inline bool HasLoadedAudio() const { return m_isDecoderRunning; }
    inline bool IsAudioPlaying() const { return m_shouldPlayAudio; }
    inline bool ShouldPlayNextTrack() const { return m_shouldPlayNextTrack; }

    float PlaybackProgress() const;
    const TrackInfo* CurrentTrack() const { return m_currentTrack; }

    void PlaybackStart();
    void PlaybackPause();
    void PlaybackStop();
    void PlaybackSeek(float timestamp);
    int PlayTrack(TrackInfo* info);
    int LoadTrack(std::string filepath, TrackInfo* info);

    SampleBuffer sampleBuffers[AUDIOCONTEXT_NUM_SAMPLE_BUFFERS];
    ssize_t samplesPerBuffer;
    int lastSampleBuffer;
    int currentSampleBuffer;
    std::mutex sampleBuffersLock;
    std::condition_variable decoderWaitCondition;
    std::condition_variable decoderShouldRunCondition;
    std::condition_variable playerShouldRunCondition;
    int numValidBuffers;

private:
    void DecodeAudio();
    void DecoderDecodeFrame(struct AVFrame* frame);
    void DecoderDoSeek();
    void PlayAudio();
    int DecoderGetNextSampleBufferOrWait(int samplesToWrite);
    inline bool IsDecoderPacketInvalid() {
        return m_requestSeek || !m_isDecoderRunning;
    }

    void GetTrackInfo(struct AVFormatContext* fmt, TrackInfo* info);

    inline void FlushSampleBuffers() {
        for (int i = 0; i < AUDIOCONTEXT_NUM_SAMPLE_BUFFERS; i++) {
            sampleBuffers[i].samples = 0;
            sampleBuffers[i].timestamp = 0;
        }
    }

    int m_pcmOut;
    int m_pcmSampleRate;
    int m_pcmChannels;
    int m_pcmBitDepth;

    TrackInfo* m_currentTrack;
    std::thread m_playbackThread;
    std::thread m_decoderThread;
    std::mutex m_decoderLock;
    std::mutex m_playerStatusLock;
    std::mutex m_decoderStatusLock;

    bool m_shouldThreadsDie = false;
    bool m_isDecoderRunning = false;
    bool m_shouldPlayNextTrack = false;
    bool m_shouldPlayAudio = true;
    
    bool m_requestSeek = false;
    float m_seekTimestamp;
    float m_lastTimestamp;

    struct AVFormatContext* m_avfmt = nullptr;
    struct AVCodecContext* m_avcodec = nullptr;
    struct SwrContext* m_resampler = nullptr;

    struct AVStream* m_currentStream = nullptr;
    int m_currentStreamIndex = 0;
};