// ps5-homebrew-ui - Music streaming tests: Vorbis decode, loops, decks and mixing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/mixer.hpp"
#include "audio/music.hpp"
#include "audio/stream_ring.hpp"
#include "core/save_file.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#ifndef HUI_SOURCE_DIR
#define HUI_SOURCE_DIR "."
#endif

using namespace hui::audio;

namespace
{

const std::string kMusic = std::string(HUI_SOURCE_DIR) + "/tests/fixtures/music";

std::string fixture(const char *name)
{
    std::string data;
    EXPECT_TRUE(hui::save::read_file(kMusic + "/" + name, &data)) << name;
    return data;
}

double peak(const std::vector<std::int16_t> &samples)
{
    double best = 0.0;
    for (std::int16_t s : samples)
        best = std::max(best, std::fabs(static_cast<double>(s) / 32767.0));
    return best;
}

} // namespace

TEST(StreamRing, WrapsAndReportsShortReads)
{
    StreamRing ring(8);
    float in[20];
    for (int i = 0; i < 20; ++i)
        in[i] = static_cast<float>(i);
    EXPECT_EQ(ring.write(in, 10), 8u); // only 8 frames fit
    float out[20] = {};
    EXPECT_EQ(ring.read(out, 5), 5u);
    EXPECT_EQ(out[9], 9.0f);
    EXPECT_EQ(ring.write(in, 4), 4u); // wraps around the end
    EXPECT_EQ(ring.available(), 7u);
    EXPECT_EQ(ring.read(out, 20), 7u);
    EXPECT_EQ(out[5], 15.0f); // frames 5..7 of the first write
    EXPECT_EQ(out[6], 0.0f);  // then frames 0..3 of the second
    EXPECT_EQ(out[13], 7.0f);
}

TEST(MusicTrack, ReadsLoopCommentsAndLoopsForever)
{
    MusicTrack track;
    ASSERT_EQ(track.open(fixture("puzzle_calm_02.ogg")), "");
    EXPECT_EQ(track.loop_start(), 12000u);
    EXPECT_EQ(track.loop_end(), 36000u);
    // Five seconds from a one-second file: the loop keeps it going.
    std::vector<float> out(48000 * 2);
    for (int second = 0; second < 5; ++second)
        EXPECT_EQ(track.decode(out.data(), 48000), 48000);
    float loudest = 0.0f;
    for (float s : out)
        loudest = std::max(loudest, std::fabs(s));
    EXPECT_GT(loudest, 0.08f); // ffmpeg's test sine peaks at 1/8
}

TEST(MusicTrack, DuplicatesMonoAndRejectsOtherRates)
{
    MusicTrack mono;
    ASSERT_EQ(mono.open(fixture("menu_main.ogg")), "");
    std::vector<float> out(4800 * 2);
    ASSERT_EQ(mono.decode(out.data(), 4800), 4800);
    EXPECT_FLOAT_EQ(out[2000], out[2001]);
    MusicTrack wrong;
    EXPECT_NE(wrong.open(fixture("wrong_rate.ogg")).find("48000"), std::string::npos);
    MusicTrack junk;
    EXPECT_NE(junk.open("not an ogg file"), "");
}

TEST(Mixer, StreamsPlayOnTheMusicBusAndRampGain)
{
    Mixer mixer;
    StreamRing ring(4096);
    mixer.attach_stream(0, &ring);
    std::vector<float> constant(2048 * 2, 0.5f);
    ring.write(constant.data(), 2048);
    std::vector<std::int16_t> out(256 * 2);
    mixer.render(out.data(), 256);
    EXPECT_EQ(peak(out), 0.0); // gain starts at zero
    mixer.set_stream_gain(0, 1.0f, 0.0f);
    mixer.render(out.data(), 256);
    EXPECT_NEAR(peak(out), 0.5, 0.02);
    mixer.set_bus_gain(Bus::music, 0.0f);
    for (int i = 0; i < 12; ++i) // past the 2048 buffered frames
        mixer.render(out.data(), 256);
    EXPECT_LT(peak(out), 0.05);              // the music bus fader applies to streams
    EXPECT_GT(mixer.stream_underruns(), 0u); // the ring ran dry along the way
}

TEST(MusicPlayer, PlaysEverySongInTurnThenStartsOver)
{
    Mixer mixer;
    MusicPlayer player;
    // Four files; wrong_rate.ogg is listed but skipped when its turn comes.
    ASSERT_EQ(player.init(mixer, kMusic, 7), 4);
    std::vector<std::string> playable;
    for (const std::string &song : player.playlist())
        if (song != "wrong_rate")
            playable.push_back(song);
    ASSERT_EQ(playable.size(), 3u);

    // About 20 s of playback, pumped once per 60 Hz frame like the app.
    std::vector<std::string> heard;
    std::vector<std::int16_t> out(800 * 2);
    double loudest = 0.0;
    for (int frame = 0; frame < 60 * 20; ++frame)
    {
        player.pump(1.0f / 60.0f);
        mixer.render(out.data(), 800);
        loudest = std::max(loudest, peak(out));
        if (!player.current().empty() && (heard.empty() || heard.back() != player.current()))
            heard.push_back(player.current());
    }
    EXPECT_GT(loudest, 0.02); // Audible at the 40% background level.
    // Songs follow the shuffled order and the list starts over after the last.
    ASSERT_GE(heard.size(), 5u);
    for (std::size_t i = 0; i < heard.size(); ++i)
        EXPECT_EQ(heard[i], playable[i % playable.size()]) << "song " << i;
    player.duck();
    player.pump(0.5f);
}

TEST(MusicPlayer, ShuffleChangesWithTheSeed)
{
    Mixer mixer;
    MusicPlayer a;
    MusicPlayer b;
    a.init(mixer, kMusic, 1);
    b.init(mixer, kMusic, 1);
    EXPECT_EQ(a.playlist(), b.playlist()); // same launch seed, same order
    bool differs = false;
    for (std::uint64_t seed = 2; seed < 40 && !differs; ++seed)
    {
        MusicPlayer c;
        c.init(mixer, kMusic, seed);
        differs = c.playlist() != a.playlist();
    }
    EXPECT_TRUE(differs); // another launch gets another order
}

TEST(MusicPlayer, SilentWithoutSongs)
{
    Mixer mixer;
    MusicPlayer player;
    EXPECT_EQ(player.init(mixer, "/nonexistent", 3), 0);
    player.pump(0.016f);
    EXPECT_EQ(player.current(), "");
}
