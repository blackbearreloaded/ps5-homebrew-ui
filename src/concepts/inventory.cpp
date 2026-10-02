// ps5-homebrew-ui - Design "Satchel": an inventory you handle, not a list you browse.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A role-playing-game bag and equipment screen built around one gesture: lift
// an item, carry it, set it down. What makes it feel finished:
//
//   - items are objects with a position of their own: each one springs to
//     wherever the bag says it lives, so a swap, a sort, a filter and an equip
//     are all the same motion and nothing ever teleports;
//   - a lifted item grows, casts a shadow, tilts with its speed and trails the
//     focus ring a little, while the places that would take it pulse amber;
//   - the tooltip answers "is this better than what I am wearing?" with
//     arrows and differences against the worn item of the same slot, and
//     names what Cross would do right now, refusals included;
//   - the totals count to their new values, the figure flashes and takes the
//     colours of what it wears, and the carry bar turns warm near the limit;
//   - every rule has its own paper sound (pickup, drop, rotate for a swap,
//     merge pitched by the stack, connect pitched by the loadout) and every
//     refusal says why in the line under the bag.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kInk = Color::rgb(0xf3e6cc);      // parchment: all text
const Color kLeather = Color::rgb(0x2a1c12);  // panels
const Color kHide = Color::rgb(0x45301f);     // the figure, raised details
const Color kWell = Color::rgb(0x120b07);     // the inside of a slot
const Color kWellEdge = Color::rgb(0x7a6046); // its fine border
const Color kAmber = Color::rgb(0xe9a646);    // the one accent
const Color kGood = Color::rgb(0x7fd66b);
const Color kBad = Color::rgb(0xe8604c);
const Color kWarm = Color::rgb(0xf0843a); // the carry bar near its limit
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kBlack = Color::rgb(0x000000);

constexpr float kMargin = 96.0f;
constexpr float kTop = 184.0f; // the three columns share a top and a bottom
constexpr float kBottom = 900.0f;
constexpr float kCharX = 96.0f; // character panel
constexpr float kCharW = 480.0f;
constexpr float kBagX = 608.0f; // bag column
constexpr float kBagW = 792.0f;
constexpr float kTipX = 1432.0f; // tooltip card
constexpr float kTipW = 392.0f;
constexpr float kSlot = 88.0f;
constexpr float kGap = 8.0f;
constexpr float kGridX = kBagX + 16.0f;
constexpr float kGridY = 268.0f;
constexpr float kRadius = 14.0f;      // slots
constexpr float kPanelRadius = 26.0f; // panels and cards
constexpr float kPlate = 80.0f;       // an item's own tile, inside a slot
constexpr float kLift = 16.0f;        // how far a carried item hovers above its slot
constexpr float kWearPitch = 116.0f;  // equipment slots, top to top: room for a label
constexpr float kLiftScale = 0.15f;

constexpr int kColumns = 8;
constexpr int kRows = 5;
constexpr int kCells = kColumns * kRows;
constexpr int kWearSlots = 7;
constexpr int kPool = 64; // item objects: 40 in the bag, 7 worn, 1 carried, a few fading out
constexpr int kTabs = 6;
constexpr int kSorts = 3;
constexpr float kCarryLimit = 90.0f;
constexpr float kPi = 3.14159265f;

constexpr const char *kTechniques[] = {
    "Items own a spring position: swap, sort, filter and equip are one travel animation",
    "Pick up and place: lift, shadow, speed tilt, a trailing follow and pulsing valid targets",
    "Comparing tooltip on frosted glass: arrows and differences against the worn item",
    "Icons composed from kit shapes and tinted per item; rarity as border and inner glow",
    "Paper sounds with meaning: merge pitched by the stack, connect by the loadout's power",
    "Totals that count, a carry bar that warms, refusals explained in a status line",
};

constexpr app::TourStep kTour[] = {
    // Lift the tonics and carry them toward their twin stack.
    {0.6f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, 0, Direction::right},
    {0.25f, 0, Direction::down},
    {0.7f, 0, Direction::left, "carry"},
    {0.4f, action_bit(Action::confirm)},
    // Carry the greatsword to the main hand.
    {0.5f, 0, Direction::up},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {0.2f, 0, Direction::left},
    {0.2f, 0, Direction::left},
    {0.2f, 0, Direction::left},
    {0.2f, 0, Direction::left},
    {0.2f, 0, Direction::left},
    {0.8f, action_bit(Action::confirm), Direction::none, "equip"},
    // Back to the bag, filter it, sort it.
    {0.7f, 0, Direction::right},
    {0.4f, action_bit(Action::jump_next)},
    {1.0f, action_bit(Action::menu), Direction::none, "weapons"},
    {0.4f, 0, Direction::down},
    {0.7f, action_bit(Action::confirm), Direction::none, "sort"},
    {0.3f, action_bit(Action::jump_prev)},
    {1.2f, 0, Direction::none, "sorted"},
};

// ---- the items -------------------------------------------------------------

enum class Rarity : std::uint8_t
{
    common,
    uncommon,
    rare,
    epic,
    legendary,
};

constexpr const char *kRarityNames[] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};

Color rarity_color(Rarity rarity)
{
    switch (rarity)
    {
    case Rarity::common:
        return Color::rgb(0xa9a196);
    case Rarity::uncommon:
        return Color::rgb(0x6cc467);
    case Rarity::rare:
        return Color::rgb(0x58a4f2);
    case Rarity::epic:
        return Color::rgb(0xb47cf4);
    case Rarity::legendary:
        return Color::rgb(0xf5a938);
    }
    return kInk;
}

// The tabs above the bag are "All" followed by these, in this order.
enum class Category : std::uint8_t
{
    weapon,
    armour,
    consumable,
    material,
    quest,
};

constexpr const char *kTabNames[kTabs] = {"All",         "Weapons",   "Armour",
                                          "Consumables", "Materials", "Quest"};
constexpr const char *kCategoryNames[] = {"Main hand", "Worn", "Consumable", "Material",
                                          "Quest item"};

// Where an item is worn. The first seven are also the equipment slots, in the
// order the character panel lays them out: a column of four, a column of three.
enum class Wear : std::uint8_t
{
    head,
    chest,
    hands,
    legs,
    main_hand,
    off_hand,
    trinket,
    none,
};

constexpr const char *kWearNames[kWearSlots] = {"Head",      "Chest",    "Hands",  "Legs",
                                                "Main hand", "Off hand", "Trinket"};
constexpr const char *kWearRefusals[kWearSlots] = {
    "is not worn on the head", "is not worn on the chest",  "is not worn on the hands",
    "is not worn on the legs", "is not a main-hand weapon", "does not go in the off hand",
    "is not a trinket",
};

enum class StatId : std::uint8_t
{
    none,
    attack,
    defence,
    speed,
    crit,
    focus,
    warding,
    health,
    lasts,
    reading,
    worth,
    purity,
    hardness,
};

struct Stat
{
    StatId id = StatId::none;
    int value = 0;
};

const char *stat_name(StatId id)
{
    switch (id)
    {
    case StatId::attack:
        return "Attack";
    case StatId::defence:
        return "Defence";
    case StatId::speed:
        return "Speed";
    case StatId::crit:
        return "Critical";
    case StatId::focus:
        return "Focus";
    case StatId::warding:
        return "Warding";
    case StatId::health:
        return "Health";
    case StatId::lasts:
        return "Lasts";
    case StatId::reading:
        return "Reading";
    case StatId::worth:
        return "Worth";
    case StatId::purity:
        return "Purity";
    case StatId::hardness:
        return "Hardness";
    case StatId::none:
        break;
    }
    return "";
}

void format_stat(char *out, std::size_t size, Stat stat, bool consumable)
{
    switch (stat.id)
    {
    case StatId::lasts:
    case StatId::reading:
        std::snprintf(out, size, "%d s", stat.value);
        break;
    case StatId::purity:
        std::snprintf(out, size, "%d%%", stat.value);
        break;
    case StatId::worth:
    case StatId::hardness:
        std::snprintf(out, size, "%d", stat.value);
        break;
    default:
        // What a consumable gives is a gain; what gear has is a value.
        std::snprintf(out, size, consumable || stat.value < 0 ? "%+d" : "%d", stat.value);
        break;
    }
}

enum class Icon : std::uint8_t
{
    sword,
    greatsword,
    dagger,
    bow,
    axe,
    staff,
    shield,
    buckler,
    helm,
    hood,
    crown,
    chest,
    scale_chest,
    gloves,
    legs,
    ring,
    amulet,
    potion,
    flask,
    bread,
    scroll,
    gem,
    ore,
    ingot,
    scale,
    hide,
    feather,
    letter,
    key,
    map,
};

struct ItemDef
{
    const char *name;
    const char *kind;
    Category category;
    Wear wear;
    Rarity rarity;
    Icon icon;
    std::uint32_t tint;   // the icon's body colour
    std::uint32_t accent; // its second colour: a guard, a gem, a trim
    Stat stats[3];
    float weight;  // of one
    int max_stack; // 1: does not stack
    const char *flavour;
};

constexpr std::uint32_t kSteel = 0xc8d0d8;
constexpr std::uint32_t kWood = 0x9b6b3f;
constexpr std::uint32_t kGold = 0xe7b450;

// Indices into kItems, for the starting layout.
enum Id : int
{
    wayfarers_blade,
    tidewater_sabre,
    emberfall_greatsword,
    hollowpine_bow,
    thistle_dagger,
    rustbitten_hatchet,
    stormcaller_staff,
    oakheart_buckler,
    bulwark,
    travellers_hood,
    helm_of_the_pale_watch,
    antlered_crown,
    quilted_gambeson,
    scalemail,
    brigands_jerkin,
    smiths_mitts,
    gauntlets,
    marsh_waders,
    greaves,
    ring_of_promises,
    moonmoth_amulet,
    redcap_tonic,
    skyglass_elixir,
    waybread,
    firebloom_flask,
    homeward_scroll,
    iron_ore,
    moonsilver_ingot,
    wyrm_scale,
    tanned_hide,
    ember_shard,
    raven_feather,
    sealed_letter,
    brass_key,
    half_map,
};

constexpr ItemDef kItems[] = {
    {"Wayfarer's Blade",
     "Sword",
     Category::weapon,
     Wear::main_hand,
     Rarity::uncommon,
     Icon::sword,
     kSteel,
     0xb98d4a,
     {{StatId::attack, 15}, {StatId::speed, 3}},
     2.6f,
     1,
     "Notched, honest and never once left behind."},
    {"Tidewater Sabre",
     "Sabre",
     Category::weapon,
     Wear::main_hand,
     Rarity::rare,
     Icon::sword,
     0xb4dcf2,
     0x3f7fc4,
     {{StatId::attack, 22}, {StatId::speed, 6}},
     3.0f,
     1,
     "Curved like the bay it was quenched in."},
    {"Emberfall Greatsword",
     "Greatsword",
     Category::weapon,
     Wear::main_hand,
     Rarity::legendary,
     Icon::greatsword,
     0xf6cf8e,
     0xd9532b,
     {{StatId::attack, 34}, {StatId::crit, 12}, {StatId::speed, -4}},
     6.5f,
     1,
     "Still warm from a forge that burned down a century ago."},
    {"Hollowpine Bow",
     "Bow",
     Category::weapon,
     Wear::main_hand,
     Rarity::uncommon,
     Icon::bow,
     0xb08a52,
     0xe4ecd2,
     {{StatId::attack, 17}, {StatId::crit, 8}},
     1.8f,
     1,
     "Hums a low note when the wind is right."},
    {"Thistle Dagger",
     "Dagger",
     Category::weapon,
     Wear::main_hand,
     Rarity::common,
     Icon::dagger,
     kSteel,
     0x8a6a8f,
     {{StatId::attack, 9}, {StatId::speed, 9}},
     0.8f,
     1,
     "Small, sharp and easily mislaid."},
    {"Rustbitten Hatchet",
     "Hatchet",
     Category::weapon,
     Wear::main_hand,
     Rarity::common,
     Icon::axe,
     0xb98a70,
     kWood,
     {{StatId::attack, 11}},
     2.5f,
     1,
     "It has opened more crates than doors."},
    {"Stormcaller Staff",
     "Staff",
     Category::weapon,
     Wear::main_hand,
     Rarity::epic,
     Icon::staff,
     0xb99cf5,
     0x8fe3ff,
     {{StatId::attack, 26}, {StatId::focus, 18}},
     3.2f,
     1,
     "Best left outside during thunder."},
    {"Oakheart Buckler",
     "Buckler",
     Category::armour,
     Wear::off_hand,
     Rarity::uncommon,
     Icon::buckler,
     kWood,
     0xcdb67e,
     {{StatId::defence, 12}},
     3.0f,
     1,
     "Grown, not built. It still buds in spring."},
    {"Bulwark of the Ninth Gate",
     "Tower shield",
     Category::armour,
     Wear::off_hand,
     Rarity::epic,
     Icon::shield,
     0x7d6bb8,
     0xd9c7ff,
     {{StatId::defence, 28}, {StatId::warding, 10}, {StatId::speed, -5}},
     7.0f,
     1,
     "The gate fell. The shield did not."},
    {"Traveller's Hood",
     "Hood",
     Category::armour,
     Wear::head,
     Rarity::common,
     Icon::hood,
     0x8a765f,
     0x4b3d30,
     {{StatId::defence, 3}},
     0.4f,
     1,
     "Keeps off the rain and most questions."},
    {"Helm of the Pale Watch",
     "Helm",
     Category::armour,
     Wear::head,
     Rarity::rare,
     Icon::helm,
     0xc4d2e2,
     0x5a93d8,
     {{StatId::defence, 14}, {StatId::focus, 4}},
     2.8f,
     1,
     "Its last wearer never slept on duty."},
    {"Antlered Crown",
     "Circlet",
     Category::armour,
     Wear::head,
     Rarity::epic,
     Icon::crown,
     0xd9b25e,
     0xb47cf4,
     {{StatId::defence, 9}, {StatId::focus, 16}},
     1.2f,
     1,
     "The forest lends it. The forest will want it back."},
    {"Quilted Gambeson",
     "Padded armour",
     Category::armour,
     Wear::chest,
     Rarity::common,
     Icon::chest,
     0x9a8468,
     0x6e5a44,
     {{StatId::defence, 8}},
     2.2f,
     1,
     "Forty layers of linen and one of stubbornness."},
    {"Scalemail of the Drowned King",
     "Scale armour",
     Category::armour,
     Wear::chest,
     Rarity::legendary,
     Icon::scale_chest,
     0x4fb0a6,
     0xf2c15a,
     {{StatId::defence, 36}, {StatId::warding, 14}, {StatId::speed, -3}},
     9.0f,
     1,
     "Every scale remembers the weight of the sea."},
    {"Brigand's Jerkin",
     "Leather armour",
     Category::armour,
     Wear::chest,
     Rarity::uncommon,
     Icon::chest,
     0x8a5a36,
     0x6fae62,
     {{StatId::defence, 13}, {StatId::speed, 2}},
     3.4f,
     1,
     "More pockets than its maker admitted to."},
    {"Smith's Mitts",
     "Gloves",
     Category::armour,
     Wear::hands,
     Rarity::common,
     Icon::gloves,
     0x8a6a4c,
     0x5b4534,
     {{StatId::defence, 4}},
     0.6f,
     1,
     "Scorched outside, soft inside."},
    {"Gauntlets of the Quiet Hand",
     "Gauntlets",
     Category::armour,
     Wear::hands,
     Rarity::rare,
     Icon::gloves,
     0xb9c6d6,
     0x4f86cf,
     {{StatId::defence, 9}, {StatId::crit, 6}},
     1.6f,
     1,
     "They make no sound, even when clapping."},
    {"Marsh Waders",
     "Boots",
     Category::armour,
     Wear::legs,
     Rarity::common,
     Icon::legs,
     0x74805a,
     0x4c5238,
     {{StatId::defence, 5}},
     1.4f,
     1,
     "Dry on the inside, most days."},
    {"Greaves of the Long Road",
     "Greaves",
     Category::armour,
     Wear::legs,
     Rarity::uncommon,
     Icon::legs,
     0xb8b0a0,
     0x6fae62,
     {{StatId::defence, 11}, {StatId::speed, 4}},
     3.0f,
     1,
     "Worn smooth by a thousand leagues."},
    {"Ring of Three Promises",
     "Ring",
     Category::armour,
     Wear::trinket,
     Rarity::epic,
     Icon::ring,
     kGold,
     0xb47cf4,
     {{StatId::focus, 12}, {StatId::crit, 8}},
     0.1f,
     1,
     "Two were kept."},
    {"Moonmoth Amulet",
     "Amulet",
     Category::armour,
     Wear::trinket,
     Rarity::rare,
     Icon::amulet,
     0xcfd8e6,
     0x6fb1f5,
     {{StatId::focus, 9}, {StatId::defence, 3}},
     0.2f,
     1,
     "It flutters, faintly, near an open flame."},
    {"Redcap Tonic",
     "Tonic",
     Category::consumable,
     Wear::none,
     Rarity::common,
     Icon::potion,
     0xd9483b,
     kWood,
     {{StatId::health, 40}},
     0.3f,
     20,
     "Tastes of cherries and second chances."},
    {"Skyglass Elixir",
     "Elixir",
     Category::consumable,
     Wear::none,
     Rarity::rare,
     Icon::potion,
     0x5fb6f2,
     kWood,
     {{StatId::focus, 20}, {StatId::lasts, 90}},
     0.3f,
     20,
     "Bottled on a mountain, at noon, in silence."},
    {"Waybread",
     "Ration",
     Category::consumable,
     Wear::none,
     Rarity::common,
     Icon::bread,
     0xd2a25e,
     0x9a6a34,
     {{StatId::health, 15}},
     0.2f,
     20,
     "Keeps for a month and tastes like it."},
    {"Firebloom Flask",
     "Flask",
     Category::consumable,
     Wear::none,
     Rarity::uncommon,
     Icon::flask,
     0xf08a3c,
     kWood,
     {{StatId::attack, 10}, {StatId::lasts, 60}},
     0.4f,
     20,
     "Shake well. Then stand well back."},
    {"Scroll of the Homeward Wind",
     "Scroll",
     Category::consumable,
     Wear::none,
     Rarity::uncommon,
     Icon::scroll,
     0x7fae6a,
     0xe9dcc0,
     {{StatId::reading, 3}},
     0.1f,
     20,
     "Read it aloud and wake beside the last hearth you lit."},
    {"Iron Ore",
     "Ore",
     Category::material,
     Wear::none,
     Rarity::common,
     Icon::ore,
     0x8d8176,
     0xc97b4a,
     {{StatId::worth, 4}, {StatId::purity, 62}},
     0.5f,
     20,
     "Heavy, dull and exactly what the smith asked for."},
    {"Moonsilver Ingot",
     "Ingot",
     Category::material,
     Wear::none,
     Rarity::rare,
     Icon::ingot,
     0xcfe0f2,
     0x6fb1f5,
     {{StatId::worth, 85}, {StatId::purity, 97}},
     0.8f,
     20,
     "Cold to the touch, even fresh from the crucible."},
    {"Wyrm Scale",
     "Scale",
     Category::material,
     Wear::none,
     Rarity::epic,
     Icon::scale,
     0x9a6be0,
     0xe7d2ff,
     {{StatId::worth, 240}, {StatId::hardness, 9}},
     0.4f,
     20,
     "Shed, the hunters say. Nobody asks them twice."},
    {"Tanned Hide",
     "Leather",
     Category::material,
     Wear::none,
     Rarity::common,
     Icon::hide,
     0xa87648,
     0x6e4a2a,
     {{StatId::worth, 6}, {StatId::hardness, 2}},
     0.6f,
     20,
     "Supple, and only slightly fragrant."},
    {"Ember Shard",
     "Gem",
     Category::material,
     Wear::none,
     Rarity::uncommon,
     Icon::gem,
     0xf07a3c,
     0xffd08a,
     {{StatId::worth, 30}, {StatId::purity, 80}},
     0.2f,
     20,
     "A splinter of something that refuses to cool."},
    {"Raven Feather",
     "Feather",
     Category::material,
     Wear::none,
     Rarity::common,
     Icon::feather,
     0x565c74,
     0xaab3d6,
     {{StatId::worth, 1}},
     0.05f,
     20,
     "Fletchers pay for these. Ravens do not sell them."},
    {"Sealed Letter",
     "Letter",
     Category::quest,
     Wear::none,
     Rarity::rare,
     Icon::letter,
     0xe9dcc0,
     0xc23b2e,
     {},
     0.1f,
     1,
     "For the hands of the lighthouse keeper, and no others."},
    {"Brass Key of the Undercroft",
     "Key",
     Category::quest,
     Wear::none,
     Rarity::uncommon,
     Icon::key,
     0xd9a84a,
     0x8a6a2c,
     {},
     0.2f,
     1,
     "Three teeth, and a fourth filed away on purpose."},
    {"Cartographer's Half-Map",
     "Map",
     Category::quest,
     Wear::none,
     Rarity::epic,
     Icon::map,
     0xe2cfa4,
     0xc23b2e,
     {},
     0.1f,
     1,
     "The useful half is, naturally, the other one."},
};

struct Start
{
    int cell;
    Id id;
    int count;
};

// A lived-in bag: mixed kinds, a few gaps, two stacks of the same tonic.
constexpr Start kStartBag[] = {
    {0, tidewater_sabre, 1},
    {1, redcap_tonic, 5},
    {2, helm_of_the_pale_watch, 1},
    {3, iron_ore, 14},
    {4, emberfall_greatsword, 1},
    {6, waybread, 8},
    {7, sealed_letter, 1},
    {8, hollowpine_bow, 1},
    {9, redcap_tonic, 3},
    {10, scalemail, 1},
    {11, moonsilver_ingot, 3},
    {12, ring_of_promises, 1},
    {13, firebloom_flask, 1},
    {15, brass_key, 1},
    {16, thistle_dagger, 1},
    {17, skyglass_elixir, 2},
    {18, brigands_jerkin, 1},
    {19, wyrm_scale, 2},
    {20, bulwark, 1},
    {22, raven_feather, 12},
    {23, half_map, 1},
    {24, rustbitten_hatchet, 1},
    {25, homeward_scroll, 2},
    {26, gauntlets, 1},
    {27, tanned_hide, 9},
    {28, moonmoth_amulet, 1},
    {29, antlered_crown, 1},
    {32, stormcaller_staff, 1},
    {33, greaves, 1},
    {34, ember_shard, 4},
};

// What the character wears when the screen opens; the trinket slot is free.
constexpr Id kStartWorn[] = {travellers_hood, quilted_gambeson, smiths_mitts,
                             marsh_waders,    wayfarers_blade,  oakheart_buckler};

constexpr const char *kSortNames[kSorts] = {"By kind", "By rarity", "By name"};

// ---- icons, composed from kit shapes --------------------------------------

Color darker(Color c, float amount)
{
    return gfx::mix(c, kBlack.with_alpha(c.a), amount);
}

Color lighter(Color c, float amount)
{
    return gfx::mix(c, Color::rgb(0xffffff, c.a), amount);
}

// Draws in a 64-unit square centred on (cx, cy). `flat` paints every part in
// one colour and leaves the highlights out: the ghost of an empty slot.
struct Pen
{
    gfx::DrawList &list;
    float cx;
    float cy;
    float u; // virtual pixels per unit
    bool flat;
    Color flat_color;

    Color pick(Color c) const
    {
        return flat ? flat_color : c;
    }
    void rect(float x, float y, float w, float h, float r, Color c) const
    {
        list.rounded_rect({cx + x * u, cy + y * u, w * u, h * u}, r * u, pick(c));
    }
    // Centre-based, turned clockwise by `angle`.
    void turned(float x, float y, float w, float h, float r, float angle, Color c) const
    {
        list.rotated_rect({cx + (x - w * 0.5f) * u, cy + (y - h * 0.5f) * u, w * u, h * u}, r * u,
                          angle, pick(c));
    }
    void disc(float x, float y, float r, Color c) const
    {
        list.circle(cx + x * u, cy + y * u, r * u, pick(c));
    }
    void hoop(float x, float y, float r, float t, Color c) const
    {
        list.ring(cx + x * u, cy + y * u, r * u, t * u, pick(c));
    }
    void tri(float x, float y, float w, float h, float angle, Color c) const
    {
        list.triangle({cx + (x - w * 0.5f) * u, cy + (y - h * 0.5f) * u, w * u, h * u}, pick(c),
                      0.0f, angle);
    }
    void stroke(float x1, float y1, float x2, float y2, float t, Color c) const
    {
        list.line(cx + x1 * u, cy + y1 * u, cx + x2 * u, cy + y2 * u, t * u, pick(c));
    }
    void arc(float x, float y, float r, float t, float start, float sweep, Color c,
             bool caps = false) const
    {
        list.arc(cx + x * u, cy + y * u, r * u, t * u, start, sweep, pick(c), caps);
    }
    void cut(float x, float y, float w, float h, float corner, Color c) const
    {
        list.chamfer_rect({cx + x * u, cy + y * u, w * u, h * u}, corner * u, pick(c));
    }
    // Highlights only exist on the real thing.
    void shine_disc(float x, float y, float r, float alpha) const
    {
        if (!flat)
            list.circle(cx + x * u, cy + y * u, r * u, Color::rgb(0xffffff, alpha));
    }
    void shine_rect(float x, float y, float w, float h, float r, float alpha) const
    {
        if (!flat)
            list.rounded_rect({cx + x * u, cy + y * u, w * u, h * u}, r * u,
                              Color::rgb(0xffffff, alpha));
    }
};

// A blade on the diagonal: a turned rectangle, a triangle for the point, a
// cross-guard turned the same way, a grip and a pommel.
void draw_blade(const Pen &pen, float width, float length, float guard, Color steel, Color trim)
{
    constexpr float kAngle = 0.7854f;
    constexpr float kDir = 0.7071f;
    // The hilt sits bottom-left; everything is placed along the diagonal.
    const float base = -11.0f; // where blade meets guard, along the diagonal from the centre
    const auto along = [&](float d) { return d * kDir; };
    const float mid = base + length * 0.5f;
    pen.turned(along(mid), -along(mid), width, length, 1.5f, kAngle, steel);
    const float tip = base + length + width * 0.5f;
    pen.tri(along(tip), -along(tip), width, width, kAngle, steel);
    if (!pen.flat)
        pen.turned(along(mid) - 0.9f, -along(mid) - 0.9f, width * 0.22f, length * 0.9f, 0.5f,
                   kAngle, lighter(steel, 0.55f));
    pen.turned(along(base - 6.0f), -along(base - 6.0f), 5.5f, 12.0f, 2.0f, kAngle,
               Color::rgb(0x5a3d28));
    pen.turned(along(base), -along(base), guard, 5.5f, 2.5f, kAngle, trim);
    pen.disc(along(base - 13.5f), -along(base - 13.5f), 4.2f, trim);
}

void draw_torso(const Pen &pen, Color body, Color trim)
{
    // Short sleeves: two turned rectangles behind the body.
    pen.turned(-20, -9, 13, 24, 5, 0.55f, trim);
    pen.turned(20, -9, 13, 24, 5, -0.55f, trim);
    pen.cut(-18, -19, 36, 43, 8, body);
    pen.tri(0, -14, 16, 10, kPi, darker(body, 0.55f));
    pen.rect(-18, 9, 36, 5, 1, trim);
}

void draw_icon(gfx::DrawList &list, const ItemDef &def, float cx, float cy, float size,
               bool flat = false, Color flat_color = {})
{
    const Pen pen{list, cx, cy, size / 64.0f, flat, flat_color};
    const Color tint = Color::rgb(def.tint);
    const Color accent = Color::rgb(def.accent);
    switch (def.icon)
    {
    case Icon::sword:
        draw_blade(pen, 8.0f, 38.0f, 22.0f, tint, accent);
        break;
    case Icon::greatsword:
        draw_blade(pen, 12.5f, 40.0f, 30.0f, tint, accent);
        break;
    case Icon::dagger:
        draw_blade(pen, 8.0f, 24.0f, 18.0f, tint, accent);
        break;
    case Icon::bow:
        // An arc for the stave, a line for the string, an arrow across both.
        pen.arc(-12, 0, 27, 5, 0.38f, 2.38f, tint, true);
        pen.stroke(-2.5f, -22.5f, -2.5f, 22.5f, 1.4f, accent);
        pen.stroke(-18, 0, 19, 0, 2.2f, Color::rgb(kWood));
        pen.tri(23, 0, 10, 9, 1.5708f, Color::rgb(kSteel));
        break;
    case Icon::axe:
        pen.turned(1, 3, 6, 50, 3, 0.5f, accent);
        // The head is a thick ring sector hung on the top of the haft.
        pen.arc(9, -13, 21, 14, -2.0f, 1.75f, tint);
        if (!flat)
            pen.arc(9, -13, 21, 3, -2.0f, 1.75f, lighter(tint, 0.5f));
        break;
    case Icon::staff:
        pen.turned(-3, 5, 5, 50, 2.5f, 0.35f, Color::rgb(kWood));
        if (!flat)
            list.glow({cx + 0.0f * pen.u, cy - 30.0f * pen.u, 16.0f * pen.u, 16.0f * pen.u},
                      8.0f * pen.u, 9.0f * pen.u, accent.with_alpha(0.45f));
        pen.hoop(8, -22, 11.5f, 2.2f, tint);
        pen.disc(8, -22, 6.5f, accent);
        pen.shine_disc(6, -24, 2.2f, 0.7f);
        break;
    case Icon::shield:
        pen.cut(-20, -25, 40, 50, 12, accent);
        pen.cut(-16.5f, -21.5f, 33, 43, 10, tint);
        pen.rect(-3, -21, 6, 42, 1, darker(tint, 0.35f));
        pen.rect(-16, -5, 32, 6, 1, darker(tint, 0.35f));
        pen.disc(0, -2, 6.5f, accent);
        pen.shine_disc(-1.5f, -3.5f, 2.2f, 0.6f);
        break;
    case Icon::buckler:
        pen.disc(0, 0, 25, accent);
        pen.disc(0, 0, 21.5f, tint);
        pen.hoop(0, 0, 14, 2.0f, darker(tint, 0.35f));
        pen.disc(0, 0, 7, accent);
        for (int i = 0; i < 4; ++i)
        {
            const float a = 0.7854f + 1.5708f * static_cast<float>(i);
            pen.disc(std::sin(a) * 17.5f, -std::cos(a) * 17.5f, 2.2f, accent);
        }
        pen.shine_disc(-2, -2, 2.4f, 0.55f);
        break;
    case Icon::helm:
        // A half disc for the dome: an arc as thick as its radius.
        pen.arc(0, 0, 24, 24, -1.5708f, 3.1416f, tint);
        pen.rect(-24, -2, 48, 23, 7, tint);
        // The visor: a dark T cut into the face.
        pen.rect(-15, 5, 30, 5.5f, 2, darker(tint, 0.72f));
        pen.rect(-2.8f, 5, 5.6f, 16, 2, darker(tint, 0.72f));
        pen.rect(-26, -5, 52, 6.5f, 3, accent);
        pen.shine_rect(-14, -19, 5, 11, 2.5f, 0.3f);
        break;
    case Icon::hood:
        pen.arc(0, -1, 24, 24, -1.5708f, 3.1416f, tint);
        pen.rect(-24, -2, 48, 24, 9, tint);
        // The opening for the face, and the cloth gathered under the chin.
        pen.disc(0, 3, 13, darker(accent, 0.45f));
        pen.rect(-13, 3, 26, 14, 5, darker(accent, 0.45f));
        pen.rect(-17, 15, 34, 8, 4, darker(tint, 0.22f));
        break;
    case Icon::crown:
        pen.tri(-15, -7, 13, 22, -0.25f, tint);
        pen.tri(0, -11, 14, 28, 0.0f, tint);
        pen.tri(15, -7, 13, 22, 0.25f, tint);
        pen.rect(-23, 4, 46, 13, 3, tint);
        pen.rect(-23, 13, 46, 4, 2, darker(tint, 0.3f));
        pen.disc(-13, 9.5f, 3, accent);
        pen.disc(0, 9.5f, 3.6f, accent);
        pen.disc(13, 9.5f, 3, accent);
        break;
    case Icon::chest:
        draw_torso(pen, tint, accent);
        pen.stroke(0, -6, 0, 8, 1.6f, darker(tint, 0.4f));
        break;
    case Icon::scale_chest:
        draw_torso(pen, tint, accent);
        // Rows of overlapping discs read as scales.
        for (int row = 0; row < 3 && !flat; ++row)
        {
            for (int i = 0; i < 4 - (row & 1); ++i)
            {
                const float x = -10.5f + 7.0f * static_cast<float>(i) + 3.5f * (row & 1);
                pen.disc(x, -5.0f + 6.0f * static_cast<float>(row), 3.6f, lighter(tint, 0.35f));
                pen.disc(x, -6.2f + 6.0f * static_cast<float>(row), 3.4f, tint);
            }
        }
        break;
    case Icon::gloves:
        for (int i = 0; i < 4; ++i)
            pen.rect(-12.0f + 6.2f * static_cast<float>(i), i == 0 || i == 3 ? -19.0f : -23.0f,
                     5.4f, 20, 2.7f, tint);
        pen.turned(-17, 2, 6.5f, 17, 3.2f, -0.6f, tint);
        pen.rect(-13, -6, 26, 24, 7, tint);
        pen.rect(-14, 14, 28, 9, 3, accent);
        pen.shine_rect(-8, -1, 16, 3, 1.5f, 0.18f);
        break;
    case Icon::legs:
        pen.rect(-17, -25, 14, 40, 5, tint);
        pen.rect(3, -25, 14, 40, 5, tint);
        pen.rect(-22, 11, 19, 12, 5, accent);
        pen.rect(3, 11, 19, 12, 5, accent);
        pen.disc(-10, -7, 4.5f, accent);
        pen.disc(10, -7, 4.5f, accent);
        break;
    case Icon::ring:
        pen.hoop(0, 7, 18, 5.5f, tint);
        if (!flat)
            pen.arc(0, 7, 18, 2.0f, -2.2f, 1.6f, lighter(tint, 0.55f), true);
        pen.turned(0, -13, 14, 14, 2, 0.7854f, accent);
        pen.shine_disc(-2, -15, 2.2f, 0.7f);
        break;
    case Icon::amulet:
        // The lower half of a thin ring is the chain.
        pen.arc(0, -16, 21, 2.4f, 1.5708f, 3.1416f, Color::rgb(kGold), true);
        pen.disc(0, 9, 12.5f, Color::rgb(kGold));
        pen.disc(0, 9, 9.5f, tint);
        pen.turned(0, 9, 9.5f, 9.5f, 1, 0.7854f, accent);
        pen.shine_disc(-2.5f, 6.5f, 2.0f, 0.7f);
        break;
    case Icon::potion:
        pen.rect(-5.5f, -19, 11, 16, 2, Color::rgb(0xd8e6ea, 0.85f));
        pen.rect(-7.5f, -20, 15, 4, 2, Color::rgb(0xd8e6ea));
        pen.rect(-5, -27, 10, 8, 2.5f, accent);
        pen.disc(0, 8, 18, Color::rgb(0xd8e6ea, 0.9f));
        pen.disc(0, 8, 15.5f, tint);
        if (!flat)
            pen.arc(0, 8, 15.5f, 15.5f, -1.5708f, 3.1416f, darker(tint, 0.3f));
        pen.shine_disc(-7, 12, 4, 0.4f);
        pen.shine_disc(4, 16, 1.8f, 0.3f);
        break;
    case Icon::flask:
        pen.rect(-5, -21, 10, 16, 2, Color::rgb(0xd8e6ea, 0.85f));
        pen.rect(-7, -22, 14, 4, 2, Color::rgb(0xd8e6ea));
        pen.rect(-4.5f, -28, 9, 7, 2.5f, accent);
        pen.tri(0, 7, 42, 34, 0.0f, Color::rgb(0xd8e6ea, 0.9f));
        pen.tri(0, 9.5f, 31, 25, 0.0f, tint);
        pen.shine_disc(-5, 15, 3, 0.4f);
        pen.shine_disc(3, 11, 1.8f, 0.35f);
        break;
    case Icon::bread:
        pen.rect(-24, -11, 48, 25, 12, accent);
        pen.rect(-22, -12, 44, 21, 10.5f, tint);
        for (int i = 0; i < 3; ++i)
            pen.turned(-11.0f + 11.0f * static_cast<float>(i), -2, 3, 13, 1.5f, 0.5f,
                       darker(tint, 0.3f));
        break;
    case Icon::scroll:
        pen.rect(-17, -14, 34, 28, 2, accent);
        for (int i = 0; i < 3 && !flat; ++i)
            pen.rect(-9, -7.0f + 6.0f * static_cast<float>(i), i == 2 ? 11.0f : 18.0f, 2, 1,
                     Color::rgb(0x5a4632, 0.55f));
        pen.rect(-24, -18, 9, 36, 4.5f, tint);
        pen.rect(15, -18, 9, 36, 4.5f, tint);
        pen.shine_rect(-22, -14, 2, 28, 1, 0.3f);
        pen.shine_rect(17, -14, 2, 28, 1, 0.3f);
        break;
    case Icon::gem:
        // A square on its point, with lighter and darker facets.
        pen.turned(0, 0, 30, 30, 3, 0.7854f, tint);
        if (!flat)
        {
            pen.tri(0, -10.5f, 21, 21, 0.0f, lighter(tint, 0.3f));
            pen.tri(0, 10.5f, 21, 21, kPi, darker(tint, 0.25f));
            pen.turned(0, 0, 14, 14, 1.5f, 0.7854f, accent.with_alpha(0.75f));
        }
        pen.shine_disc(-4, -6, 2.2f, 0.8f);
        break;
    case Icon::ore:
        pen.turned(-9, 7, 24, 20, 6, 0.25f, darker(tint, 0.25f));
        pen.turned(9, 5, 22, 22, 6, -0.3f, tint);
        pen.turned(-1, -9, 20, 17, 5, 0.5f, lighter(tint, 0.15f));
        pen.disc(-3, -9, 2.6f, accent);
        pen.disc(10, 3, 3.2f, accent);
        pen.disc(-10, 9, 2.2f, accent);
        break;
    case Icon::ingot:
        pen.cut(-17, -15, 40, 17, 5, darker(tint, 0.35f));
        pen.cut(-23, -3, 46, 19, 6, tint);
        pen.shine_rect(-16, 1, 32, 3.5f, 1.5f, 0.45f);
        pen.rect(-16, 10, 32, 2.5f, 1, darker(tint, 0.3f));
        break;
    case Icon::scale:
        pen.tri(0, 12, 31, 24, kPi, tint);
        pen.disc(0, -6, 17, tint);
        if (!flat)
        {
            pen.arc(0, -6, 17, 3, -1.9f, 2.2f, accent.with_alpha(0.8f), true);
            pen.stroke(0, -14, 0, 18, 1.8f, darker(tint, 0.35f));
        }
        break;
    case Icon::hide:
        pen.disc(-18, -13, 6.5f, tint);
        pen.disc(18, -13, 6.5f, tint);
        pen.disc(-18, 13, 6.5f, tint);
        pen.disc(18, 13, 6.5f, tint);
        pen.rect(-20, -17, 40, 34, 10, tint);
        pen.rect(-13, -10, 26, 20, 6, lighter(tint, 0.14f));
        for (int i = 0; i < 4 && !flat; ++i)
            pen.rect(-10.5f + 6.0f * static_cast<float>(i), -1, 3.5f, 2, 1, accent);
        break;
    case Icon::feather:
        pen.turned(2, -2, 15, 44, 7.5f, 0.62f, tint);
        if (!flat)
            pen.turned(4.5f, -4, 5, 38, 2.5f, 0.62f, lighter(tint, 0.22f));
        pen.stroke(-21, 23, 15, -19, 1.8f, accent);
        break;
    case Icon::letter:
        pen.rect(-23, -15, 46, 30, 3, tint);
        if (!flat)
        {
            pen.stroke(-21, -13, 0, 3, 1.4f, Color::rgb(0x6a5540, 0.6f));
            pen.stroke(21, -13, 0, 3, 1.4f, Color::rgb(0x6a5540, 0.6f));
        }
        pen.disc(0, 3, 6.5f, accent);
        pen.shine_disc(-1.5f, 1.5f, 1.8f, 0.4f);
        break;
    case Icon::key:
        pen.hoop(-13, 13, 10.5f, 4.2f, tint);
        pen.stroke(-6, 6, 19, -19, 4.2f, tint);
        pen.stroke(12, -12, 17.5f, -6.5f, 3.6f, tint);
        pen.stroke(18, -18, 23.5f, -12.5f, 3.6f, tint);
        pen.shine_disc(-17, 9, 1.6f, 0.6f);
        break;
    case Icon::map:
        pen.rect(-23, -17, 46, 34, 3, tint);
        if (!flat)
        {
            pen.rect(-8, -17, 1.5f, 34, 0, Color::rgb(0x6a5540, 0.3f));
            pen.rect(7, -17, 1.5f, 34, 0, Color::rgb(0x6a5540, 0.3f));
            for (int i = 0; i < 5; ++i)
            {
                // A dotted trail wandering up to the cross.
                const float t = static_cast<float>(i);
                pen.disc(-16.0f + 5.6f * t, 8.0f - 4.0f * std::sin(t * 0.9f) - 2.0f * t, 1.3f,
                         Color::rgb(0x6a5540, 0.8f));
            }
            pen.stroke(11, -9, 17, -3, 2.0f, accent);
            pen.stroke(17, -9, 11, -3, 2.0f, accent);
        }
        break;
    }
}

// ---- layout ----------------------------------------------------------------

Rect cell_rect(int cell)
{
    const int column = cell % kColumns;
    const int row = cell / kColumns;
    return {kGridX + static_cast<float>(column) * (kSlot + kGap),
            kGridY + static_cast<float>(row) * (kSlot + kGap), kSlot, kSlot};
}

// Head, chest, hands and legs down the left of the figure; the two hands and
// the trinket down its right.
Rect wear_rect(int slot)
{
    if (slot < 4)
        return {kCharX + 24.0f, 240.0f + static_cast<float>(slot) * kWearPitch, kSlot, kSlot};
    return {kCharX + kCharW - 24.0f - kSlot, 298.0f + static_cast<float>(slot - 4) * kWearPitch,
            kSlot, kSlot};
}

// A focusable place: a cell of the bag view or an equipment slot.
struct Place
{
    bool worn = false;
    int index = 0;
};

Rect place_rect(Place place)
{
    return place.worn ? wear_rect(place.index) : cell_rect(place.index);
}

// One item object. It lives in a bag slot, an equipment slot or the hand, and
// its drawn position chases wherever that is.
struct Stack
{
    bool alive = false;
    bool dying = false; // fading out after a merge or the last use
    int def = 0;
    int count = 0;
    tween::Spring x, y; // centre
    tween::Spring show; // 0 hidden by the filter, 1 visible
    tween::Spring lift; // 1 while carried
    ui::Pulse pop;      // count changed
    float delay = 0.0f; // seconds before it starts travelling (sort stagger)
};

struct Puff
{
    float x = 0.0f, y = 0.0f, dx = 0.0f, dy = 0.0f;
    float age = 1.0f; // 0..1; 1 is spent
    float size = 0.0f;
    Color color;
};

// What Cross would do with the carried item on the focused place.
enum class Verdict : std::uint8_t
{
    nothing,
    set_down,
    swap,
    merge,
    equip,
    wrong_slot,
    wrong_tab,
    no_room,
    stack_full,
};

bool is_refusal(Verdict verdict)
{
    return verdict >= Verdict::wrong_slot;
}

// What the tooltip shows. Two tips are equal when they describe the same
// thing; the count may change without a cross-fade.
struct Tip
{
    int def = -1; // -1: an empty place
    int count = 0;
    int wear = -1; // the equipment slot under the focus, if any
    bool worn = false;

    bool same(const Tip &other) const
    {
        return def == other.def && wear == other.wear && worn == other.worn;
    }
};

struct Line
{
    ui::Button button = ui::Button::none;
    char text[80] = "";
    bool bad = false;
};

ui::GlyphStyle glyph_style()
{
    return {Color::rgb(0x1a110b, 0.92f), Color::rgb(0x8f7658, 0.9f), kInk, kInk.with_alpha(0.86f),
            true};
}

class Inventory final : public app::Concept
{
  public:
    explicit Inventory(app::Context &context) : context_(context)
    {
        bag_.fill(-1);
        worn_.fill(-1);
        for (const Start &start : kStartBag)
            bag_[static_cast<std::size_t>(start.cell)] = spawn(start.id, start.count);
        for (Id id : kStartWorn)
            worn_[static_cast<std::size_t>(kItems[id].wear)] = spawn(id, 1);
        layout_tabs();
        rebuild_view();
        aim_stacks();
        for (Stack &stack : stacks_)
        {
            stack.x.snap(stack.x.target);
            stack.y.snap(stack.y.target);
            stack.show.snap(1.0f);
        }
        ring_.snap(place_rect(focus_).inset(-4));
        ring_color_.snap(focus_color());
        tab_ring_.snap(tab_rects_[0]);
        attack_.snap(static_cast<float>(total(StatId::attack)));
        defence_.snap(static_cast<float>(total(StatId::defence)));
        weight_.snap(carried_weight());
        shown_ = previous_ = current_tip();
        tip_color_.snap(tip_accent(shown_));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "inventory",
            "Satchel",
            "An inventory you handle: lift, carry, swap, stack and equip, with a comparing tooltip",
            "src/concepts/inventory.cpp",
            audio::SoundSet::paper,
            kAmber,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        menu_open_ = false;
        // Coming back with an item in the air would be a puzzle: put it home.
        if (held_ >= 0)
            return_held();
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        notice_age_ += dt;
        if (menu_open_)
            update_menu(input, feedback);
        else
            update_field(input, feedback);
        animate(dt);
    }

    void draw(app::Frame &frame) const override
    {
        // Firelight: slow amber discs over dark leather.
        frame.backdrop.mode = gfx::BackdropMode::bokeh;
        frame.backdrop.colors[0] = Color::rgb(0x1d130c);
        frame.backdrop.colors[1] = Color::rgb(0x0c0705);
        frame.backdrop.colors[2] = Color::rgb(0xa85f26);
        frame.backdrop.colors[3] = Color::rgb(0x6e2f16);
        frame.backdrop.time = context_.settings.reduced_motion ? 0.0f : clock_;

        gfx::DrawList &list = frame.scene;
        const float back = menu_.value;
        list.push_transform(1.0f - 0.02f * back, 960, 540, 0, 0);
        draw_header(list);
        draw_panels(list);
        draw_character(list);
        draw_tabs(list);
        draw_bag(list);
        draw_stacks(list);
        draw_effects(list);
        draw_footer(list);
        list.pop_transform();

        // The tooltip is glass, so the frame always asks for the blurred copy.
        // It lives in the overlay but belongs to the screen: it steps back with
        // the scene when the sort menu opens.
        frame.glass = true;
        frame.overlay.push_transform(1.0f - 0.02f * back, 960, 540, 0, 0);
        draw_tooltip(frame.overlay, frame.glass_texture);
        frame.overlay.pop_transform();
        if (back > 0.01f)
        {
            frame.overlay.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                                       Color::rgb(0x080403, 0.5f * back));
            draw_menu(frame.overlay, frame.glass_texture);
        }
        draw_hints(frame.overlay);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- the model ---------------------------------------------------------

    static const ItemDef &def_of(const Stack &stack)
    {
        return kItems[stack.def];
    }

    const Stack &stack(int index) const
    {
        return stacks_[static_cast<std::size_t>(index)];
    }

    Stack &stack(int index)
    {
        return stacks_[static_cast<std::size_t>(index)];
    }

    int spawn(int def, int count)
    {
        for (int i = 0; i < kPool; ++i)
        {
            Stack &s = stack(i);
            if (s.alive)
                continue;
            s = Stack{};
            s.alive = true;
            s.def = def;
            s.count = count;
            s.show.snap(1.0f);
            return i;
        }
        return -1;
    }

    static int tab_of(const ItemDef &def)
    {
        return static_cast<int>(def.category) + 1;
    }

    // The view maps cells to bag slots. "All" shows every slot in place; a
    // category shows its items packed from the first cell, in bag order.
    void rebuild_view()
    {
        view_.fill(-1);
        cell_of_.fill(-1);
        int next = 0;
        for (int slot = 0; slot < kCells; ++slot)
        {
            const int item = bag_[static_cast<std::size_t>(slot)];
            if (tab_ == 0)
            {
                view_[static_cast<std::size_t>(slot)] = slot;
                cell_of_[static_cast<std::size_t>(slot)] = slot;
            }
            else if (item >= 0 && tab_of(def_of(stack(item))) == tab_)
            {
                view_[static_cast<std::size_t>(next)] = slot;
                cell_of_[static_cast<std::size_t>(slot)] = next++;
            }
        }
    }

    int stack_at(Place place) const
    {
        if (place.worn)
            return worn_[static_cast<std::size_t>(place.index)];
        const int slot = view_[static_cast<std::size_t>(place.index)];
        return slot >= 0 ? bag_[static_cast<std::size_t>(slot)] : -1;
    }

    int first_free(int except = -1) const
    {
        for (int slot = 0; slot < kCells; ++slot)
        {
            if (slot != except && bag_[static_cast<std::size_t>(slot)] < 0)
                return slot;
        }
        return -1;
    }

    // The empty slot closest to a cell of the "All" view.
    int nearest_free(int cell) const
    {
        int best = -1;
        int best_distance = 1 << 20;
        for (int slot = 0; slot < kCells; ++slot)
        {
            if (bag_[static_cast<std::size_t>(slot)] >= 0)
                continue;
            const int dx = slot % kColumns - cell % kColumns;
            const int dy = slot / kColumns - cell / kColumns;
            if (dx * dx + dy * dy < best_distance)
            {
                best_distance = dx * dx + dy * dy;
                best = slot;
            }
        }
        return best;
    }

    int used_slots() const
    {
        int used = 0;
        for (int item : bag_)
            used += item >= 0 ? 1 : 0;
        return used;
    }

    int total(StatId id) const
    {
        int sum = 0;
        for (int item : worn_)
        {
            if (item < 0)
                continue;
            for (const Stat &stat : def_of(stack(item)).stats)
                sum += stat.id == id ? stat.value : 0;
        }
        return sum;
    }

    // Everything on the character: bag, gear and whatever is in the hand.
    float carried_weight() const
    {
        float sum = 0.0f;
        for (const Stack &s : stacks_)
        {
            if (s.alive && !s.dying)
                sum += def_of(s).weight * static_cast<float>(s.count);
        }
        return sum;
    }

    Verdict verdict() const
    {
        if (held_ < 0)
            return Verdict::nothing;
        const Stack &held = stack(held_);
        const ItemDef &def = def_of(held);
        if (focus_.worn)
        {
            return static_cast<int>(def.wear) == focus_.index ? Verdict::equip
                                                              : Verdict::wrong_slot;
        }
        // A filtered bag only takes what it shows.
        if (tab_ != 0 && tab_of(def) != tab_)
            return Verdict::wrong_tab;
        const int slot = view_[static_cast<std::size_t>(focus_.index)];
        const int other = slot >= 0 ? bag_[static_cast<std::size_t>(slot)] : -1;
        if (other < 0)
        {
            if (tab_ == 0 || !origin_.worn)
                return Verdict::set_down;
            return first_free() >= 0 ? Verdict::set_down : Verdict::no_room;
        }
        if (stack(other).def == held.def && def.max_stack > 1)
            return stack(other).count < def.max_stack ? Verdict::merge : Verdict::stack_full;
        // The displaced item goes where the carried one came from. Gear that
        // was lifted off the character leaves a slot only its own kind fits.
        if (!origin_.worn || static_cast<int>(def_of(stack(other)).wear) == origin_.index)
            return Verdict::swap;
        return first_free(slot) >= 0 ? Verdict::swap : Verdict::no_room;
    }

    // ---- input -------------------------------------------------------------

    void update_field(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav != Direction::none)
            move_focus(input, feedback);
        if (input.is_pressed(Action::jump_prev) || input.is_pressed(Action::jump_next))
            change_tab(input.is_pressed(Action::jump_next) ? 1 : -1, feedback);
        if (input.is_pressed(Action::confirm))
        {
            if (held_ < 0)
                pick_up(feedback);
            else
                place(feedback);
        }
        if (input.is_pressed(Action::back))
        {
            if (held_ >= 0)
            {
                // The item flies home; the focus stays where the player left it.
                return_held();
                feedback.play(audio::Cue::back, 1.0f, focus_pan());
                notice("Put back where it came from.", false);
            }
            else if (tab_ != 0)
            {
                change_tab(-tab_, feedback);
            }
        }
        if (input.is_pressed(Action::north))
        {
            if (held_ >= 0)
                refuse(feedback, "Set the item down first.");
            else
                quick_action(feedback);
        }
        if (input.is_pressed(Action::west))
        {
            if (held_ >= 0)
                refuse(feedback, "Set the item down first.");
            else
                split(feedback);
        }
        if (input.is_pressed(Action::menu))
        {
            if (held_ >= 0)
            {
                refuse(feedback, "Set the item down before sorting.");
            }
            else
            {
                menu_open_ = true;
                menu_position_.snap(static_cast<float>(sort_));
                feedback.play(audio::Cue::modal_open);
            }
        }
    }

    void update_menu(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = sort_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kSorts)
            {
                sort_ = next;
                feedback.play(audio::Cue::focus, 1.05f - 0.05f * static_cast<float>(sort_), 0.3f);
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.3f, 0.6f);
                nudge_.trigger();
                nudge_x_ = 0.0f;
                nudge_y_ = input.nav == Direction::down ? 1.0f : -1.0f;
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            sort_bag();
            menu_open_ = false;
            // One sound for the whole shuffle, not a chain of ticks.
            feedback.play(audio::Cue::cascade);
            char text[64];
            std::snprintf(text, sizeof(text), "Sorted %s.",
                          sort_ == 0   ? "by kind"
                          : sort_ == 1 ? "by rarity"
                                       : "by name");
            notice(text, false);
        }
        else if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
        {
            menu_open_ = false;
            feedback.play(audio::Cue::modal_close);
        }
    }

    float focus_pan() const
    {
        return ui::pan_for_x(place_rect(focus_).cx());
    }

    // The slot of [first, last] whose centre is closest in height.
    static int nearest_wear(int first, int last, float cy)
    {
        int best = first;
        for (int slot = first; slot <= last; ++slot)
        {
            if (std::fabs(wear_rect(slot).cy() - cy) < std::fabs(wear_rect(best).cy() - cy))
                best = slot;
        }
        return best;
    }

    void move_focus(const InputFrame &input, app::Feedback &feedback)
    {
        Place next = focus_;
        bool moved = false;
        const float cy = place_rect(focus_).cy();
        if (!focus_.worn)
        {
            const int column = focus_.index % kColumns;
            const int row = focus_.index / kColumns;
            switch (input.nav)
            {
            case Direction::left:
                // The first column opens onto the equipment beside it.
                next = column > 0 ? Place{false, focus_.index - 1}
                                  : Place{true, nearest_wear(4, 6, cy)};
                moved = true;
                break;
            case Direction::right:
                moved = column < kColumns - 1;
                next.index = focus_.index + 1;
                break;
            case Direction::up:
                moved = row > 0;
                next.index = focus_.index - kColumns;
                break;
            case Direction::down:
                moved = row < kRows - 1;
                next.index = focus_.index + kColumns;
                break;
            case Direction::none:
                break;
            }
        }
        else
        {
            const bool left_column = focus_.index < 4;
            const int first = left_column ? 0 : 4;
            const int last = left_column ? 3 : 6;
            switch (input.nav)
            {
            case Direction::left:
                moved = !left_column;
                next.index = nearest_wear(0, 3, cy);
                break;
            case Direction::right:
                moved = true;
                if (left_column)
                {
                    next.index = nearest_wear(4, 6, cy);
                }
                else
                {
                    // Back into the bag, on the row level with this slot.
                    const float row = (cy - kGridY - kSlot * 0.5f) / (kSlot + kGap);
                    next = {false, std::clamp(static_cast<int>(std::lround(row)), 0, kRows - 1) *
                                       kColumns};
                }
                break;
            case Direction::up:
                moved = focus_.index > first;
                next.index = focus_.index - 1;
                break;
            case Direction::down:
                moved = focus_.index < last;
                next.index = focus_.index + 1;
                break;
            case Direction::none:
                break;
            }
        }
        if (moved)
        {
            focus_ = next;
            const Rect r = place_rect(focus_);
            // Carrying sounds different from looking: the item slides along.
            if (held_ >= 0)
                feedback.play(audio::Cue::slide, 1.0f, ui::pan_for_x(r.cx()), 0.7f);
            else
                feedback.play(audio::Cue::focus, 1.06f - 0.00016f * r.cy(), ui::pan_for_x(r.cx()));
        }
        else if (!input.nav_repeat)
        {
            feedback.play(audio::Cue::error, 1.0f, focus_pan(), 0.6f);
            feedback.rumble(0.25f, 0.05f);
            nudge_.trigger();
            nudge_x_ = input.nav == Direction::left    ? -1.0f
                       : input.nav == Direction::right ? 1.0f
                                                       : 0.0f;
            nudge_y_ = input.nav == Direction::up     ? -1.0f
                       : input.nav == Direction::down ? 1.0f
                                                      : 0.0f;
        }
    }

    void change_tab(int step, app::Feedback &feedback)
    {
        const int next = tab_ + step;
        if (next < 0 || next >= kTabs)
        {
            feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            tab_nudge_.trigger();
            return;
        }
        tab_ = next;
        rebuild_view();
        feedback.play(audio::Cue::tab, 0.94f + 0.03f * static_cast<float>(tab_),
                      ui::pan_for_x(tab_rects_[static_cast<std::size_t>(tab_)].cx()));
    }

    void pick_up(app::Feedback &feedback)
    {
        const int item = stack_at(focus_);
        if (item < 0)
        {
            refuse(feedback, "Nothing here to lift.");
            return;
        }
        if (focus_.worn)
        {
            origin_ = focus_;
            worn_[static_cast<std::size_t>(focus_.index)] = -1;
        }
        else
        {
            const int slot = view_[static_cast<std::size_t>(focus_.index)];
            origin_ = {false, slot};
            bag_[static_cast<std::size_t>(slot)] = -1;
        }
        held_ = item;
        rebuild_view();
        feedback.play(audio::Cue::pickup, 1.0f, focus_pan());
    }

    // Puts the carried item back where it was lifted from.
    void return_held()
    {
        if (origin_.worn)
            worn_[static_cast<std::size_t>(origin_.index)] = held_;
        else
            bag_[static_cast<std::size_t>(origin_.index)] = held_;
        held_ = -1;
        rebuild_view();
    }

    void place(app::Feedback &feedback)
    {
        const Verdict what = verdict();
        Stack &held = stack(held_);
        const ItemDef &def = def_of(held);
        const float pan = focus_pan();
        char text[96];
        switch (what)
        {
        case Verdict::wrong_slot:
            std::snprintf(text, sizeof(text), "%s %s.", def.name, kWearRefusals[focus_.index]);
            refuse(feedback, text);
            return;
        case Verdict::wrong_tab:
            std::snprintf(text, sizeof(text), "This view holds %s only. Switch to All.",
                          kTabNames[tab_]);
            refuse(feedback, text);
            return;
        case Verdict::no_room:
            refuse(feedback, "The bag is full.");
            return;
        case Verdict::stack_full:
            refuse(feedback, "That stack is full.");
            return;
        case Verdict::nothing:
            return;
        case Verdict::equip:
        {
            // What was worn travels to the slot the new piece came from.
            const int before = worn_[static_cast<std::size_t>(focus_.index)];
            worn_[static_cast<std::size_t>(focus_.index)] = held_;
            if (before >= 0)
                bag_[static_cast<std::size_t>(origin_.index)] = before;
            held_ = -1;
            equipped(def, pan, feedback);
            break;
        }
        case Verdict::set_down:
        {
            int slot = focus_.index;
            if (tab_ != 0)
                slot = origin_.worn ? first_free() : origin_.index;
            bag_[static_cast<std::size_t>(slot)] = held_;
            held_ = -1;
            feedback.play(audio::Cue::drop, 1.0f, pan);
            break;
        }
        case Verdict::merge:
        {
            Stack &other = stack(stack_at(focus_));
            const int moved = std::min(held.count, def.max_stack - other.count);
            other.count += moved;
            held.count -= moved;
            other.pop.trigger();
            // The pitch climbs with the stack: a bigger pile sounds fuller.
            feedback.play(audio::Cue::merge, merge_pitch(other.count), pan);
            std::snprintf(text, sizeof(text), "Stacked: %d %s.", other.count, def.name);
            notice(text, false);
            if (held.count == 0)
            {
                // The carried stack sinks into the other one and is gone.
                held.dying = true;
                held.x.target = other.x.target;
                held.y.target = other.y.target;
                held_ = -1;
            }
            break;
        }
        case Verdict::swap:
        {
            const int slot = view_[static_cast<std::size_t>(focus_.index)];
            const int other = bag_[static_cast<std::size_t>(slot)];
            bag_[static_cast<std::size_t>(slot)] = held_;
            held_ = -1;
            if (!origin_.worn)
            {
                bag_[static_cast<std::size_t>(origin_.index)] = other;
                feedback.play(audio::Cue::rotate, 1.0f, pan);
            }
            else if (static_cast<int>(def_of(stack(other)).wear) == origin_.index)
            {
                worn_[static_cast<std::size_t>(origin_.index)] = other;
                equipped(def_of(stack(other)), pan, feedback);
            }
            else
            {
                bag_[static_cast<std::size_t>(first_free(slot))] = other;
                feedback.play(audio::Cue::rotate, 1.0f, pan);
            }
            break;
        }
        }
        rebuild_view();
    }

    static float merge_pitch(int count)
    {
        return std::min(1.4f, 0.8f + 0.02f * static_cast<float>(count));
    }

    // A stronger loadout answers higher.
    float power_pitch() const
    {
        const float power = static_cast<float>(total(StatId::attack) + total(StatId::defence));
        return std::clamp(0.8f + power / 250.0f, 0.8f, 1.4f);
    }

    void equipped(const ItemDef &def, float pan, app::Feedback &feedback)
    {
        feedback.play(audio::Cue::connect, power_pitch(), pan);
        figure_flash_.trigger();
        flash_color_ = rarity_color(def.rarity);
        char text[96];
        std::snprintf(text, sizeof(text), "%s equipped.", def.name);
        notice(text, false);
    }

    // Triangle: the obvious thing to do with what is under the focus.
    void quick_action(app::Feedback &feedback)
    {
        const int item = stack_at(focus_);
        if (item < 0)
        {
            refuse(feedback, "Nothing here to use.");
            return;
        }
        Stack &s = stack(item);
        const ItemDef &def = def_of(s);
        const float pan = focus_pan();
        char text[96];
        if (focus_.worn)
        {
            const int slot = first_free();
            if (slot < 0)
            {
                refuse(feedback, "The bag is full.");
                return;
            }
            worn_[static_cast<std::size_t>(focus_.index)] = -1;
            bag_[static_cast<std::size_t>(slot)] = item;
            feedback.play(audio::Cue::drop, 1.0f, pan);
            std::snprintf(text, sizeof(text), "%s taken off.", def.name);
            notice(text, false);
        }
        else if (def.wear != Wear::none)
        {
            // Quick-equip: trade places with whatever is worn there.
            const std::size_t wear = static_cast<std::size_t>(def.wear);
            const int slot = view_[static_cast<std::size_t>(focus_.index)];
            bag_[static_cast<std::size_t>(slot)] = worn_[wear];
            worn_[wear] = item;
            equipped(def, pan, feedback);
        }
        else if (def.category == Category::consumable)
        {
            s.count -= 1;
            s.pop.trigger();
            float_text(def, s.x.value, s.y.value);
            if (s.count > 0)
            {
                feedback.play(audio::Cue::reveal, 1.0f, pan);
                std::snprintf(text, sizeof(text), "Used one %s. %d left.", def.name, s.count);
            }
            else
            {
                // The last one: the stack disappears in a puff.
                const int slot = view_[static_cast<std::size_t>(focus_.index)];
                bag_[static_cast<std::size_t>(slot)] = -1;
                s.dying = true;
                puff(s.x.value, s.y.value, rarity_color(def.rarity));
                feedback.play(audio::Cue::flip, 1.0f, pan);
                std::snprintf(text, sizeof(text), "Used the last %s.", def.name);
            }
            notice(text, false);
        }
        else
        {
            refuse(feedback, def.category == Category::quest
                                 ? "Quest items cannot be used, only carried."
                                 : "Materials are for the forge, not for the road.");
            return;
        }
        rebuild_view();
    }

    // Square: half of a stack goes to the nearest empty slot.
    void split(app::Feedback &feedback)
    {
        const int item = focus_.worn ? -1 : stack_at(focus_);
        if (item < 0 || def_of(stack(item)).max_stack <= 1 || stack(item).count < 2)
        {
            refuse(feedback, item >= 0 && def_of(stack(item)).category == Category::quest
                                 ? "Quest items cannot be split."
                                 : "Only a stack of two or more can be split.");
            return;
        }
        const int slot = tab_ == 0 ? nearest_free(focus_.index) : first_free();
        const int half = slot >= 0 ? spawn(stack(item).def, stack(item).count / 2) : -1;
        if (half < 0)
        {
            refuse(feedback, "No empty slot for the other half.");
            return;
        }
        Stack &from = stack(item);
        Stack &to = stack(half);
        from.count -= to.count;
        from.pop.trigger();
        to.pop.trigger();
        // The new half starts on top of its parent and travels out of it.
        to.x.snap(from.x.value);
        to.y.snap(from.y.value);
        bag_[static_cast<std::size_t>(slot)] = half;
        rebuild_view();
        feedback.play(audio::Cue::place, 1.0f, focus_pan());
        char text[96];
        std::snprintf(text, sizeof(text), "Split into %d and %d.", from.count, to.count);
        notice(text, false);
    }

    void sort_bag()
    {
        std::array<int, kCells> items{};
        int count = 0;
        for (int item : bag_)
        {
            if (item >= 0)
                items[static_cast<std::size_t>(count++)] = item;
        }
        const int mode = sort_;
        std::stable_sort(items.begin(), items.begin() + count,
                         [this, mode](int a, int b)
                         {
                             const ItemDef &da = def_of(stack(a));
                             const ItemDef &db = def_of(stack(b));
                             if (mode == 0)
                             {
                                 if (da.category != db.category)
                                     return da.category < db.category;
                                 if (da.wear != db.wear)
                                     return da.wear < db.wear;
                             }
                             if (mode != 2 && da.rarity != db.rarity)
                                 return da.rarity > db.rarity;
                             const int order = std::strcmp(da.name, db.name);
                             if (order != 0)
                                 return order < 0;
                             return stack(a).count > stack(b).count;
                         });
        bag_.fill(-1);
        for (int i = 0; i < count; ++i)
        {
            const int item = items[static_cast<std::size_t>(i)];
            bag_[static_cast<std::size_t>(i)] = item;
            // Items leave one after the other, in the order they will lie.
            stack(item).delay =
                context_.settings.reduced_motion ? 0.0f : 0.016f * static_cast<float>(i);
        }
        rebuild_view();
    }

    void refuse(app::Feedback &feedback, const char *why)
    {
        feedback.play(audio::Cue::invalid, 1.0f, focus_pan(), 0.85f);
        feedback.rumble(0.25f, 0.05f);
        deny_.trigger();
        deny_rect_ = place_rect(focus_);
        notice(why, true);
    }

    void notice(const char *text, bool bad)
    {
        std::snprintf(notice_, sizeof(notice_), "%s", text);
        notice_bad_ = bad;
        notice_age_ = 0.0f;
    }

    void puff(float x, float y, Color color)
    {
        for (int i = 0; i < static_cast<int>(puffs_.size()); ++i)
        {
            const float angle = static_cast<float>(i) * 0.898f + 0.4f;
            const float reach = 44.0f + 9.0f * static_cast<float>(i % 3);
            Puff &p = puffs_[static_cast<std::size_t>(i)];
            p = {x,
                 y,
                 std::sin(angle) * reach,
                 -std::cos(angle) * reach,
                 0.0f,
                 5.0f + 2.0f * static_cast<float>(i % 3),
                 color};
        }
    }

    void float_text(const ItemDef &def, float x, float y)
    {
        const Stat &stat = def.stats[0];
        if (stat.id == StatId::health || stat.id == StatId::attack || stat.id == StatId::focus)
            std::snprintf(float_, sizeof(float_), "%+d %s", stat.value, stat_name(stat.id));
        else
            std::snprintf(float_, sizeof(float_), "Used");
        float_x_ = x;
        float_y_ = y;
        float_timer_.start(0.9f);
    }

    // ---- animation ---------------------------------------------------------

    static void aim(Stack &s, const Rect &r)
    {
        if (s.delay > 0.0f)
            return;
        s.x.target = r.cx();
        s.y.target = r.cy();
    }

    // Tells every item where it lives now. Hidden items keep their place and
    // fade, so they travel from it when the filter lets them back in.
    void aim_stacks()
    {
        for (int slot = 0; slot < kCells; ++slot)
        {
            const int item = bag_[static_cast<std::size_t>(slot)];
            if (item < 0)
                continue;
            Stack &s = stack(item);
            const int cell = cell_of_[static_cast<std::size_t>(slot)];
            s.show.target = cell >= 0 ? 1.0f : 0.0f;
            s.lift.target = 0.0f;
            if (cell >= 0)
                aim(s, cell_rect(cell));
        }
        for (int slot = 0; slot < kWearSlots; ++slot)
        {
            const int item = worn_[static_cast<std::size_t>(slot)];
            if (item < 0)
                continue;
            Stack &s = stack(item);
            s.show.target = 1.0f;
            s.lift.target = 0.0f;
            aim(s, wear_rect(slot));
        }
        if (held_ >= 0)
        {
            Stack &s = stack(held_);
            s.show.target = 1.0f;
            s.lift.target = 1.0f;
            s.delay = 0.0f;
            aim(s, place_rect(focus_));
        }
    }

    Color focus_color() const
    {
        if (is_refusal(verdict()))
            return kBad;
        const int item = held_ >= 0 ? held_ : stack_at(focus_);
        return item >= 0 ? gfx::mix(rarity_color(def_of(stack(item)).rarity), kInk, 0.35f) : kInk;
    }

    Tip current_tip() const
    {
        Tip tip;
        int item = stack_at(focus_);
        tip.wear = focus_.worn ? focus_.index : -1;
        // Over an empty place, and over any equipment slot, the tooltip keeps
        // describing the carried item: that is where its comparison matters.
        if (held_ >= 0 && (item < 0 || focus_.worn))
            item = held_;
        tip.worn = focus_.worn && item >= 0 && item != held_;
        if (item >= 0)
        {
            tip.def = stack(item).def;
            tip.count = stack(item).count;
        }
        return tip;
    }

    static Color tip_accent(const Tip &tip)
    {
        return tip.def >= 0 ? rarity_color(kItems[tip.def].rarity) : Color::rgb(0x8a7a66);
    }

    void animate(float dt)
    {
        const bool calm = context_.settings.reduced_motion;
        aim_stacks();
        for (int i = 0; i < kPool; ++i)
        {
            Stack &s = stack(i);
            if (!s.alive)
                continue;
            s.delay = std::max(0.0f, s.delay - dt);
            if (s.dying)
            {
                s.show.target = 0.0f;
                s.lift.target = 0.0f;
            }
            // The carried item is softer than the ring, so it trails behind it.
            const float omega = calm ? 60.0f : i == held_ ? 13.0f : 15.0f;
            s.x.update(dt, omega);
            s.y.update(dt, omega);
            s.show.update(dt, s.dying ? 16.0f : 12.0f);
            s.lift.update(dt, calm ? 60.0f : 20.0f);
            s.pop.update(dt, 7.0f);
            if (s.dying && s.show.value < 0.03f)
                s.alive = false;
        }

        ring_.target(place_rect(focus_).inset(-4));
        ring_.update(dt, calm ? 60.0f : 22.0f);
        ring_color_.target(focus_color());
        ring_color_.update(dt, 14.0f);
        tab_ring_.target(tab_rects_[static_cast<std::size_t>(tab_)]);
        tab_ring_.update(dt, calm ? 60.0f : 18.0f);
        tab_nudge_.update(dt, 9.0f);
        nudge_.update(dt, 9.0f);
        deny_.update(dt, 5.0f);
        figure_flash_.update(dt, 3.5f);
        carry_.target = held_ >= 0 ? 1.0f : 0.0f;
        carry_.update(dt, 10.0f);

        // The totals count to their new values instead of jumping.
        attack_.target = static_cast<float>(total(StatId::attack));
        defence_.target = static_cast<float>(total(StatId::defence));
        weight_.target = carried_weight();
        const float counting = calm ? 60.0f : 7.0f;
        attack_.update(dt, counting);
        defence_.update(dt, counting);
        weight_.update(dt, counting);

        const Tip tip = current_tip();
        if (!tip.same(shown_))
        {
            previous_ = shown_;
            tip_timer_.start(calm ? 0.12f : 0.3f);
            tip_color_.target(tip_accent(tip));
        }
        shown_ = tip;
        tip_timer_.update(dt);
        tip_color_.update(dt, 9.0f);

        menu_.target = menu_open_ ? 1.0f : 0.0f;
        menu_.update(dt, calm ? 40.0f : 14.0f);
        menu_position_.target = static_cast<float>(sort_);
        menu_position_.update(dt, 22.0f);

        for (Puff &p : puffs_)
            p.age = std::min(1.0f, p.age + dt / 0.55f);
        float_timer_.update(dt);
    }

    // ---- drawing -----------------------------------------------------------

    // The screen assembles as a diagonal sweep from the top-left corner.
    float arrive(float x, float y) const
    {
        const int index = static_cast<int>((x - kMargin) / 150.0f + (y - 120.0f) / 150.0f);
        return tween::stagger(age_, std::max(0, index), 0.045f, 0.45f);
    }

    float rise(float in) const
    {
        return context_.settings.reduced_motion ? 0.0f : 22.0f * (1.0f - in);
    }

    static void panel(gfx::DrawList &list, const Rect &r)
    {
        list.shadow({r.x, r.y + 12, r.w, r.h}, kPanelRadius, 34, Color::rgb(0x000000, 0.4f));
        list.gradient_rect(r, kPanelRadius, kLeather.with_alpha(0.86f),
                           darker(kLeather, 0.3f).with_alpha(0.9f));
        list.bordered_rect(r, kPanelRadius, kClear, 1.5f, kInk.with_alpha(0.1f));
    }

    // An inset well: darker than the panel, shaded from the top, fine border.
    static void well(gfx::DrawList &list, const Rect &r, float edge = 0.55f)
    {
        list.rounded_rect(r, kRadius, kWell.with_alpha(0.82f));
        list.gradient_rect({r.x, r.y, r.w, r.h * 0.6f}, kRadius, Color::rgb(0x000000, 0.42f),
                           kClear);
        list.bordered_rect(r, kRadius, kClear, 1.5f, kWellEdge.with_alpha(edge));
    }

    void label(gfx::DrawList &list, const char *text, float x, float y, Color color,
               gfx::Align align = gfx::Align::left) const
    {
        ui::text(list, context_.fonts.semibold, text, x, y, 16, color, align, 3.0f);
    }

    void draw_header(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.05f, 0.5f);
        list.push_opacity(in);
        const float y = 136.0f - rise(in) * 0.5f;
        const float w = ui::text(list, fonts.display, "Satchel", kMargin - 3, y, 62, kInk);
        ui::text(list, fonts.regular, "Wren Ashdale  \xC2\xB7  Wayfarer of the ninth road",
                 kMargin + w + 28, y, 24, kInk.with_alpha(0.6f));
        // The purse, top right: a coin from two discs.
        ui::text(list, fonts.semibold, "1,284", 1824, y, 28, kInk, gfx::Align::right);
        const float coin = 1824.0f - fonts.semibold.measure("1,284", 28) - 28.0f;
        list.circle(coin, y - 10, 15, Color::rgb(0xb07a2a));
        list.circle(coin, y - 11.5f, 13.5f, Color::rgb(kGold));
        list.ring(coin, y - 11.5f, 9, 1.6f, Color::rgb(0xb07a2a, 0.8f));
        list.pop_opacity();
    }

    // The silhouette takes the colour of what it wears and lights the part
    // that belongs to the focused equipment slot.
    Color part_color(Wear wear) const
    {
        Color color = kHide;
        const int item = worn_[static_cast<std::size_t>(wear)];
        if (item >= 0)
            color = gfx::mix(color, rarity_color(def_of(stack(item)).rarity), 0.34f);
        if (focus_.worn && focus_.index == static_cast<int>(wear))
            color = lighter(color, 0.28f + 0.1f * ui::breathe(clock_));
        return gfx::mix(color, flash_color_, 0.5f * figure_flash_.value);
    }

    void draw_figure(gfx::DrawList &list) const
    {
        const float cx = kCharX + kCharW * 0.5f;
        const bool calm = context_.settings.reduced_motion;
        // A slow breath: the shoulders rise a pixel or two.
        const float breath = calm ? 0.0f : std::sin(clock_ * 1.3f) * 1.5f;
        if (figure_flash_.value > 0.01f)
            list.glow({cx - 70, 300, 140, 330}, 70, 70,
                      flash_color_.with_alpha(0.45f * figure_flash_.value));
        const Color legs = part_color(Wear::legs);
        const Color body = part_color(Wear::chest);
        const Color hands = part_color(Wear::hands);
        list.shadow({cx - 84, 662, 168, 14}, 7, 20, Color::rgb(0x000000, 0.55f));
        list.rounded_rect({cx - 52, 652, 50, 22}, 10, darker(legs, 0.18f));
        list.rounded_rect({cx + 2, 652, 50, 22}, 10, darker(legs, 0.18f));
        list.rounded_rect({cx - 44, 500, 40, 166}, 16, legs);
        list.rounded_rect({cx + 4, 500, 40, 166}, 16, legs);
        // Arms are turned rectangles hanging from the shoulders.
        list.rotated_rect({cx - 87, 356 - breath, 26, 138}, 13, 0.15f, darker(body, 0.12f));
        list.rotated_rect({cx + 61, 356 - breath, 26, 138}, 13, -0.15f, darker(body, 0.12f));
        list.circle(cx - 85, 494 - breath, 15, hands);
        list.circle(cx + 85, 494 - breath, 15, hands);
        list.rounded_rect({cx - 46, 478, 92, 44}, 18, darker(legs, 0.1f));
        list.rounded_rect({cx - 10, 322 - breath, 20, 30}, 6, darker(kHide, 0.1f));
        // Shoulders wider than the waist, and a belt where body and legs meet.
        list.rounded_rect({cx - 62, 342 - breath, 124, 64}, 28, body);
        list.rounded_rect({cx - 50, 362 - breath, 100, 132}, 22, body);
        list.rounded_rect({cx - 50, 480, 100, 11}, 4, darker(body, 0.32f));
        list.rounded_rect({cx - 8, 479, 16, 13}, 3, gfx::mix(kHide, kAmber, 0.45f));
        list.circle(cx, 298 - breath, 34, part_color(Wear::head));

        // What the hands hold and the trinket at the collar, in the items' own colours.
        const int weapon = worn_[static_cast<std::size_t>(Wear::main_hand)];
        const bool on_weapon = focus_.worn && focus_.index == static_cast<int>(Wear::main_hand);
        if (weapon >= 0)
            list.rotated_rect({cx + 95, 414 - breath, 7, 92}, 3, 0.2f,
                              Color::rgb(def_of(stack(weapon)).tint));
        else if (on_weapon)
            list.ring(cx + 85, 494 - breath, 21, 2, kInk.with_alpha(0.5f));
        const int shield = worn_[static_cast<std::size_t>(Wear::off_hand)];
        const bool on_shield = focus_.worn && focus_.index == static_cast<int>(Wear::off_hand);
        if (shield >= 0)
        {
            list.circle(cx - 88, 478 - breath, 27, Color::rgb(def_of(stack(shield)).accent));
            list.circle(cx - 88, 478 - breath, 23, Color::rgb(def_of(stack(shield)).tint));
            list.circle(cx - 88, 478 - breath, 6, Color::rgb(def_of(stack(shield)).accent));
        }
        else if (on_shield)
        {
            list.ring(cx - 85, 494 - breath, 21, 2, kInk.with_alpha(0.5f));
        }
        const int trinket = worn_[static_cast<std::size_t>(Wear::trinket)];
        const bool on_trinket = focus_.worn && focus_.index == static_cast<int>(Wear::trinket);
        if (trinket >= 0)
            list.circle(cx, 366 - breath, 7, Color::rgb(def_of(stack(trinket)).accent));
        else if (on_trinket)
            list.ring(cx, 366 - breath, 9, 2, kInk.with_alpha(0.5f));
    }

    // The two leather panels and, on top of them, the light of the focus
    // ring. A glow fills its interior, so it has to lie under the wells: an
    // empty focused slot then stays a dark well with light around it.
    void draw_panels(gfx::DrawList &list) const
    {
        const float left = tween::stagger(age_, 1, 0.06f, 0.5f);
        list.push_opacity(left);
        panel(list, {kCharX, kTop + rise(left), kCharW, kBottom - kTop});
        list.pop_opacity();
        list.push_opacity(tween::stagger(age_, 2, 0.06f, 0.5f));
        panel(list, {kBagX, kGridY - 16, kBagW, kRows * (kSlot + kGap) - kGap + 32});
        list.pop_opacity();

        const Rect ring = focus_ring();
        list.glow(
            ring, kRadius + 4, 20,
            ring_color_.value((0.26f + 0.12f * ui::breathe(clock_)) * arrive(ring.x, ring.y)));
        if (deny_.value > 0.01f)
            list.glow(deny_rect_, kRadius, 26, kBad.with_alpha(0.7f * deny_.value));
    }

    void draw_character(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect card{kCharX, kTop, kCharW, kBottom - kTop};
        const float in = tween::stagger(age_, 1, 0.06f, 0.5f);
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, rise(in));
        // Centred over the figure, clear of an item lifted off the top slots.
        label(list, "EQUIPMENT", card.cx(), card.y + 40, kAmber, gfx::Align::center);
        draw_figure(list);

        for (int slot = 0; slot < kWearSlots; ++slot)
        {
            const Rect r = wear_rect(slot);
            // The slot a carried item belongs in pulses: it would take it.
            const bool wanted = held_ >= 0 && static_cast<int>(def_of(stack(held_)).wear) == slot;
            if (wanted)
                list.glow(r, kRadius, 18, target_light(0.3f, 0.4f));
            well(list, r);
            if (wanted)
                list.bordered_rect(r, kRadius, kClear, 2, target_light(0.5f, 0.5f));
            if (worn_[static_cast<std::size_t>(slot)] < 0)
                draw_ghost(list, slot, r.cx(), r.cy(), 50);
            const bool focused = focus_.worn && focus_.index == slot;
            label(list, ui::upper(kWearNames[slot]).c_str(), r.cx(), r.y + r.h + 19,
                  kInk.with_alpha(focused ? 0.95f : 0.5f), gfx::Align::center);
        }

        // Totals.
        const float y = 720.0f;
        list.rounded_rect({card.x + 24, y - 14, card.w - 48, 1.5f}, 0, kInk.with_alpha(0.1f));
        draw_total(list, {card.x + 24, y, 208, 84}, "ATTACK", attack_, Wear::main_hand);
        draw_total(list, {card.x + 248, y, 208, 84}, "DEFENCE", defence_, Wear::off_hand);

        // The carry bar warms from parchment to amber as the limit comes near.
        const float fraction = weight_.value / kCarryLimit;
        const Color bar_color =
            gfx::mix(kInk.with_alpha(0.75f), kWarm, tween::inverse_lerp(0.7f, 0.9f, fraction));
        label(list, "CARRY WEIGHT", card.x + 24, 842, kInk.with_alpha(0.55f));
        char text[48];
        std::snprintf(text, sizeof(text), "%.1f / %.0f", static_cast<double>(weight_.value),
                      static_cast<double>(kCarryLimit));
        ui::text(list, fonts.mono, text, card.x + card.w - 24, 843, 21, bar_color,
                 gfx::Align::right);
        const Rect bar{card.x + 24, 858, card.w - 48, 10};
        list.rounded_rect(bar, 5, kWell.with_alpha(0.85f));
        if (fraction > 0.8f)
            list.glow({bar.x, bar.y, bar.w * tween::clamp01(fraction), bar.h}, 5, 10,
                      kWarm.with_alpha(0.25f + 0.12f * pulse()));
        list.gradient_rect_h(
            {bar.x, bar.y, std::max(10.0f, bar.w * tween::clamp01(fraction)), bar.h}, 5,
            darker(bar_color, 0.25f), bar_color);
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_total(gfx::DrawList &list, const Rect &r, const char *name,
                    const tween::Spring &value, Wear emblem) const
    {
        const ui::Fonts &fonts = context_.fonts;
        list.rounded_rect(r, 18, kWell.with_alpha(0.5f));
        label(list, name, r.x + 20, r.y + 32, kInk.with_alpha(0.55f));
        // While the number is still counting it glows the colour of the change.
        const float moving = tween::clamp01(std::fabs(value.target - value.value) / 3.0f);
        const Color color = gfx::mix(kInk, value.target > value.value ? kGood : kBad, moving);
        char text[16];
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(std::lround(value.value)));
        ui::text(list, fonts.mono, text, r.x + 19, r.y + 70, 36, color);
        draw_ghost(list, static_cast<int>(emblem), r.x + r.w - 44, r.cy(), 44);
    }

    // The emblem of an equipment slot: the icon of a typical piece, flat.
    static void draw_ghost(gfx::DrawList &list, int slot, float cx, float cy, float size)
    {
        constexpr Id kTypical[kWearSlots] = {
            helm_of_the_pale_watch, quilted_gambeson, gauntlets,       greaves,
            wayfarers_blade,        bulwark,          ring_of_promises};
        draw_icon(list, kItems[kTypical[slot]], cx, cy, size, true, Color::rgb(0x4a3929));
    }

    // Shared slow pulse for everything that says "this can go here".
    float pulse() const
    {
        return context_.settings.reduced_motion ? 0.5f : ui::breathe(clock_, 1.3f);
    }

    // The amber light of a place that would take the carried item. It fades
    // in with the lift and breathes between `low` and `low + swing`.
    Color target_light(float low, float swing) const
    {
        return kAmber.with_alpha(carry_.value * (low + swing * pulse()));
    }

    void layout_tabs()
    {
        const ui::Fonts &fonts = context_.fonts;
        const float glyph = ui::button_width(ui::Button::l2, 36) + 14.0f;
        const float room = kBagW - 2.0f * glyph;
        float sum = 0.0f;
        float widths[kTabs];
        for (int i = 0; i < kTabs; ++i)
        {
            widths[i] = fonts.semibold.measure(kTabNames[i], 22);
            sum += widths[i];
        }
        // The spare width is shared out as padding, so the row fills the column.
        const float pad = (room - sum) / static_cast<float>(kTabs);
        float x = kBagX + glyph;
        for (int i = 0; i < kTabs; ++i)
        {
            tab_rects_[static_cast<std::size_t>(i)] = {x, kTop, widths[i] + pad, 48};
            x += widths[i] + pad;
        }
    }

    void draw_tabs(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 2, 0.06f, 0.5f);
        list.push_opacity(in);
        const ui::GlyphStyle style = glyph_style();
        const float nudge = ui::shake(tab_nudge_.value, clock_, 8.0f, 9.0f);
        ui::draw_button(list, fonts, style, ui::Button::l2, kBagX, kTop + 24, 36);
        ui::draw_button(list, fonts, style, ui::Button::r2,
                        kBagX + kBagW - ui::button_width(ui::Button::r2, 36), kTop + 24, 36);
        Rect pill = tab_ring_.value();
        pill.x += nudge;
        list.rounded_rect(pill.inset(2), 22, kInk.with_alpha(0.1f));
        list.rounded_rect({pill.x + 18, pill.y + pill.h - 5, pill.w - 36, 3}, 1.5f, kAmber);
        for (int i = 0; i < kTabs; ++i)
        {
            const Rect &r = tab_rects_[static_cast<std::size_t>(i)];
            const bool active = i == tab_;
            ui::text(list, active ? fonts.semibold : fonts.regular, kTabNames[i], r.cx(),
                     r.y + 32 - rise(in) * 0.4f, 22, kInk.with_alpha(active ? 1.0f : 0.58f),
                     gfx::Align::center);
        }
        list.pop_opacity();
    }

    void draw_bag(gfx::DrawList &list) const
    {
        const Verdict what = verdict();
        for (int cell = 0; cell < kCells; ++cell)
        {
            const Rect r = cell_rect(cell);
            const float appear = arrive(r.x, r.y);
            list.push_opacity(appear);
            const int item = stack_at({false, cell});
            // A carried stack makes its twins pulse: they would take it in.
            const bool twin = held_ >= 0 && item >= 0 && stack(item).def == stack(held_).def &&
                              def_of(stack(item)).max_stack > 1;
            if (twin)
                list.glow(r, kRadius, 18, target_light(0.3f, 0.4f));
            // Empty cells brighten a little while something is in the hand.
            const bool open = held_ >= 0 && item < 0 && !is_refusal(what) &&
                              what != Verdict::nothing && (tab_ == 0 || cell == packed_count());
            well(list, r, open ? 0.55f + 0.3f * carry_.value * pulse() : 0.55f);
            if (twin)
                list.bordered_rect(r, kRadius, kClear, 2, target_light(0.5f, 0.5f));
            list.pop_opacity();
        }
        // Where the carried item came from: a dashed-looking mark in its slot.
        if (held_ >= 0 && !origin_.worn && tab_ == 0)
        {
            const Rect r = cell_rect(origin_.index).inset(30);
            list.bordered_rect(r, 8, kClear, 2, kInk.with_alpha(0.22f * carry_.value));
        }
        else if (held_ >= 0 && origin_.worn)
        {
            const Rect r = wear_rect(origin_.index).inset(4);
            list.bordered_rect(r, 11, kClear, 2, kInk.with_alpha(0.16f * carry_.value));
        }
    }

    // In a filtered view, the cell after the last shown item.
    int packed_count() const
    {
        int count = 0;
        for (int slot : view_)
            count += slot >= 0 ? 1 : 0;
        return count;
    }

    // One item: its own tile (rarity border, glow rising from below), the icon
    // and the count. A lifted item grows, tilts and casts a shadow.
    void draw_stack(gfx::DrawList &list, const Stack &s, float alpha) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const ItemDef &def = def_of(s);
        const Color rarity = rarity_color(def.rarity);
        const float lift = s.lift.value;
        const float scale = 1.0f + kLiftScale * lift + 0.16f * s.pop.value -
                            (s.dying ? 0.4f * (1.0f - s.show.value) : 0.0f);
        float cx = s.x.value;
        const float cy = s.y.value - kLift * lift;
        const bool calm = context_.settings.reduced_motion;
        if (lift > 0.5f)
            cx += ui::shake(deny_.value, clock_, 10.0f, 11.0f);
        const Rect tile{cx - kPlate * 0.5f, cy - kPlate * 0.5f, kPlate, kPlate};
        const Color fill = gfx::mix(Color::rgb(0x1b120b), rarity, 0.1f);

        list.push_opacity(alpha);
        list.push_transform(scale, cx, cy, 0, 0);
        if (lift > 0.02f)
        {
            // The tile leans into its movement: a turned backing under an
            // upright icon reads as the whole item tilting.
            const float lean = std::clamp(s.x.velocity * 0.00028f, -0.14f, 0.14f);
            const float tilt = calm ? 0.0f : lift * (lean - 0.05f);
            list.shadow({tile.x, tile.y + (10 + 16 * lift) / scale, tile.w, tile.h}, 12,
                        18 + 20 * lift, Color::rgb(0x000000, 0.3f + 0.3f * lift));
            list.rotated_rect(tile.inset(-1.5f), 13, tilt, rarity);
            list.rotated_rect(tile.inset(1.5f), 10.5f, tilt, gfx::mix(fill, rarity, 0.08f));
            list.rotated_rect({tile.x + 4, tile.y + tile.h * 0.5f, tile.w - 8, tile.h * 0.5f - 4},
                              9, tilt, rarity.with_alpha(0.16f));
        }
        else
        {
            list.rounded_rect(tile, 11, fill);
            list.gradient_rect(tile, 11, rarity.with_alpha(0.0f), rarity.with_alpha(0.3f));
            list.bordered_rect(tile, 11, kClear, 2, rarity.with_alpha(0.9f));
        }
        draw_icon(list, def, cx, cy - (def.max_stack > 1 ? 3.0f : 0.0f), 56);
        // Quest items carry a small amber diamond in the corner.
        if (def.category == Category::quest)
            list.rotated_rect({tile.x + 7, tile.y + 7, 10, 10}, 2, 0.7854f, kAmber);
        if (def.max_stack > 1 && s.count > 0)
        {
            // The count sits on a dark plate, in the face whose digits share
            // one width, so it does not jitter as it changes.
            char text[8];
            std::snprintf(text, sizeof(text), "%d", s.count);
            const float w = fonts.mono.measure(text, 20);
            list.rounded_rect({tile.x + tile.w - w - 15, tile.y + tile.h - 28, w + 11, 24}, 8,
                              Color::rgb(0x0e0805, 0.86f));
            ui::text(list, fonts.mono, text, tile.x + tile.w - 9.5f, tile.y + tile.h - 9.5f, 20,
                     kInk, gfx::Align::right);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    static bool in_motion(const Stack &s)
    {
        return std::fabs(s.x.value - s.x.target) + std::fabs(s.y.value - s.y.target) > 1.0f ||
               s.lift.value > 0.02f || s.pop.value > 0.01f;
    }

    void draw_stacks(gfx::DrawList &list) const
    {
        const Rect ring = focus_ring();
        const float ring_in = arrive(ring.x, ring.y);
        if (deny_.value > 0.01f)
        {
            // A refused place flashes red.
            list.rounded_rect(deny_rect_, kRadius, kBad.with_alpha(0.4f * deny_.value));
            list.bordered_rect(deny_rect_, kRadius, kClear, 3, kBad.with_alpha(deny_.value));
        }

        // Resting items first, travelling ones above them, the carried one last.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < kPool; ++i)
            {
                const Stack &s = stack(i);
                if (!s.alive || i == held_ || in_motion(s) != (pass == 1))
                    continue;
                draw_stack(list, s, s.show.value * arrive(s.x.value - 44, s.y.value - 44));
            }
        }
        // The ring's border goes over the items, under the one in the hand.
        list.bordered_rect(ring, kRadius + 4, kClear, 3, ring_color_.value(ring_in));
        if (held_ >= 0)
            draw_stack(list, stack(held_), 1.0f);
    }

    Rect focus_ring() const
    {
        Rect ring = ring_.value();
        const float amount = ui::shake(nudge_.value, clock_, 10.0f, 9.0f);
        ring.x += amount * nudge_x_;
        ring.y += amount * nudge_y_;
        return ring;
    }

    void draw_effects(gfx::DrawList &list) const
    {
        // The puff of a used-up stack: small discs that spread, grow and fade.
        for (const Puff &p : puffs_)
        {
            if (p.age >= 1.0f)
                continue;
            const float t = tween::cubic_out(p.age);
            list.circle(p.x + p.dx * t, p.y + p.dy * t, p.size * (0.6f + 1.2f * t),
                        gfx::mix(p.color, kInk, 0.4f).with_alpha(0.7f * (1.0f - p.age)));
        }
        if (float_timer_.running)
        {
            const float t = float_timer_.progress();
            const float alpha = tween::clamp01(t * 6.0f) * (1.0f - tween::cubic_in(t));
            const float y = float_y_ - 52.0f - 34.0f * tween::cubic_out(t);
            const float w = context_.fonts.semibold.measure(float_, 24) + 28.0f;
            list.rounded_rect({float_x_ - w * 0.5f, y - 25, w, 36}, 18,
                              Color::rgb(0x120b07, 0.85f * alpha));
            ui::text(list, context_.fonts.semibold, float_, float_x_, y, 24,
                     kGood.with_alpha(alpha), gfx::Align::center);
        }
    }

    // The strip under the bag: how full it is, the rarity key, and one line
    // that explains the last thing that happened or was refused.
    void draw_footer(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect card{kBagX, 788, kBagW, kBottom - 788};
        const float in = tween::stagger(age_, 6, 0.06f, 0.5f);
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, rise(in));
        panel(list, card);
        char text[32];
        label(list, "BAG", card.x + 24, card.y + 38, kInk.with_alpha(0.55f));
        std::snprintf(text, sizeof(text), "%d / %d", used_slots(), kCells);
        ui::text(list, fonts.mono, text, card.x + 78, card.y + 39, 21, kInk);
        float x = card.x + card.w - 24;
        for (int i = 4; i >= 0; --i)
        {
            const Color c = rarity_color(static_cast<Rarity>(i));
            x -= ui::text(list, fonts.regular, kRarityNames[i], x, card.y + 38, 20,
                          kInk.with_alpha(0.62f), gfx::Align::right);
            list.circle(x - 13, card.y + 31, 5.5f, c);
            x -= 40;
        }
        list.rounded_rect({card.x + 24, card.y + 56, card.w - 48, 1.5f}, 0, kInk.with_alpha(0.1f));

        // The notice fades over the standing line and gives it back later.
        // The standing line is gone before the notice arrives, so the two are
        // never legible on top of each other.
        const float gone = 1.0f - tween::smoothstep((notice_age_ - 3.2f) / 0.5f);
        const bool noticed = notice_[0] != '\0';
        const float show = noticed ? tween::clamp01((notice_age_ - 0.07f) / 0.16f) * gone : 0.0f;
        const float rest = noticed ? 1.0f - tween::clamp01(notice_age_ / 0.07f) * gone : 1.0f;
        const float base = card.y + 92;
        if (rest > 0.01f)
            ui::text(list, fonts.regular, fonts.regular.font->fit(standing_line(), 24, card.w - 48),
                     card.x + 24, base, 24, kInk.with_alpha(0.62f * rest * rest));
        if (show > 0.01f)
        {
            const Color c = notice_bad_ ? kBad : kAmber;
            list.circle(card.x + 31, base - 8, 5, c.with_alpha(show));
            ui::text(list, fonts.regular, fonts.regular.font->fit(notice_, 24, card.w - 76),
                     card.x + 48, base + 4.0f * (1.0f - show), 24,
                     gfx::mix(kInk, c, notice_bad_ ? 0.35f : 0.0f).with_alpha(show));
        }
        list.pop_transform();
        list.pop_opacity();
    }

    std::string standing_line() const
    {
        if (held_ >= 0)
        {
            const ItemDef &def = def_of(stack(held_));
            return std::string("Carrying ") + def.name +
                   (def.wear != Wear::none ? ". Its slot on the figure is lit."
                    : def.max_stack > 1    ? ". Matching stacks are lit."
                                           : ". Set it down anywhere.");
        }
        if (tab_ != 0)
            return std::string("Showing ") + kTabNames[tab_] +
                   " only. Items keep their place in the bag.";
        return "Lift an item, carry it and set it down. Like kinds stack.";
    }

    // ---- the tooltip -------------------------------------------------------

    // The worn item a tip is compared with, or -1.
    int rival(const Tip &tip) const
    {
        if (tip.def < 0 || tip.worn || kItems[tip.def].wear == Wear::none)
            return -1;
        const int item = worn_[static_cast<std::size_t>(kItems[tip.def].wear)];
        return item >= 0 ? stack(item).def : -1;
    }

    static int stat_value(const ItemDef &def, StatId id)
    {
        for (const Stat &stat : def.stats)
        {
            if (stat.id == id)
                return stat.value;
        }
        return 0;
    }

    // The lines of actions under the tooltip: what each button would do now.
    int actions(Line *lines) const
    {
        int count = 0;
        const auto add = [&](ui::Button button, const char *text, bool bad = false)
        {
            Line &line = lines[count++];
            line.button = button;
            line.bad = bad;
            std::snprintf(line.text, sizeof(line.text), "%s", text);
        };
        char text[80];
        if (held_ >= 0)
        {
            switch (verdict())
            {
            case Verdict::set_down:
                add(ui::Button::cross, "Set down here");
                break;
            case Verdict::swap:
                add(ui::Button::cross, "Swap the two");
                break;
            case Verdict::merge:
                add(ui::Button::cross, "Add to this stack");
                break;
            case Verdict::equip:
                add(ui::Button::cross, "Equip");
                break;
            case Verdict::wrong_slot:
                add(ui::Button::cross, "Does not belong here", true);
                break;
            case Verdict::wrong_tab:
                add(ui::Button::cross, "Not in this view", true);
                break;
            case Verdict::no_room:
                add(ui::Button::cross, "The bag is full", true);
                break;
            case Verdict::stack_full:
                add(ui::Button::cross, "Stack is full", true);
                break;
            case Verdict::nothing:
                break;
            }
            add(ui::Button::circle, "Put it back");
            return count;
        }
        const int item = stack_at(focus_);
        if (item < 0)
            return 0;
        const ItemDef &def = def_of(stack(item));
        add(ui::Button::cross, "Lift and carry");
        if (focus_.worn)
        {
            add(ui::Button::triangle, "Take off");
        }
        else if (def.wear != Wear::none)
        {
            const int worn = worn_[static_cast<std::size_t>(def.wear)];
            if (worn >= 0)
                std::snprintf(text, sizeof(text), "Equip, swapping with worn");
            else
                std::snprintf(text, sizeof(text), "Equip");
            add(ui::Button::triangle, text);
        }
        else if (def.category == Category::consumable)
        {
            add(ui::Button::triangle, "Use one");
        }
        if (!focus_.worn && def.max_stack > 1 && stack(item).count >= 2)
            add(ui::Button::square, "Split the stack in half");
        return count;
    }

    // One tip's content, drawn at an opacity and a slide so two can cross-fade.
    void draw_tip(gfx::DrawList &list, const Tip &tip, const Rect &card, float alpha,
                  float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const float x = card.x + 28 + slide;
        const float w = card.w - 56;
        float y = card.y + 28;
        char text[96];
        list.push_opacity(alpha);
        well(list, {x, y, 76, 76});
        if (tip.def < 0)
        {
            if (tip.wear >= 0)
                draw_ghost(list, tip.wear, x + 38, y + 38, 46);
            label(list, tip.wear >= 0 ? "EQUIPMENT SLOT" : "BAG SLOT", x + 96, y + 20,
                  kInk.with_alpha(0.55f));
            ui::text(list, fonts.regular, "Empty", x + 96, y + 48, 22, kInk.with_alpha(0.7f));
            ui::text(list, fonts.display, tip.wear >= 0 ? kWearNames[tip.wear] : "Empty slot", x,
                     y + 122, 30, kInk.with_alpha(0.85f));
            ui::paragraph(list, fonts.regular,
                          tip.wear >= 0 ? "Nothing is worn here. Carry a fitting piece over and "
                                          "set it down to equip it."
                                        : "Room for one more thing. Carry an item here and set it "
                                          "down.",
                          x, y + 170, 22, w, 30, kInk.with_alpha(0.5f), 4);
            list.pop_opacity();
            return;
        }

        const ItemDef &def = kItems[tip.def];
        const Color rarity = rarity_color(def.rarity);
        const bool consumable = def.category == Category::consumable;
        const Rect tile{x + 3, y + 3, 70, 70};
        list.rounded_rect(tile, 10, gfx::mix(Color::rgb(0x1b120b), rarity, 0.12f));
        list.gradient_rect(tile, 10, rarity.with_alpha(0.0f), rarity.with_alpha(0.3f));
        list.bordered_rect(tile, 10, kClear, 2, rarity.with_alpha(0.9f));
        draw_icon(list, def, x + 38, y + 38, 50);

        // Rarity and kind beside the icon, then the name in the rarity colour.
        label(list, ui::upper(kRarityNames[static_cast<int>(def.rarity)]).c_str(), x + 96, y + 20,
              rarity);
        ui::text(list, fonts.regular, fonts.regular.font->fit(def.kind, 22, w - 96), x + 96, y + 48,
                 22, kInk.with_alpha(0.85f));
        ui::text(list, fonts.regular,
                 def.wear != Wear::none ? kWearNames[static_cast<int>(def.wear)]
                                        : kCategoryNames[static_cast<int>(def.category)],
                 x + 96, y + 74, 20, kInk.with_alpha(0.55f));
        y = ui::paragraph(list, fonts.display, def.name, x, y + 122, 30, w, 36, rarity, 2);
        y -= 12;
        list.rounded_rect({x, y, w, 1.5f}, 0, kInk.with_alpha(0.12f));
        y += 40;

        // Stats, each against the worn item of the same slot.
        const int other = rival(tip);
        const bool compare = !tip.worn && def.wear != Wear::none;
        Stat rows[4];
        int count = 0;
        for (const Stat &stat : def.stats)
        {
            if (stat.id != StatId::none)
                rows[count++] = stat;
        }
        if (other >= 0)
        {
            // What the worn piece has and this one lacks is a loss worth showing.
            for (const Stat &stat : kItems[other].stats)
            {
                if (stat.id != StatId::none && stat_value(def, stat.id) == 0 && count < 4)
                    rows[count++] = {stat.id, 0};
            }
        }
        for (int i = 0; i < count; ++i)
        {
            const Stat &stat = rows[i];
            ui::text(list, fonts.regular, stat_name(stat.id), x, y, 24, kInk.with_alpha(0.72f));
            if (stat.value == 0 && compare)
                std::snprintf(text, sizeof(text), "\xE2\x80\x93");
            else
                format_stat(text, sizeof(text), stat, consumable);
            ui::text(list, fonts.semibold, text, x + w - (compare ? 92.0f : 0.0f), y, 24, kInk,
                     gfx::Align::right);
            if (compare)
            {
                const int difference =
                    stat.value - (other >= 0 ? stat_value(kItems[other], stat.id) : 0);
                if (difference != 0)
                {
                    // Up and green for better, down and red for worse.
                    const bool better = difference > 0;
                    const Color c = better ? kGood : kBad;
                    list.triangle({x + w - 76, y - 17, 16, 14}, c, 0.0f, better ? 0.0f : kPi);
                    std::snprintf(text, sizeof(text), "%d", std::abs(difference));
                    ui::text(list, fonts.semibold, text, x + w - 52, y, 22, c);
                }
                else
                {
                    list.rounded_rect({x + w - 76, y - 11, 16, 3}, 1.5f, kInk.with_alpha(0.35f));
                }
            }
            y += 36;
        }
        if (def.category == Category::quest)
        {
            ui::text(list, fonts.regular, "Cannot be used, split or equipped.", x, y, 22,
                     kAmber.with_alpha(0.9f));
            y += 36;
        }
        if (compare || tip.worn)
        {
            if (tip.worn)
                std::snprintf(text, sizeof(text), "Worn now");
            else if (other >= 0)
                std::snprintf(text, sizeof(text), "Against %s", kItems[other].name);
            else
                std::snprintf(text, sizeof(text), "Nothing worn in this slot yet");
            ui::text(list, fonts.regular, fonts.regular.font->fit(text, 20, w), x, y - 4, 20,
                     kInk.with_alpha(0.5f));
            y += 30;
        }

        // Weight: of the stack, with the sum spelled out.
        y += 2;
        ui::text(list, fonts.regular, "Weight", x, y, 24, kInk.with_alpha(0.72f));
        const float weight = def.weight * static_cast<float>(tip.count);
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(weight));
        const float number =
            ui::text(list, fonts.semibold, text, x + w, y, 24, kInk, gfx::Align::right);
        if (tip.count > 1)
        {
            std::snprintf(text, sizeof(text), "%d \xC3\x97 %.2g", tip.count,
                          static_cast<double>(def.weight));
            ui::text(list, fonts.regular, text, x + w - number - 14, y, 20, kInk.with_alpha(0.5f),
                     gfx::Align::right);
        }
        y += 20;
        list.rounded_rect({x, y, w, 1.5f}, 0, kInk.with_alpha(0.12f));
        y += 34;
        // No italic face exists: the flavour line is the regular one, quieter.
        ui::paragraph(list, fonts.regular, def.flavour, x, y, 22, w, 30, kInk.with_alpha(0.56f), 3);
        list.pop_opacity();
    }

    void draw_tooltip(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 5, 0.06f, 0.55f);
        const bool calm = context_.settings.reduced_motion;
        const Rect card{kTipX + (calm ? 0.0f : 40.0f * (1.0f - in)), kTop, kTipW, kBottom - kTop};
        const Color accent = tip_color_.value();
        list.push_opacity(in);
        list.shadow({card.x, card.y + 16, card.w, card.h}, kPanelRadius, 44,
                    Color::rgb(0x000000, 0.5f));
        // Frosted glass: the blurred firelight, a leather tint, a light edge
        // and a wash of the item's rarity colour from the top.
        list.glass(glass, card, kPanelRadius, Color::rgb(0xffffff));
        list.rounded_rect(card, kPanelRadius, Color::rgb(0x241710, 0.66f));
        list.gradient_rect({card.x, card.y, card.w, 210}, kPanelRadius, accent.with_alpha(0.2f),
                           accent.with_alpha(0.0f));
        list.bordered_rect(card, kPanelRadius, kClear, 1.5f,
                           gfx::mix(kInk, accent, 0.5f).with_alpha(0.3f));

        list.push_clip(card);
        if (tip_timer_.running)
        {
            // The old item leaves quickly; the new one arrives a beat later.
            const float t = tip_timer_.progress();
            const float move = calm ? 0.0f : 1.0f;
            draw_tip(list, previous_, card, 1.0f - tween::smoothstep(t * 2.4f),
                     -24.0f * move * tween::cubic_in(tween::clamp01(t * 2.4f)));
            const float arriving = tween::clamp01((t - 0.25f) / 0.75f);
            draw_tip(list, shown_, card, tween::smoothstep(arriving),
                     30.0f * move * (1.0f - tween::quint_out(arriving)));
        }
        else
        {
            draw_tip(list, shown_, card, 1.0f, 0.0f);
        }
        list.pop_clip();

        // The actions sit at the foot of the card and always tell the truth
        // about this very moment, so they are not part of the cross-fade.
        Line lines[4];
        const int count = actions(lines);
        const ui::GlyphStyle style = glyph_style();
        const float x = card.x + 28;
        float y = card.y + card.h - 26 - 40.0f * static_cast<float>(count);
        if (count > 0)
            list.rounded_rect({x, y - 8, card.w - 56, 1.5f}, 0, kInk.with_alpha(0.12f));
        for (int i = 0; i < count; ++i)
        {
            const float cy = y + 22;
            ui::draw_button(list, fonts, style, lines[i].button, x, cy, 30);
            ui::text(list, lines[i].bad ? fonts.semibold : fonts.regular,
                     fonts.regular.font->fit(lines[i].text, 22, card.w - 56 - 44), x + 44, cy + 8,
                     22, lines[i].bad ? kBad : kInk.with_alpha(0.9f));
            y += 40;
        }
        list.pop_opacity();
    }

    // ---- the sort menu and the hints --------------------------------------

    void draw_menu(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float t = menu_.value;
        const float row = 60.0f;
        const Rect card{kBagX + kBagW - 340, kTop + 60 - 18.0f * (1.0f - t), 340,
                        64 + row * kSorts + 16};
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.shadow({card.x, card.y + 18, card.w, card.h}, kPanelRadius, 50,
                    Color::rgb(0x000000, 0.6f));
        list.glass(glass, card, kPanelRadius, Color::rgb(0xffffff));
        list.rounded_rect(card, kPanelRadius, Color::rgb(0x2a1b11, 0.7f));
        list.bordered_rect(card, kPanelRadius, kClear, 1.5f, kInk.with_alpha(0.24f));
        label(list, "SORT THE BAG", card.x + 28, card.y + 42, kAmber);

        // One highlight that springs between the rows.
        const float nudge = ui::shake(nudge_.value, clock_, 6.0f, 9.0f) * nudge_y_;
        const Rect bar{card.x + 12, card.y + 64 + menu_position_.value * row + nudge, card.w - 24,
                       row - 8};
        list.glow(bar, 18, 14, kAmber.with_alpha(0.25f));
        list.rounded_rect(bar, 18, kInk);
        for (int i = 0; i < kSorts; ++i)
        {
            const bool focused = i == sort_;
            const float appear = tween::stagger(t, i, 0.1f, 0.6f);
            ui::text(list, fonts.semibold, kSortNames[i], card.x + 36,
                     card.y + 64 + row * static_cast<float>(i) + 35 + 10.0f * (1.0f - appear), 25,
                     focused ? Color::rgb(0x1d120b) : kInk.with_alpha(0.88f * appear));
        }
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = glyph_style();
        const float menu = menu_.value;
        if (menu > 0.5f)
        {
            list.push_opacity((menu - 0.5f) * 2.0f);
            const ui::Hint hints[] = {{ui::Button::cross, "Sort"}, {ui::Button::circle, "Close"}};
            ui::draw_hints(list, fonts, style, hints, 2, 1824, true);
            list.pop_opacity();
            return;
        }
        list.push_opacity(tween::stagger(age_, 8, 0.06f, 0.5f) * (1.0f - menu * 2.0f));
        ui::Hint hints[5];
        int count = 0;
        if (held_ >= 0)
        {
            const Verdict what = verdict();
            hints[count++] = {ui::Button::cross, is_refusal(what)         ? "Not here"
                                                 : what == Verdict::equip ? "Equip"
                                                 : what == Verdict::swap  ? "Swap"
                                                 : what == Verdict::merge ? "Stack"
                                                                          : "Set down"};
            hints[count++] = {ui::Button::circle, "Put back"};
            hints[count++] = {ui::Button::l2, "Category", ui::Button::r2};
        }
        else
        {
            const int item = stack_at(focus_);
            if (item >= 0)
            {
                const ItemDef &def = def_of(stack(item));
                hints[count++] = {ui::Button::cross, "Lift"};
                if (focus_.worn)
                    hints[count++] = {ui::Button::triangle, "Take off"};
                else if (def.wear != Wear::none)
                    hints[count++] = {ui::Button::triangle, "Equip"};
                else if (def.category == Category::consumable)
                    hints[count++] = {ui::Button::triangle, "Use"};
                if (!focus_.worn && def.max_stack > 1 && stack(item).count >= 2)
                    hints[count++] = {ui::Button::square, "Split"};
            }
            hints[count++] = {ui::Button::l2, "Category", ui::Button::r2};
            hints[count++] = {ui::Button::options, "Sort"};
        }
        ui::draw_hints(list, fonts, style, hints, count, 1824, true);
        list.pop_opacity();
    }

    app::Context &context_;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion

    // The model: item objects, and which of them each place holds.
    std::array<Stack, kPool> stacks_{};
    std::array<int, kCells> bag_{};      // bag slot -> item, -1 empty
    std::array<int, kWearSlots> worn_{}; // equipment slot -> item
    std::array<int, kCells> view_{};     // shown cell -> bag slot
    std::array<int, kCells> cell_of_{};  // bag slot -> shown cell
    int held_ = -1;                      // the carried item
    Place origin_;                       // where it was lifted from (a bag slot, not a cell)
    Place focus_;
    int tab_ = 0;
    int sort_ = 0;
    bool menu_open_ = false;

    // Animation state.
    ui::SpringRect ring_;
    ui::SpringColor ring_color_;
    ui::SpringRect tab_ring_;
    std::array<Rect, kTabs> tab_rects_{};
    ui::Pulse tab_nudge_;
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    ui::Pulse deny_;
    Rect deny_rect_;
    ui::Pulse figure_flash_;
    Color flash_color_ = kAmber;
    tween::Spring carry_; // 1 while something is in the hand
    tween::Spring attack_, defence_, weight_;
    Tip shown_;
    Tip previous_;
    tween::Timer tip_timer_;
    ui::SpringColor tip_color_;
    tween::Spring menu_;
    tween::Spring menu_position_;
    std::array<Puff, 7> puffs_{};
    char float_[32] = "";
    float float_x_ = 0.0f;
    float float_y_ = 0.0f;
    tween::Timer float_timer_;
    char notice_[96] = "";
    bool notice_bad_ = false;
    float notice_age_ = 10.0f;
};

} // namespace

std::unique_ptr<app::Concept> make_inventory(app::Context &context)
{
    return std::make_unique<Inventory>(context);
}

} // namespace hui::concepts
