// ps5-homebrew-ui - Background music: a shuffled playlist of OGG Vorbis songs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/music.hpp"

#include "core/save_file.hpp"

#include "third_party/stb/vorbis.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hui::audio
{

namespace
{

// Music is a backdrop: 40% level under everything else, before the Music
// slider in Settings.
constexpr float kBaseGain = 0.4f;
constexpr float kDuckSeconds = 2.5f;
constexpr float kDuckGain = 0.5f;     // -6 dB
constexpr float kGapSeconds = 1.5f;   // quiet breath between songs
constexpr std::size_t kAhead = 32768; // frames kept buffered (~0.7 s)
constexpr int kChunk = 2048;          // frames decoded per step
constexpr std::size_t kMaxSongs = 64;

bool comment_value(const stb_vorbis_comment &comments, const char *key, unsigned *value)
{
    const std::size_t length = std::strlen(key);
    for (int i = 0; i < comments.comment_list_length; ++i)
    {
        const char *entry = comments.comment_list[i];
        if (std::strncmp(entry, key, length) == 0 && entry[length] == '=')
        {
            *value = static_cast<unsigned>(std::strtoul(entry + length + 1, nullptr, 10));
            return true;
        }
    }
    return false;
}

// splitmix64: the shuffle needs no cryptographic quality, just a fresh order.
std::uint64_t next_random(std::uint64_t &state)
{
    std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

} // namespace

MusicTrack::~MusicTrack()
{
    if (vorbis_ != nullptr)
        stb_vorbis_close(vorbis_);
}

std::string MusicTrack::open(std::string data)
{
    data_ = std::move(data);
    int error = 0;
    vorbis_ = stb_vorbis_open_memory(reinterpret_cast<const unsigned char *>(data_.data()),
                                     static_cast<int>(data_.size()), &error, nullptr);
    if (vorbis_ == nullptr)
        return "not a Vorbis stream (error " + std::to_string(error) + ")";
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
    if (info.sample_rate != static_cast<unsigned>(kSampleRate))
        return "sample rate " + std::to_string(info.sample_rate) + " (need 48000)";
    if (info.channels < 1 || info.channels > 2)
        return std::to_string(info.channels) + " channels (need 1 or 2)";
    channels_ = info.channels;
    const unsigned total = stb_vorbis_stream_length_in_samples(vorbis_);
    if (total == 0)
        return "empty stream";
    loop_start_ = 0;
    loop_end_ = total;
    unsigned start = 0;
    unsigned length = 0;
    const stb_vorbis_comment comments = stb_vorbis_get_comment(vorbis_);
    if (comment_value(comments, "LOOPSTART", &start) && start < total)
    {
        loop_start_ = start;
        if (comment_value(comments, "LOOPLENGTH", &length) && length > 0)
            loop_end_ = std::min(total, start + length);
    }
    return {};
}

int MusicTrack::decode(float *out, int frames)
{
    int done = 0;
    int stalls = 0;
    while (done < frames)
    {
        if (position_ >= loop_end_)
        {
            if (!looping_)
                return done; // the song is over
            if (stb_vorbis_seek(vorbis_, loop_start_) == 0)
                return done;
            position_ = loop_start_;
        }
        const int want = static_cast<int>(
            std::min<unsigned>(static_cast<unsigned>(frames - done), loop_end_ - position_));
        scratch_.resize(static_cast<std::size_t>(want) * 2);
        const int got = stb_vorbis_get_samples_float_interleaved(vorbis_, channels_,
                                                                 scratch_.data(), want * channels_);
        if (channels_ == 1)
        {
            // Spread mono to both sides, back to front so nothing is overwritten.
            for (int k = got - 1; k >= 0; --k)
            {
                const float s = scratch_[static_cast<std::size_t>(k)];
                scratch_[static_cast<std::size_t>(k) * 2] = s;
                scratch_[static_cast<std::size_t>(k) * 2 + 1] = s;
            }
        }
        if (got <= 0)
        {
            // Past the real end (a LOOPLENGTH beyond the data): end or wrap now.
            if (!looping_ || ++stalls > 2)
                return done;
            position_ = loop_end_;
            continue;
        }
        stalls = 0;
        std::memcpy(out + static_cast<std::size_t>(done) * 2, scratch_.data(),
                    static_cast<std::size_t>(got) * 2 * sizeof(float));
        done += got;
        position_ += static_cast<unsigned>(got);
    }
    return done;
}

MusicPlayer::MusicPlayer() : buffer_(static_cast<std::size_t>(kChunk) * 2)
{
}

int MusicPlayer::init(Mixer &mixer, const std::string &directory, std::uint64_t seed)
{
    mixer_ = &mixer;
    directory_ = directory;
    mixer.attach_stream(0, &ring_);
    mixer.set_stream_gain(0, kBaseGain, 0.0f);
    mixer.set_stream_gain(1, 0.0f, 0.0f);

    for (const std::string &name : save::list_files(directory))
    {
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".ogg") == 0 &&
            playlist_.size() < kMaxSongs)
            playlist_.push_back(name.substr(0, name.size() - 4));
    }
    // A fresh order every launch: sort first so the seed alone decides it.
    std::sort(playlist_.begin(), playlist_.end());
    std::uint64_t state = seed;
    for (std::size_t i = playlist_.size(); i > 1; --i)
        std::swap(playlist_[i - 1], playlist_[next_random(state) % i]);
    return static_cast<int>(playlist_.size());
}

bool MusicPlayer::next_song()
{
    track_.reset();
    current_.clear();
    while (!playlist_.empty() && failures_ < static_cast<int>(playlist_.size()))
    {
        const std::string name = playlist_[next_];
        next_ = (next_ + 1) % playlist_.size();
        std::string data;
        auto track = std::make_unique<MusicTrack>();
        std::string error = "unreadable";
        if (save::read_file(directory_ + "/" + name + ".ogg", &data, 32u << 20))
            error = track->open(std::move(data));
        if (error.empty())
        {
            track->set_looping(false);
            track_ = std::move(track);
            current_ = name;
            failures_ = 0;
            return true;
        }
        std::fprintf(stderr, "[HUI] music %s skipped: %s\n", name.c_str(), error.c_str());
        ++failures_;
    }
    return false;
}

void MusicPlayer::duck()
{
    duck_ = kDuckSeconds;
    apply_gain(0.15f);
}

void MusicPlayer::apply_gain(float seconds)
{
    if (mixer_ != nullptr)
        mixer_->set_stream_gain(0, kBaseGain * (duck_ > 0.0f ? kDuckGain : 1.0f), seconds);
}

void MusicPlayer::pump(float dt)
{
    if (mixer_ == nullptr || playlist_.empty())
        return;
    if (duck_ > 0.0f)
    {
        duck_ -= dt;
        if (duck_ <= 0.0f)
            apply_gain(0.8f); // swell back
    }
    // Keep the ring about 0.7 s ahead: the song, then a breath of silence,
    // then the next song, forever.
    while (ring_.available() < kAhead)
    {
        const int room = static_cast<int>(std::min<std::size_t>(kChunk, ring_.space()));
        if (room <= 0)
            break;
        if (gap_frames_ > 0)
        {
            const int frames = std::min(room, gap_frames_);
            std::fill(buffer_.begin(), buffer_.begin() + frames * 2, 0.0f);
            ring_.write(buffer_.data(), static_cast<std::size_t>(frames));
            gap_frames_ -= frames;
            continue;
        }
        if (!track_ && !next_song())
            break; // nothing playable
        const int got = track_->decode(buffer_.data(), room);
        if (got > 0)
            ring_.write(buffer_.data(), static_cast<std::size_t>(got));
        if (got < room)
        {
            // The song ended: pause briefly, then the next one.
            track_.reset();
            gap_frames_ = static_cast<int>(kGapSeconds * static_cast<float>(kSampleRate));
        }
    }
}

} // namespace hui::audio
