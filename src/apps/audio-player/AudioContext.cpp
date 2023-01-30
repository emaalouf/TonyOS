#include "AudioContext.h"

#include <Tony/Core/Logger.h>
#include <Tony/System/ABI/Audio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>

#include <sys/ioctl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}
void AudioContext::PlayAudio() {
    int fd = m_pcmOut;
    int channels = m_pcmChannels;
    int sampleSize = m_pcmBitDepth / 8;

    AudioContext::SampleBuffer* buffers = sampleBuffers;

    while (!m_shouldThreadsDie) {
        if (!m_shouldPlayAudio) {
            std::unique_lock lockStatus{m_playerStatusLock};
            playerShouldRunCondition.wait(lockStatus, [this]() -> bool { return m_shouldPlayAudio; });
        }
        while (m_shouldPlayAudio && m_isDecoderRunning) {
            if (!numValidBuffers) {
                usleep(5000);
                continue;
            };
            currentSampleBuffer = (currentSampleBuffer + 1) % AUDIOCONTEXT_NUM_SAMPLE_BUFFERS;

            auto& buffer = buffers[currentSampleBuffer];
            m_lastTimestamp = buffer.timestamp;
            int ret = write(fd, buffer.data, buffer.samples * channels * sampleSize);
            if (ret < 0) {
                Tony::Logger::Warning("/snd/dev/pcm: Error writing samples: {}", strerror(errno));
            }
            buffer.samples = 0;
            std::unique_lock lock{sampleBuffersLock};
            if (numValidBuffers > 0) {
                numValidBuffers--;
            }
            lock.unlock();
            decoderWaitCondition.notify_all();
        }
    }
}

AudioContext::AudioContext() {
    m_pcmOut = open("/dev/snd/pcm", O_WRONLY);
    if (m_pcmOut < 0) {
        Tony::Logger::Error("Failed to open PCM output '/dev/snd/pcm': {}", strerror(errno));
        exit(1);
    }
    m_pcmChannels = ioctl(m_pcmOut, IoCtlOutputGetNumberOfChannels);
    if (m_pcmChannels <= 0) {
        Tony::Logger::Error("/dev/snd/pcm IoCtlOutputGetNumberOfChannels: {}", strerror(errno));
        exit(1);
    }
    assert(m_pcmChannels == 1 || m_pcmChannels == 2);
    int outputEncoding = ioctl(m_pcmOut, IoCtlOutputGetEncoding);
    if (outputEncoding < 0) {
        Tony::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: {}", strerror(errno));
        exit(1);
    }
    if (outputEncoding == PCMS16LE) {
        m_pcmBitDepth = 16;
    } else if (outputEncoding == PCMS20LE) {
        m_pcmBitDepth = 20;
    } else {
        Tony::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: Encoding not supported");
    }
    if (m_pcmBitDepth != 16) {
        Tony::Logger::Error("/dev/snd/pcm Unsupported PCM sample depth of {}", m_pcmBitDepth);
        exit(1);
    }
    m_pcmSampleRate = ioctl(m_pcmOut, IoCtlOutputGetSampleRate);
    if (m_pcmSampleRate < 0) {
        Tony::Logger::Error("/dev/snd/pcm IoCtlOutputGetSampleRate: {}", strerror(errno));
        exit(1);
    }
    samplesPerBuffer = m_pcmSampleRate / 10;
    for (int i = 0; i < AUDIOCONTEXT_NUM_SAMPLE_BUFFERS; i++) {
        sampleBuffers[i].data = new uint8_t[samplesPerBuffer * m_pcmChannels * (m_pcmBitDepth / 8)];
        sampleBuffers[i].samples = 0;
    }

    m_decoderThread = std::thread(&AudioContext::DecodeAudio, this);
}

AudioContext::~AudioContext() {
    m_shouldThreadsDie = true;

    PlaybackStop();
}

void AudioContext::DecodeAudio() {
    std::thread playerThread(&AudioContext::PlayAudio, this);

    while (!m_shouldThreadsDie) {
        {
            std::unique_lock lockStatus{m_decoderStatusLock};
            decoderShouldRunCondition.wait(lockStatus, [this]() -> bool { return m_isDecoderRunning; });
        }

        m_decoderLock.lock();
        m_resampler = swr_alloc();
        if (m_avcodec->channels == 1) {
            av_opt_set_int(m_resampler, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            if (m_avcodec->channels != 2) {
                Tony::Logger::Warning("Unsupported number of audio channels {}, taking first 2 and playing as stereo.",
                                       m_avcodec->channels);
            }

            av_opt_set_int(m_resampler, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }
        av_opt_set_int(m_resampler, "in_sample_rate", m_avcodec->sample_rate, 0);
        av_opt_set_sample_fmt(m_resampler, "in_sample_fmt", m_avcodec->sample_fmt, 0);
        if (m_pcmChannels == 1) {
            av_opt_set_int(m_resampler, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            av_opt_set_int(m_resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }
        av_opt_set_int(m_resampler, "out_sample_rate", m_pcmSampleRate, 0);
        av_opt_set_sample_fmt(m_resampler, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        assert(m_pcmBitDepth == 16);
        lastSampleBuffer = 0;
        currentSampleBuffer = 0;
        numValidBuffers = 0;
        FlushSampleBuffers();

        if (swr_init(m_resampler)) {
            Tony::Logger::Error("Could not initialize software resampler");
            
            std::unique_lock lockStatus{m_decoderStatusLock};
            m_isDecoderRunning = false;
        } else {
            PlaybackStart();
        }

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        int frameResult = 0;
        while (m_isDecoderRunning && (frameResult = av_read_frame(m_avfmt, packet)) >= 0) {
            if (packet->stream_index != m_currentStreamIndex) {
                av_packet_unref(packet);
                continue;
            }

            if (int ret = avcodec_send_packet(m_avcodec, packet); ret) {
                Tony::Logger::Error("Could not send packet for decoding");
                break;
            }

            ssize_t ret = 0;
            while (!IsDecoderPacketInvalid() && ret >= 0) {
                ret = avcodec_receive_frame(m_avcodec, frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    // Get the next packet and retry
                    break;
                } else if (ret) {
                    Tony::Logger::Error("Could not decode frame: {}", ret);
                    // Stop decoding audio
                    m_isDecoderRunning = false;
                    break;
                }

                DecoderDecodeFrame(frame);
                
                av_frame_unref(frame);
            }

            av_packet_unref(packet);
            if (m_isDecoderRunning && m_requestSeek) {
                DecoderDoSeek();
            }
        }
        if (frameResult == AVERROR_EOF) {
            m_shouldPlayNextTrack = true;
        }
        std::unique_lock lockStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        numValidBuffers = 0;

        swr_free(&m_resampler);
        m_resampler = nullptr;
        avcodec_free_context(&m_avcodec);

        avformat_free_context(m_avfmt);
        m_avfmt = nullptr;

        m_decoderLock.unlock();
    }

    // Wait for playerThread to finish
    playerThread.join();
}

void AudioContext::DecoderDecodeFrame(AVFrame* frame) {
    // Bytes per audio sample
    int outputSampleSize = (m_pcmBitDepth / 8);
    if (IsDecoderPacketInvalid()) {
        return;
    }
    int samplesToWrite =
        av_rescale_rnd(swr_get_delay(m_resampler, m_avcodec->sample_rate) + frame->nb_samples,
                        m_pcmSampleRate, m_avcodec->sample_rate, AV_ROUND_UP);

    int bufferIndex = DecoderGetNextSampleBufferOrWait(samplesToWrite);
    if(bufferIndex < 0) {
        // DecoderGetNextSampleBufferOrWait will return a value < 0
        // if the packet is invalid, so just return
        return;
    }

    auto* buffer = &sampleBuffers[bufferIndex];
    uint8_t* outputData = buffer->data + (buffer->samples * outputSampleSize) * m_pcmChannels;

    int samplesWritten = swr_convert(m_resampler, &outputData, (int)(samplesPerBuffer - buffer->samples),
                                        (const uint8_t**)frame->extended_data, frame->nb_samples);
    buffer->samples += samplesWritten;

    buffer->timestamp = frame->best_effort_timestamp / m_currentStream->time_base.den;
}

int AudioContext::DecoderGetNextSampleBufferOrWait(int samplesToWrite) {
    int nextValidBuffer = (lastSampleBuffer + 1) % AUDIOCONTEXT_NUM_SAMPLE_BUFFERS;
    auto hasBuffer = [&]() -> bool {
        return nextValidBuffer != currentSampleBuffer || numValidBuffers == 0 || IsDecoderPacketInvalid();
    };

    std::unique_lock lock{sampleBuffersLock};
    decoderWaitCondition.wait(lock, hasBuffer);

    if (IsDecoderPacketInvalid()) {
        return -1;
    }

    auto* buffer = &sampleBuffers[nextValidBuffer];
    if (samplesToWrite + buffer->samples > samplesPerBuffer) {
        lastSampleBuffer = nextValidBuffer;
        numValidBuffers++;

        nextValidBuffer = (lastSampleBuffer + 1) % AUDIOCONTEXT_NUM_SAMPLE_BUFFERS;
        decoderWaitCondition.wait(lock, hasBuffer);
        if (IsDecoderPacketInvalid()) {
            return -1;
        }
    }
    return nextValidBuffer;
}

void AudioContext::DecoderDoSeek() {
    assert(m_requestSeek);
    std::scoped_lock lock{sampleBuffersLock};
    numValidBuffers = 0;
    lastSampleBuffer = currentSampleBuffer;
    FlushSampleBuffers();
    avcodec_flush_buffers(m_avcodec);
    swr_convert(m_resampler, NULL, 0, NULL, 0);
    float timestamp = m_seekTimestamp;
    av_seek_frame(m_avfmt, m_currentStreamIndex,
                    (long)(timestamp / av_q2d(m_currentStream->time_base)), 0);

    m_lastTimestamp = m_seekTimestamp;
    m_requestSeek = false;
}

float AudioContext::PlaybackProgress() const {
    if (!m_isDecoderRunning) {
        return 0;
    }
    return m_lastTimestamp;
}

void AudioContext::PlaybackStart() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = true;
    playerShouldRunCondition.notify_all();
}

void AudioContext::PlaybackPause() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = false;
}

void AudioContext::PlaybackStop() {
    if (m_isDecoderRunning) {
        // Mark the decoder as not running
        std::unique_lock lockDecoderStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        m_shouldPlayAudio = false;
        // Make the decoder stop waiting for a free sample buffer
        decoderWaitCondition.notify_all();
    }
    m_currentTrack = nullptr;
}

void AudioContext::PlaybackSeek(float timestamp) {
    if (m_isDecoderRunning) {
        m_seekTimestamp = timestamp;
        m_requestSeek = true;
        decoderWaitCondition.notify_all();
        while (m_requestSeek)
            usleep(1000); 
    }
}

int AudioContext::PlayTrack(TrackInfo* info) {
    if (m_isDecoderRunning) {
        PlaybackStop();
    }

    std::lock_guard lockDecoder(m_decoderLock);

    assert(!m_avfmt);
    m_avfmt = avformat_alloc_context();

    // Opens the audio file
    if (int err = avformat_open_input(&m_avfmt, info->filepath.c_str(), NULL, NULL); err) {
        Tony::Logger::Error("Failed to open {}", info->filepath);
        return err;
    }

    // Gets metadata and information about the audio contained in the file
    if (int err = avformat_find_stream_info(m_avfmt, NULL); err) {
        Tony::Logger::Error("Failed to get stream info for {}", info->filepath);
        return err;
    }

    // Update track metadata in case the file has changed
    GetTrackInfo(m_avfmt, info);
    int streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Tony::Logger::Error("Failed to get audio stream for {}", info->filepath);
        return streamIndex;
    }

    m_currentStream = m_avfmt->streams[streamIndex];
    m_currentStreamIndex = streamIndex;
    const AVCodec* decoder = avcodec_find_decoder(m_currentStream->codecpar->codec_id);
    if (!decoder) {
        Tony::Logger::Error("Failed to find codec for '{}'", info->filepath);
        return 1;
    }

    m_avcodec = avcodec_alloc_context3(decoder);
    assert(decoder);

    if (avcodec_parameters_to_context(m_avcodec, m_currentStream->codecpar)) {
        Tony::Logger::Error("Failed to initialie codec context.");
        return 1;
    }

    if (avcodec_open2(m_avcodec, decoder, NULL) < 0) {
        Tony::Logger::Error("Failed to open codec!");
        return 1;
    }
    m_currentTrack = info;
    m_requestSeek = false;
    m_shouldPlayNextTrack = false;
    std::scoped_lock lockDecoderStatus{m_decoderStatusLock};
    m_isDecoderRunning = true;
    decoderShouldRunCondition.notify_all();
    return 0;
}

int AudioContext::LoadTrack(std::string filepath, TrackInfo* info) {
    AVFormatContext* fmt = avformat_alloc_context();
    if (int r = avformat_open_input(&fmt, filepath.c_str(), NULL, NULL); r) {
        Tony::Logger::Error("Failed to open {}", filepath);
        return r;
    }

    if (int r = avformat_find_stream_info(fmt, NULL); r) {
        avformat_free_context(fmt);
        return r;
    }

    // Fill the TrackInfo
    info->filepath = filepath;
    if (size_t lastSeparator = info->filepath.find_last_of('/'); lastSeparator != std::string::npos) {
        info->filename = filepath.substr(lastSeparator + 1);
    } else {
        info->filename = filepath;
    }

    GetTrackInfo(fmt, info);

    avformat_free_context(fmt);
    return 0;
}

void AudioContext::GetTrackInfo(struct AVFormatContext* fmt, TrackInfo* info) {
    float durationSeconds = fmt->duration / 1000000.f;
    info->duration = durationSeconds;
    info->durationString = fmt::format("{:02}:{:02}", (int)durationSeconds / 60, (int)durationSeconds % 60);
    AVDictionaryEntry* tag = nullptr;
    tag = av_dict_get(fmt->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.title = tag->value;
    } else {
        info->metadata.title = "Unknown";
    }

    tag = av_dict_get(fmt->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.artist = tag->value;
    } else {
        info->metadata.artist = "";
    }

    tag = av_dict_get(fmt->metadata, "album", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.album = tag->value;
    } else {
        info->metadata.album = "";
    }

    tag = av_dict_get(fmt->metadata, "date", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        // First 4 digits are year
        info->metadata.year = std::string(tag->value).substr(0, 4);
    }
}
