// ps5-homebrew-ui - Invented sample content shared by every design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "demo/catalog.hpp"

namespace hui::demo
{

namespace
{

using gfx::Color;

struct Seed
{
    const char *title;
    const char *studio;
    const char *genre;
    const char *blurb;
    int year;
    float rating;
    float progress;
    int hours;
    int players;
    std::uint32_t dark;
    std::uint32_t mid;
    std::uint32_t accent;
};

constexpr Seed kSeeds[] = {
    {"Lumen Drift", "Northlight Works", "Racing",
     "Thread a light-cycle through a city that rebuilds itself every lap.", 2025, 4.6f, 0.62f, 18,
     2, 0x120a2e, 0x4b2a9c, 0xff5fa2},
    {"Tidewater", "Tin Sparrow", "Adventure",
     "Chart a drowned coastline one tide at a time, and decide what stays under.", 2024, 4.8f,
     0.35f, 41, 1, 0x06222e, 0x12708a, 0x7ff0d8},
    {"Glass Orchard", "Halcyon Forge", "Puzzle",
     "Grow impossible trees by bending light through a greenhouse of prisms.", 2026, 4.3f, 0.9f, 12,
     1, 0x1c2414, 0x5f8f3a, 0xe9f28b},
    {"Neon Harbor", "Studio Marrow", "Action",
     "A dock worker, a stolen ferry and one very long night shift.", 2023, 4.1f, 0.15f, 6, 2,
     0x10162f, 0x26428f, 0x38e8ff},
    {"Paper Kites", "Open Kettle", "Platformer",
     "Fold, glide and unfold across a world made of letters nobody sent.", 2025, 4.7f, 1.0f, 23, 4,
     0x2d1a14, 0xc4603a, 0xffd28a},
    {"Hollow Signal", "Ninefold", "Mystery",
     "A lighthouse keeps answering a radio that was switched off years ago.", 2024, 4.4f, 0.48f, 9,
     1, 0x0d1418, 0x35505c, 0xf2b84b},
    {"Starlane Courier", "Northlight Works", "Simulation",
     "Deliver small parcels across large distances. Mind the solar weather.", 2026, 4.2f, 0.07f, 3,
     1, 0x070b1f, 0x2b2f7a, 0x9db4ff},
    {"Ember and Ash", "Halcyon Forge", "Role-playing",
     "Two rival smiths share one forge, one city and one prophecy.", 2022, 4.9f, 0.77f, 96, 1,
     0x230b08, 0xa3341c, 0xffb347},
    {"Quiet Machines", "Ninefold", "Strategy",
     "Program a valley of patient robots to farm, mend and remember.", 2025, 4.5f, 0.52f, 37, 1,
     0x141a1c, 0x4d6b6f, 0xb6f0c4},
    {"Saltwind", "Tin Sparrow", "Sailing", "No map, no engine: read the water and the birds.", 2023,
     4.0f, 0.28f, 14, 2, 0x0a1d33, 0x2a6fb0, 0xf4f1e6},
    {"Polar Bloom", "Studio Marrow", "Survival",
     "Keep one flower alive through a winter that lasts the whole game.", 2026, 4.6f, 0.0f, 0, 1,
     0x0f1b2b, 0x5d89b8, 0xff8fb3},
    {"Midnight Relay", "Open Kettle", "Rhythm",
     "Pass the beat between four runners without ever dropping it.", 2024, 4.3f, 0.66f, 21, 4,
     0x1a0b2b, 0x7a2bbf, 0x4dffcf},
    {"Cinder Peak", "Halcyon Forge", "Climbing",
     "A mountain that is also a volcano that is also, somehow, a clock.", 2025, 4.4f, 0.41f, 16, 1,
     0x1e1210, 0x7a4334, 0xff7043},
    {"Velvet Circuit", "Northlight Works", "Racing",
     "Midnight time trials on roads that hum when you get the line right.", 2022, 3.9f, 0.83f, 29,
     2, 0x180818, 0x8c1f5b, 0xffc857},
    {"Moth and Lantern", "Tin Sparrow", "Adventure", "You are the moth. The lantern has opinions.",
     2026, 4.7f, 0.22f, 5, 2, 0x16130b, 0x6b5a1e, 0xffe9a3},
    {"Orbit Garden", "Ninefold", "Builder",
     "Terrace a tiny moon, then set it spinning just fast enough for rain.", 2024, 4.5f, 0.58f, 44,
     1, 0x08141a, 0x1f6b5c, 0xc8ff7a},
    {"Low Tide Radio", "Open Kettle", "Narrative",
     "A late-night call-in show for a town that only exists at low tide.", 2023, 4.8f, 1.0f, 8, 1,
     0x101727, 0x3a4f8a, 0xff9f6e},
    {"Brasswork", "Halcyon Forge", "Puzzle",
     "Every door is a mechanism and every mechanism is a little bit alive.", 2025, 4.2f, 0.31f, 11,
     1, 0x1c150a, 0x9a742a, 0xfde7a8},
    {"Farlight", "Studio Marrow", "Role-playing",
     "Carry the last lit candle across a continent that forgot fire.", 2026, 4.6f, 0.12f, 7, 1,
     0x0e0f24, 0x42358f, 0xffd166},
    {"Kiln", "Tin Sparrow", "Crafting",
     "Throw, glaze and fire pottery for customers with very specific dreams.", 2024, 4.1f, 0.7f, 33,
     1, 0x241511, 0xb5553c, 0xf7e3d0},
    {"Thousand Steps", "Ninefold", "Roguelike",
     "One staircase, rebuilt every climb. Bring comfortable shoes.", 2023, 4.4f, 0.45f, 52, 1,
     0x12121a, 0x55557a, 0xe35d6a},
    {"Blue Hour", "Northlight Works", "Photography",
     "You have twenty minutes of perfect light. Spend them well.", 2025, 4.7f, 0.93f, 15, 1,
     0x0b1430, 0x2f55b5, 0xffb3c7},
    {"Signal Fires", "Studio Marrow", "Tactics",
     "Hold the ridge line by lighting the right beacon at the right moment.", 2022, 4.0f, 0.37f, 26,
     2, 0x1a100a, 0x8a4a1f, 0xffe08a},
    {"Understory", "Open Kettle", "Exploration",
     "Shrink to the size of a seed and learn the forest floor by name.", 2026, 4.5f, 0.05f, 2, 1,
     0x0b1a10, 0x2f7a45, 0xd6ff9a},
};

} // namespace

Catalog::Catalog()
{
    for (const Seed &seed : kSeeds)
    {
        Item item{};
        item.title = seed.title;
        item.studio = seed.studio;
        item.genre = seed.genre;
        item.blurb = seed.blurb;
        item.year = seed.year;
        item.rating = seed.rating;
        item.progress = seed.progress;
        item.hours = seed.hours;
        item.players = seed.players;
        item.dark = Color::rgb(seed.dark);
        item.mid = Color::rgb(seed.mid);
        item.accent = Color::rgb(seed.accent);
        items_.push_back(item);
    }
}

} // namespace hui::demo
