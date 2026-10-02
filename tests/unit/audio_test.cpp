// ps5-homebrew-ui - Mixer, WAV decoding and sound bank tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/cues.hpp"
#include "audio/mixer.hpp"
#include "audio/wav.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace hui::audio;

std::vector<std::int16_t> render(Mixer &mixer, int frames)
{
    std::vector<std::int16_t> out(static_cast<std::size_t>(frames) * 2);
    mixer.render(out.data(), frames);
    return out;
}

double peak(const std::vector<std::int16_t> &samples)
{
    double best = 0.0;
    for (std::int16_t s : samples)
        best = std::max(best, std::fabs(static_cast<double>(s)) / 32767.0);
    return best;
}

std::string wav_bytes(int channels, int bits, int rate, const std::vector<std::int32_t> &values)
{
    auto u32 = [](std::string &s, std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            s.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    };
    auto u16 = [](std::string &s, std::uint16_t v)
    {
        s.push_back(static_cast<char>(v & 0xff));
        s.push_back(static_cast<char>(v >> 8));
    };
    std::string data;
    for (std::int32_t value : values)
    {
        for (int i = 0; i < bits / 8; ++i)
            data.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
    }
    std::string out = "RIFF";
    u32(out, static_cast<std::uint32_t>(36 + data.size()));
    out += "WAVEfmt ";
    u32(out, 16);
    u16(out, 1);
    u16(out, static_cast<std::uint16_t>(channels));
    u32(out, static_cast<std::uint32_t>(rate));
    u32(out, static_cast<std::uint32_t>(rate * channels * bits / 8));
    u16(out, static_cast<std::uint16_t>(channels * bits / 8));
    u16(out, static_cast<std::uint16_t>(bits));
    out += "data";
    u32(out, static_cast<std::uint32_t>(data.size()));
    return out + data;
}

TEST(Mixer, SilentWithoutVoices)
{
    Mixer mixer;
    EXPECT_EQ(peak(render(mixer, 256)), 0.0);
    EXPECT_EQ(mixer.active_voices(), 0);
}

TEST(Mixer, ToneSoundsThenEnds)
{
    Mixer mixer;
    Tone tone;
    tone.seconds = 0.02f;
    ASSERT_TRUE(mixer.play_tone(tone, PlayParams{}));
    EXPECT_GT(peak(render(mixer, 960)), 0.3);
    render(mixer, 960);
    EXPECT_EQ(mixer.active_voices(), 0);
    EXPECT_EQ(peak(render(mixer, 256)), 0.0);
}

TEST(Mixer, LimiterNeverExceedsCeiling)
{
    EXPECT_FLOAT_EQ(Mixer::limit(0.5f), 0.5f);
    EXPECT_LE(Mixer::limit(10.0f), Mixer::kCeiling);
    EXPECT_GE(Mixer::limit(-10.0f), -Mixer::kCeiling);
    Mixer mixer;
    Tone tone;
    tone.seconds = 0.1f;
    for (int i = 0; i < 20; ++i)
        mixer.play_tone(tone, PlayParams{});
    EXPECT_LE(peak(render(mixer, 2400)), Mixer::kCeiling + 1e-3);
}

TEST(Mixer, PlaysClipSamplesAndMutesBus)
{
    std::vector<float> samples(200, 0.5f);
    Clip clip{samples.data(), 100};
    Mixer mixer;
    PlayParams params;
    params.bus = Bus::ui;
    mixer.play_clip(&clip, params);
    auto out = render(mixer, 50);
    EXPECT_NEAR(out[20] / 32767.0, 0.5, 0.01);
    mixer.set_bus_gain(Bus::ui, 0.0f);
    render(mixer, 4800); // let the gain ramp settle; the clip has ended anyway
    mixer.play_clip(&clip, params);
    out = render(mixer, 50);
    EXPECT_LT(peak(out), 0.01);
}

TEST(Mixer, StealsOldestVoiceWhenFullAndDropsWhenQueueFull)
{
    Mixer mixer;
    Tone tone;
    tone.seconds = 1.0f;
    for (std::size_t i = 0; i < Mixer::kVoices + 5; ++i)
        mixer.play_tone(tone, PlayParams{});
    render(mixer, 16);
    EXPECT_EQ(mixer.active_voices(), static_cast<int>(Mixer::kVoices));
    for (std::size_t i = 0; i < Mixer::kQueue + 10; ++i)
        mixer.stop_all();
    EXPECT_GT(mixer.dropped_commands(), 0u);
}

TEST(Wav, DecodesMono16AndStereo24)
{
    auto mono = decode_wav(wav_bytes(1, 16, 48000, {16384, -16384}));
    ASSERT_TRUE(mono.ok()) << mono.error;
    ASSERT_EQ(mono.frames, 2u);
    EXPECT_NEAR(mono.samples[0], 0.5f, 1e-4);
    EXPECT_NEAR(mono.samples[1], 0.5f, 1e-4);
    EXPECT_NEAR(mono.samples[3], -0.5f, 1e-4);

    auto stereo = decode_wav(wav_bytes(2, 24, 48000, {4194304, -4194304}));
    ASSERT_TRUE(stereo.ok()) << stereo.error;
    ASSERT_EQ(stereo.frames, 1u);
    EXPECT_NEAR(stereo.samples[0], 0.5f, 1e-4);
    EXPECT_NEAR(stereo.samples[1], -0.5f, 1e-4);
}

TEST(Wav, RejectsWrongRateFormatAndGarbage)
{
    EXPECT_FALSE(decode_wav(wav_bytes(2, 16, 44100, {0, 0})).ok());
    EXPECT_FALSE(decode_wav(wav_bytes(3, 16, 48000, {0, 0, 0})).ok());
    EXPECT_FALSE(decode_wav(wav_bytes(1, 8, 48000, {0})).ok());
    EXPECT_FALSE(decode_wav("RIFF").ok());
    std::string truncated = wav_bytes(1, 16, 48000, {1, 2, 3});
    truncated.resize(truncated.size() - 4);
    EXPECT_FALSE(decode_wav(truncated).ok());
}

TEST(Cues, NamesAreUniqueAndRoundTrip)
{
    std::set<std::string> names;
    for (std::size_t i = 0; i < kCueCount; ++i)
    {
        const Cue cue = static_cast<Cue>(i);
        const std::string name = cue_name(cue);
        EXPECT_TRUE(names.insert(name).second) << name;
        Cue parsed = Cue::count;
        ASSERT_TRUE(cue_from_name(name, &parsed));
        EXPECT_EQ(parsed, cue);
        EXPECT_GT(placeholder_tone(cue).seconds, 0.0f);
    }
    EXPECT_EQ(cue_bus(Cue::focus), Bus::ui);
    EXPECT_EQ(cue_bus(Cue::complete), Bus::sfx);
    EXPECT_STREQ(sound_set_name(SoundSet::paper), "paper");
    EXPECT_STREQ(sound_set_name(SoundSet::glass), "glass");
}

TEST(SoundBank, PrefersItsSetThenAnotherSetThenPlaceholder)
{
    SoundBank bank;
    bank.add(SoundSet::paper, Cue::place, std::vector<float>(20, 0.1f), 10);
    bank.add(SoundSet::glass, Cue::saved, std::vector<float>(20, 0.2f), 10);
    EXPECT_TRUE(bank.has_recording(Cue::place, SoundSet::paper));
    EXPECT_FALSE(bank.has_recording(Cue::place, SoundSet::glass));
    EXPECT_TRUE(bank.has_recording(Cue::saved, SoundSet::glass));
    EXPECT_FALSE(bank.has_recording(Cue::complete, SoundSet::paper));

    Mixer mixer;
    bank.play(mixer, SoundSet::glass, CueEvent{Cue::complete}); // placeholder tone
    bank.play(mixer, SoundSet::glass, CueEvent{Cue::place});    // borrowed from paper
    bank.play(mixer, SoundSet::glass, CueEvent{Cue::saved});    // its own recording
    render(mixer, 4);
    EXPECT_EQ(mixer.active_voices(), 3);
}

} // namespace
