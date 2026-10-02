// ps5-homebrew-ui - Design "Storefront": browse, inspect and buy titles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A digital shop window on deep charcoal: a featured banner, filter chips and
// a grid of product cards, a full-screen product page, a cart drawer and a
// checkout. Nothing is sold: the prices are invented "credits" derived from
// each title's name. What makes it feel finished:
//
//   - commerce clarity: one function draws every price, so a discount (badge,
//     old price struck through, new price in the accent), "Free" and "Owned"
//     look the same on a card, in the banner, on the page and in the cart;
//   - the banner cross-fades every six seconds over a wide crop of the cover
//     that drifts slowly (its uv rectangle is animated, not its position), and
//     waits while it is focused or while anything is open over it;
//   - Cross opens the product page with a shared element: the card's cover
//     grows into the page's preview, its crop opening up to the whole cover,
//     while the rest of the page fades in around it (and back on Circle);
//   - "Add to cart" throws a small copy of the cover along an arc into the
//     cart counter, which bumps and rolls to its new number when it lands;
//   - the cart is a frosted drawer: rows collapse with a spring when removed,
//     the subtotal counts to its new value in the mono face, and the checkout
//     asks first, with the focus on Cancel;
//   - one focus highlight glides between banner, chips and cards, the grid
//     scrolls as a page with the chips sticking under the top bar, and every
//     edge answers with a soft refusal.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kWhite = Color::rgb(0xffffff);
const Color kBlack = Color::rgb(0x000000);
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kInk = Color::rgb(0xf4efe6);      // warm white: all text
const Color kAccent = Color::rgb(0xff9f43);   // prices that changed, calls to action
const Color kOnAccent = Color::rgb(0x201407); // text on the accent
const Color kCoal = Color::rgb(0x0e0f12);     // backdrop, top
const Color kCoalLow = Color::rgb(0x17181d);  // backdrop, bottom
const Color kPanel = Color::rgb(0x202228);    // plates and panels
const Color kOwned = Color::rgb(0x8fdab2);    // "this is yours"
const Color kStar = Color::rgb(0xffcf70);

constexpr float kWidth = gfx::kVirtualWidth;
constexpr float kHeight = gfx::kVirtualHeight;
constexpr float kMargin = 96.0f;
constexpr float kRight = kWidth - kMargin;

// The home page scrolls as one sheet under the top bar.
constexpr float kPageTop = 104.0f;
constexpr float kBannerY = 116.0f;
constexpr float kBannerH = 412.0f;
constexpr float kBannerArtW = 820.0f; // the artwork fills the banner's right side
constexpr float kBannerRadius = 28.0f;
constexpr float kBannerSeconds = 6.0f;
constexpr int kFeatured = 5;
constexpr float kChipsY = 552.0f;
constexpr float kChipH = 48.0f;
constexpr float kStickY = 116.0f; // where the chips stop when the banner scrolls away
constexpr float kGridY = 628.0f;
constexpr int kColumns = 5;
constexpr float kCardGap = 24.0f;
constexpr float kCardW = (kWidth - 2.0f * kMargin - (kColumns - 1) * kCardGap) / kColumns;
constexpr float kCoverH = 184.0f;
constexpr float kCardH = 272.0f;
constexpr float kCardRadius = 14.0f;
constexpr float kRowPitch = 300.0f;
constexpr float kLift = 0.05f;                        // how much the focused card grows
constexpr float kViewBottom = 968.0f;                 // the grid ends above the hint row
constexpr float kFadeFoot = 72.0f;                    // ... and fades out over this distance
constexpr float kFadeHead = 24.0f;                    // the fade under the chips
constexpr float kStuckTop = kStickY + kChipH + 28.0f; // top of the focused row when scrolled

// The product page.
constexpr Rect kPreview{96.0f, 128.0f, 600.0f, 600.0f};
constexpr float kPreviewRadius = 28.0f;
constexpr int kShots = 3;
constexpr float kThumbY = 752.0f;
constexpr float kThumbH = 112.0f;
constexpr float kThumbGap = 16.0f;
constexpr float kThumbW = (600.0f - 2.0f * kThumbGap) / 3.0f;
constexpr float kInfoX = 768.0f;
constexpr float kInfoW = 520.0f; // the text column beside the buy box
constexpr Rect kBuyBox{1336.0f, 392.0f, 488.0f, 472.0f};
constexpr Rect kBuyButton{1368.0f, 600.0f, 424.0f, 72.0f};
constexpr Rect kWishButton{1368.0f, 688.0f, 424.0f, 64.0f};
constexpr float kButtonRadius = 18.0f;

// The cart: its counter in the top bar and the drawer.
constexpr float kCartX = 1776.0f;
constexpr float kCartY = 78.0f;
constexpr float kDrawerW = 640.0f;
constexpr float kLinesTop = 200.0f;
constexpr float kLineH = 100.0f;
constexpr float kLinesView = 600.0f;
constexpr float kFlySeconds = 0.62f;

constexpr Rect kDialog{600.0f, 340.0f, 720.0f, 400.0f};
constexpr float kToastSeconds = 3.6f;

// The part of a square cover a card shows: a band above the printed title.
// Covers are canvases (bottom row first), so v runs upward: see crop().
constexpr float kCardCropY = 0.129f;

constexpr const char *kTechniques[] = {
    "Shared-element transition: the card's cover and its uv crop grow into the product page",
    "Featured banner: cross-fade plus a slow uv-rectangle drift, paused while focused",
    "Add to cart: an arc flight into the counter, which bumps and rolls its number",
    "Frosted cart drawer: spring-collapsed rows, a counting subtotal, cancel-first checkout",
    "One price routine everywhere: badge, struck-through old price, Free and Owned",
    "A page that scrolls as one sheet with sticky filter chips and per-pixel edge fades",
    "Cards drawn layer by layer (covers, shapes, each font) to keep the draw calls low",
};

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::right},
    {0.5f, action_bit(Action::confirm)},
    {1.3f, action_bit(Action::confirm), Direction::none, "product"},
    {0.28f, 0, Direction::none, "flight"},
    {1.1f, action_bit(Action::back)},
    {0.7f, 0, Direction::right},
    {0.4f, action_bit(Action::confirm)},
    {1.0f, action_bit(Action::confirm)},
    {1.0f, action_bit(Action::west)},
    {1.1f, action_bit(Action::confirm), Direction::none, "cart"},
    {0.9f, 0, Direction::right, "confirm"},
    {0.4f, action_bit(Action::confirm)},
    {0.7f, action_bit(Action::back)},
    {0.4f, action_bit(Action::back)},
    {0.5f, 0, Direction::down},
    {0.9f, 0, Direction::none, "purchased"},
    {1.2f, 0, Direction::up},
};

enum Filter : int
{
    kFilterAll,
    kFilterSale,
    kFilterNew,
    kFilterFree,
    kFilterOwned,
    kFilters,
};

constexpr const char *kFilterNames[kFilters] = {"All", "On sale", "New", "Free", "Owned"};

// What one pass over a group of cards records. A card alternates between its
// cover texture, plain shapes and several font atlases; drawn card by card,
// every switch would start a new draw call. The grid and the cart therefore
// draw all covers, then all shapes, then each face: the same pictures in a
// third of the draw calls.
enum Layer : unsigned
{
    kImages = 1,
    kShapes = 2,
    kSemibold = 4,
    kRegular = 8,
    kMono = 16,
    kAllLayers = 31,
};

enum class Zone : std::uint8_t
{
    banner,
    chips,
    grid,
};

enum class PageFocus : std::uint8_t
{
    buy,
    wishlist,
    shots,
};

// One line per catalogue entry, in its order: what the banner says under the
// title. A banner has room for a sentence fragment, not for the whole blurb.
constexpr const char *kPitches[] = {
    "The city rebuilds itself every lap.",
    "Chart a drowned coast, one tide at a time.",
    "Bend light. Grow impossible trees.",
    "One stolen ferry. One very long night.",
    "Fold, glide and unfold across unsent letters.",
    "The radio was switched off years ago.",
    "Small parcels, large distances.",
    "Two smiths, one forge, one prophecy.",
    "A valley of patient robots awaits orders.",
    "No map, no engine. Read the water.",
    "Keep one flower alive all winter.",
    "Four runners. Never drop the beat.",
    "A mountain, a volcano and a clock.",
    "Roads that hum when you find the line.",
    "You are the moth. The lantern has opinions.",
    "Spin a tiny moon just fast enough for rain.",
    "Late-night calls from a town under water.",
    "Every door is a little bit alive.",
    "Carry the last candle across a dark continent.",
    "Pottery for very specific dreams.",
    "One staircase, rebuilt every climb.",
    "Twenty minutes of perfect light.",
    "Light the right beacon at the right moment.",
    "Learn the forest floor by name.",
};
constexpr int kPitchCount = static_cast<int>(sizeof(kPitches) / sizeof(kPitches[0]));

// What the shop asks for a title. Everything is derived from the title's
// name, so the same title costs the same on every run and on every screen.
struct Offer
{
    int base = 0;    // list price in credits; 0 is a free title
    int percent = 0; // discount, 0 when there is none
    bool starts_owned = false;
    int ratings = 0; // how many players rated it
    int size_gb = 0;

    bool free() const
    {
        return base == 0;
    }
    // What is charged now: the discount applied, rounded down to ten credits.
    int price() const
    {
        return base * (100 - percent) / 100 / 10 * 10;
    }
};

Offer make_offer(const char *title)
{
    // FNV-1a over the title; the salt picks a pleasant spread for the sample
    // catalogue (a few free and owned titles, about a quarter on sale).
    std::uint32_t hash = 2166136261u ^ 2906u;
    for (const char *c = title; *c != '\0'; ++c)
    {
        hash ^= static_cast<unsigned char>(*c);
        hash *= 16777619u;
    }
    constexpr int kBase[] = {1490, 1990, 2490, 2990, 3490, 3990, 4990, 5990};
    constexpr int kPercent[] = {20, 30, 40, 50};
    Offer offer;
    const bool free = hash % 8 == 0;
    offer.base = free ? 0 : kBase[(hash >> 13) % 8];
    offer.starts_owned = !free && (hash >> 3) % 6 == 0;
    offer.percent = !free && (hash >> 7) % 3 == 0 ? kPercent[(hash >> 11) % 4] : 0;
    offer.ratings = 180 + static_cast<int>((hash >> 5) % 4200);
    offer.size_gb = 4 + static_cast<int>((hash >> 17) % 44);
    return offer;
}

// "2,490": credits with a thousands separator.
void format_credits(char *out, std::size_t size, int amount)
{
    if (amount >= 1000)
        std::snprintf(out, size, "%d,%03d", amount / 1000, amount % 1000);
    else
        std::snprintf(out, size, "%d", amount);
}

// A cover's uv rectangle from a top-left based crop in 0..1.
constexpr Rect crop(float x, float y, float w, float h)
{
    return {x, 1.0f - y, w, -h};
}

constexpr Rect kCoverUv = crop(0.0f, 0.0f, 1.0f, 1.0f);
constexpr Rect kCardCrop = crop(0.0f, kCardCropY, 1.0f, kCoverH / kCardW);

Rect mix_rect(const Rect &a, const Rect &b, float t)
{
    return {tween::lerp(a.x, b.x, t), tween::lerp(a.y, b.y, t), tween::lerp(a.w, b.w, t),
            tween::lerp(a.h, b.h, t)};
}

Rect scaled(const Rect &r, float scale)
{
    return {r.cx() - r.w * scale * 0.5f, r.cy() - r.h * scale * 0.5f, r.w * scale, r.h * scale};
}

// Baseline that centres a line of the given size on cy.
float centred(float cy, float size)
{
    return cy + size * 0.35f;
}

void draw_check(gfx::DrawList &list, float cx, float cy, float size, Color colour)
{
    const float stroke = std::max(2.0f, size * 0.16f);
    list.line(cx - size * 0.42f, cy + size * 0.02f, cx - size * 0.12f, cy + size * 0.32f, stroke,
              colour);
    list.line(cx - size * 0.12f, cy + size * 0.32f, cx + size * 0.44f, cy - size * 0.3f, stroke,
              colour);
}

// The mark of the invented currency: a ring with a small diamond in it.
void draw_coin(gfx::DrawList &list, float cx, float cy, float radius, Color colour)
{
    list.ring(cx, cy, radius, std::max(1.5f, radius * 0.24f), colour);
    const float d = radius * 0.62f;
    list.rotated_rect({cx - d * 0.5f, cy - d * 0.5f, d, d}, 1.0f, 0.7854f, colour);
}

// A shopping cart from five strokes and two wheels: handle, basket, rim.
void draw_cart(gfx::DrawList &list, float cx, float cy, float size, Color colour)
{
    const float stroke = std::max(2.0f, size * 0.095f);
    const float s = size;
    list.line(cx - 0.54f * s, cy - 0.38f * s, cx - 0.36f * s, cy - 0.38f * s, stroke, colour);
    list.line(cx - 0.36f * s, cy - 0.38f * s, cx - 0.2f * s, cy + 0.14f * s, stroke, colour);
    list.line(cx - 0.2f * s, cy + 0.14f * s, cx + 0.36f * s, cy + 0.14f * s, stroke, colour);
    list.line(cx + 0.36f * s, cy + 0.14f * s, cx + 0.48f * s, cy - 0.2f * s, stroke, colour);
    list.line(cx + 0.48f * s, cy - 0.2f * s, cx - 0.3f * s, cy - 0.2f * s, stroke, colour);
    list.circle(cx - 0.12f * s, cy + 0.38f * s, 0.085f * s, colour);
    list.circle(cx + 0.28f * s, cy + 0.38f * s, 0.085f * s, colour);
}

class Store final : public app::Concept
{
  public:
    explicit Store(app::Context &context) : context_(context)
    {
        const auto &items = context.catalog.items();
        const int count = static_cast<int>(items.size());
        for (int i = 0; i < count; ++i)
        {
            const demo::Item &it = items[static_cast<std::size_t>(i)];
            const Offer offer = make_offer(it.title);
            offers_.push_back(offer);
            owned_.push_back(offer.starts_owned);
            newest_year_ = std::max(newest_year_, it.year);
            // Wrapping and fitting allocate: do it once, not per frame.
            card_titles_.push_back(context.fonts.semibold.font->fit(it.title, 24, kCardW - 24.0f));
            line_titles_.push_back(
                context.fonts.semibold.font->fit(it.title, 24, kDrawerW - 334.0f));
            Copy copy;
            const gfx::Font &lead = *context.fonts.semibold.font;
            copy.pitch_lines =
                std::min(2, static_cast<int>(lead.wrap(pitch(i), 30, kInfoW).size()));
            // A lead line that wraps is narrowed until its two lines are
            // about even: no single word left alone on the second one.
            while (copy.pitch_lines == 2 && copy.pitch_width > 260.0f &&
                   lead.wrap(pitch(i), 30, copy.pitch_width - 20.0f).size() == 2)
                copy.pitch_width -= 20.0f;
            copy.blurb_lines = std::min(
                4, static_cast<int>(context.fonts.regular.font->wrap(it.blurb, 26, kInfoW).size()));
            copy_.push_back(copy);
        }
        wished_.assign(static_cast<std::size_t>(count), false);
        lift_.resize(static_cast<std::size_t>(count));

        // The banner features the best-rated titles the player does not own,
        // the deepest discounts first.
        std::vector<int> order;
        for (int i = 0; i < count; ++i)
        {
            if (!offers_[static_cast<std::size_t>(i)].starts_owned)
                order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(),
                         [&](int a, int b) {
                             return items[static_cast<std::size_t>(a)].rating >
                                    items[static_cast<std::size_t>(b)].rating;
                         });
        if (order.size() > static_cast<std::size_t>(kFeatured))
            order.resize(static_cast<std::size_t>(kFeatured));
        std::stable_sort(order.begin(), order.end(),
                         [&](int a, int b)
                         {
                             return offers_[static_cast<std::size_t>(a)].percent >
                                    offers_[static_cast<std::size_t>(b)].percent;
                         });
        if (order.empty())
            order.push_back(0);
        featured_ = order;

        rebuild_visible();
        apply_palette(true);
        ring_.snap(focus_target());
        ring_radius_.snap(focus_radius());
        plate_.snap(1.0f);
        glow_.snap(visible_.empty() ? kAccent : item(visible_.front()).accent);
        Rect chips[kFilters];
        chip_layout(chips);
        chip_pill_.snap(chips[filter_]);
        page_ring_.snap(kBuyButton.inset(-6.0f));
        page_ring_radius_.snap(kButtonRadius + 6.0f);
        drawer_ring_.snap(checkout_rect());
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "store",
            "Storefront",
            "A shop window: featured banner, product pages, a cart and a checkout",
            "src/concepts/store.cpp",
            audio::SoundSet::glass,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        dialog_open_ = false;
        drawer_open_ = false;
        page_open_ = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        // The topmost layer takes the input; the layers under it only animate.
        if (dialog_open_)
            update_dialog(input, feedback);
        else if (drawer_open_)
            update_drawer(input, feedback);
        else if (page_open_)
            update_page(input, feedback);
        else
            update_home(input, feedback);
        animate(dt, feedback);
    }

    void draw(app::Frame &frame) const override
    {
        // The backdrop stays charcoal; only its undertone and one soft light
        // behind the banner take the featured title's colours.
        const float away = tween::clamp01(scroll_.value / (kChipsY - kStickY));
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = palette_[0].value();
        frame.backdrop.colors[1] = palette_[1].value();
        frame.backdrop.colors[2] = palette_[2].value();
        frame.backdrop.params[0] = 0.72f;
        frame.backdrop.params[1] = 0.3f - 0.25f * away;
        frame.backdrop.params[2] = 0.26f - 0.12f * away;
        frame.backdrop.time = clock_;

        const float page = page_.value;
        const float drawer = drawer_.value;
        const float dialog = dialog_.value;

        // Anything opened over the home page pushes it back a little.
        gfx::DrawList &scene = frame.scene;
        scene.push_transform(1.0f - 0.03f * std::max(page, 0.6f * drawer) * travel(), 960, 540, 0,
                             0);
        draw_top_bar(scene);
        scene.push_clip({0, kPageTop, kWidth, kHeight - kPageTop});
        draw_banner(scene);
        draw_grid(scene);
        draw_chips(scene);
        scene.pop_clip();
        scene.pop_transform();
        // Each layer has its own hint row in the same corner. A row leaves
        // during the first third of the next layer's arrival and that layer's
        // row comes in afterwards, so two rows are never legible at once.
        const auto leaving = [](float above) { return 1.0f - tween::clamp01(above * 3.0f); };
        const auto arriving = [](float value) { return tween::clamp01((value - 0.4f) / 0.6f); };
        draw_hints(scene, 0,
                   tween::stagger(age_, 10, 0.07f, 0.5f) * leaving(std::max(page, drawer)));

        gfx::DrawList &overlay = frame.overlay;
        frame.glass = page > 0.004f || drawer > 0.004f || dialog > 0.004f || toast_.value > 0.01f;
        draw_page(overlay, frame.glass_texture);
        draw_drawer(overlay, frame.glass_texture);
        // The counter lives above the page and the drawer: a flight must be
        // able to land in it from either.
        draw_cart_counter(overlay);
        draw_dialog(overlay, frame.glass_texture);
        draw_flight(overlay);
        draw_toast(overlay, frame.glass_texture);
        draw_hints(overlay, 1, arriving(page) * leaving(drawer));
        draw_hints(overlay, 2, arriving(drawer) * leaving(dialog));
        draw_hints(overlay, 3, arriving(dialog));
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // A cart row. It stays in the list while it leaves, so the rows below it
    // can close the gap smoothly.
    struct Line
    {
        int item = 0;
        tween::Spring open; // 1 at rest; the row's height and opacity
        bool leaving = false;
        bool sold = false;  // leaving because it was bought: slides out too
        float delay = 0.0f; // seconds before it starts to leave
    };

    // Line counts of a title's page copy, measured once.
    struct Copy
    {
        int pitch_lines = 1;
        int blurb_lines = 1;
        float pitch_width = kInfoW;
    };

    // ---- data --------------------------------------------------------------

    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(index)];
    }
    // The short line that sells a title; the blurb stands in for titles the
    // table above does not know.
    const char *pitch(int index) const
    {
        return index < kPitchCount ? kPitches[index] : item(index).blurb;
    }
    const Offer &offer(int index) const
    {
        return offers_[static_cast<std::size_t>(index)];
    }
    bool owned(int index) const
    {
        return owned_[static_cast<std::size_t>(index)];
    }
    bool calm() const
    {
        return context_.settings.reduced_motion;
    }
    // Spring speed: plain, near-instant fades under "Reduce motion".
    float omega(float normal) const
    {
        return calm() ? 40.0f : normal;
    }
    // Scales every decorative slide: nothing travels under "Reduce motion".
    float travel() const
    {
        return calm() ? 0.0f : 1.0f;
    }
    // The idle pulse of whatever has the focus; a steady light when calm.
    float breathe() const
    {
        return calm() ? 0.5f : ui::breathe(clock_);
    }

    bool matches(int filter, int index) const
    {
        const Offer &o = offer(index);
        switch (filter)
        {
        case kFilterSale:
            return o.percent > 0 && !owned(index);
        case kFilterNew:
            return item(index).year == newest_year_;
        case kFilterFree:
            return o.free() && !owned(index);
        case kFilterOwned:
            return owned(index);
        default:
            return true;
        }
    }

    void rebuild_visible()
    {
        visible_.clear();
        for (int &count : counts_)
            count = 0;
        for (int i = 0; i < static_cast<int>(offers_.size()); ++i)
        {
            for (int f = 0; f < kFilters; ++f)
                counts_[f] += matches(f, i) ? 1 : 0;
            if (matches(filter_, i))
                visible_.push_back(i);
        }
        const int count = static_cast<int>(visible_.size());
        focus_ = std::clamp(focus_, 0, std::max(0, count - 1));
        if (count == 0 && zone_ == Zone::grid)
            zone_ = Zone::chips;
    }

    int live_lines() const
    {
        int count = 0;
        for (const Line &line : lines_)
            count += line.leaving ? 0 : 1;
        return count;
    }
    // Index into lines_ of the nth row that is not leaving, or -1.
    int live_line(int nth) const
    {
        for (int i = 0; i < static_cast<int>(lines_.size()); ++i)
        {
            if (!lines_[static_cast<std::size_t>(i)].leaving && nth-- == 0)
                return i;
        }
        return -1;
    }
    bool in_cart(int index) const
    {
        for (const Line &line : lines_)
        {
            if (!line.leaving && line.item == index)
                return true;
        }
        return false;
    }
    int subtotal() const
    {
        int sum = 0;
        for (const Line &line : lines_)
            sum += line.leaving ? 0 : offer(line.item).price();
        return sum;
    }
    // The subtotal as the drawer shows it: on its way to the real one.
    int subtotal_shown() const
    {
        const float t = tween::cubic_out(subtotal_count_.progress());
        return static_cast<int>(std::lround(
            tween::lerp(static_cast<float>(subtotal_from_), static_cast<float>(subtotal_to_), t)));
    }

    // ---- geometry ----------------------------------------------------------

    static Rect banner_rect()
    {
        return {kMargin, kBannerY, kWidth - 2.0f * kMargin, kBannerH};
    }
    static Rect banner_art()
    {
        const Rect b = banner_rect();
        return {b.x + b.w - kBannerArtW, b.y, kBannerArtW, b.h};
    }
    // A card's place on the unscrolled page.
    static Rect card_rect(int index)
    {
        return {kMargin + static_cast<float>(index % kColumns) * (kCardW + kCardGap),
                kGridY + static_cast<float>(index / kColumns) * kRowPitch, kCardW, kCardH};
    }
    // The chips scroll with the page until they reach the top bar, then stay.
    float chips_y() const
    {
        return std::max(kChipsY - scroll_.value, kStickY);
    }
    // Chip rectangles on screen, left to right; widths follow their labels.
    void chip_layout(Rect out[kFilters]) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char number[8];
        float x = kMargin + ui::button_width(ui::Button::l2, 30) + 16.0f;
        for (int i = 0; i < kFilters; ++i)
        {
            std::snprintf(number, sizeof(number), "%d", counts_[i]);
            const float w = 48.0f + fonts.semibold.measure(kFilterNames[i], 22) + 10.0f +
                            fonts.mono.measure(number, 18);
            out[i] = {x, chips_y(), w, kChipH};
            x += w + 12.0f;
        }
    }
    static Rect thumb_rect(int shot)
    {
        return {kPreview.x + static_cast<float>(shot) * (kThumbW + kThumbGap), kThumbY, kThumbW,
                kThumbH};
    }
    Rect checkout_rect() const
    {
        return {kWidth - kDrawerW + 44.0f, 904.0f, kDrawerW - 88.0f, 68.0f};
    }
    // Top of a cart row inside the list (before scrolling): the rows above
    // it, each as tall as it is open.
    float line_offset(int index) const
    {
        float y = 0.0f;
        for (int i = 0; i < index && i < static_cast<int>(lines_.size()); ++i)
            y += lines_[static_cast<std::size_t>(i)].open.value * kLineH;
        return y;
    }

    // What the home page's focus highlight surrounds, in page coordinates
    // (the scroll offset is taken off when it is drawn, so the highlight
    // rides with the page instead of chasing it).
    Rect focus_target() const
    {
        if (zone_ == Zone::banner)
            return banner_rect().inset(-7.0f);
        if (zone_ == Zone::chips || visible_.empty())
        {
            Rect chips[kFilters];
            chip_layout(chips);
            Rect chip = chips[filter_].inset(-6.0f);
            chip.y += scroll_.value;
            return chip;
        }
        return card_rect(focus_).inset(-14.0f);
    }
    float focus_radius() const
    {
        if (zone_ == Zone::banner)
            return kBannerRadius + 7.0f;
        if (zone_ == Zone::chips || visible_.empty())
            return kChipH * 0.5f + 6.0f;
        return 24.0f;
    }

    // Where the page scroll should rest for the current focus.
    float scroll_target() const
    {
        if (zone_ != Zone::grid || visible_.empty() || focus_ < kColumns)
            return 0.0f;
        // Any row but the first sends the banner away and parks the chips
        // under the top bar; from there the page moves only as far as needed
        // to keep the focused row whole and clear of the fade at the bottom.
        const float top = card_rect(focus_).y;
        const float lowest = std::max(kChipsY - kStickY, top + kCardH + kFadeFoot - kViewBottom);
        const float highest = top - kStuckTop;
        return std::clamp(scroll_.target, lowest, std::max(lowest, highest));
    }

    // The banner artwork's crop of the square cover. It widens and slides a
    // little over the banner's life: the picture moves inside a still frame.
    Rect banner_uv(int slot, float seconds) const
    {
        float t = calm() ? 0.5f : tween::clamp01(seconds / 9.0f);
        if (slot % 2 == 1)
            t = 1.0f - t; // every other banner drifts the opposite way
        const float w = 1.0f / (1.05f + 0.11f * t);
        const float h = w * kBannerH / kBannerArtW;
        const float x = (1.0f - w) * (0.25f + 0.5f * t);
        // Vertically it stays on the band between the studio's name and the
        // printed title: the banner sets the title itself.
        const float y = 0.125f + (0.54f - h) * (0.2f + 0.6f * t);
        return crop(x, y, w, h);
    }

    // The three "screenshots" of a title: the cover and two crops of it.
    static Rect shot_uv(int shot, bool thumb)
    {
        Rect r{0.0f, 0.0f, 1.0f, 1.0f};
        if (shot == 1)
            r = {0.18f, 0.12f, 0.56f, 0.56f};
        else if (shot == 2)
            r = {0.44f, 0.1f, 0.5f, 0.5f};
        if (thumb)
        {
            // A thumbnail is wider than tall: keep the width, take a band.
            const float h = r.w * kThumbH / kThumbW;
            r.y += (r.h - h) * (shot == 0 ? 0.32f : 0.5f);
            r.h = h;
        }
        return crop(r.x, r.y, r.w, r.h);
    }
    Color shot_tint(int shot) const
    {
        return shot == 2 ? gfx::mix(kWhite, item(page_item_).accent, 0.5f) : kWhite;
    }

    // Where the product page's cover starts its journey: the card it was
    // opened on, or the banner artwork. False when neither is on screen.
    bool page_source(Rect *rect, Rect *uv, float *radius) const
    {
        if (page_from_banner_)
        {
            *rect = banner_art();
            rect->y -= scroll_.value;
            *uv = banner_uv(banner_, drift_);
            *radius = kBannerRadius;
            return true;
        }
        for (int k = 0; k < static_cast<int>(visible_.size()); ++k)
        {
            if (visible_[static_cast<std::size_t>(k)] != page_item_)
                continue;
            Rect card = card_rect(k);
            card.y -= scroll_.value;
            const float grow = 1.0f + kLift * lift_[static_cast<std::size_t>(page_item_)].value;
            const Rect lifted = scaled(card, grow);
            *rect = {lifted.x, lifted.y, lifted.w, kCoverH * grow};
            *uv = kCardCrop;
            *radius = kCardRadius;
            return true;
        }
        return false;
    }

    // ---- input: the home page ----------------------------------------------

    // The edge of something: a quiet "no" (and nothing at all for a held
    // direction, which only means the player has not let go yet).
    void refuse(app::Feedback &feedback, bool repeat, float dx, float dy)
    {
        if (repeat)
            return;
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_x_ = dx;
        nudge_y_ = dy;
    }

    void tick_card(app::Feedback &feedback) const
    {
        // Rows further down sound a little lower.
        const float pitch = std::max(0.82f, 1.06f - 0.04f * static_cast<float>(focus_ / kColumns));
        feedback.play(audio::Cue::focus, pitch, ui::pan_for_x(card_rect(focus_).cx()));
    }

    void update_home(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::west))
        {
            open_drawer(feedback);
            return;
        }
        if (input.is_pressed(Action::jump_prev))
            step_filter(-1, false, feedback);
        else if (input.is_pressed(Action::jump_next))
            step_filter(1, false, feedback);

        if (input.nav != Direction::none)
        {
            if (zone_ == Zone::grid && !visible_.empty())
                navigate_grid(input, feedback);
            else if (zone_ == Zone::banner)
                navigate_banner(input, feedback);
            else
                navigate_chips(input, feedback);
        }

        if (input.is_pressed(Action::confirm))
        {
            if (zone_ == Zone::grid && !visible_.empty())
                open_page(visible_[static_cast<std::size_t>(focus_)], false, feedback);
            else if (zone_ == Zone::banner)
                open_page(featured_[static_cast<std::size_t>(banner_)], true, feedback);
            else if (!visible_.empty())
            {
                zone_ = Zone::grid;
                feedback.play(audio::Cue::select);
            }
            else
                refuse(feedback, false, 0.0f, 1.0f);
        }
        // Back has one step to take here: from deep in the grid to its top.
        if (input.is_pressed(Action::back) && zone_ == Zone::grid && focus_ >= kColumns)
        {
            focus_ %= kColumns;
            feedback.play(audio::Cue::back);
        }
    }

    void navigate_grid(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(visible_.size());
        const int column = focus_ % kColumns;
        const int row = focus_ / kColumns;
        switch (input.nav)
        {
        case Direction::left:
            if (column == 0)
                return refuse(feedback, input.nav_repeat, -1.0f, 0.0f);
            --focus_;
            break;
        case Direction::right:
            if (column == kColumns - 1 || focus_ + 1 >= count)
                return refuse(feedback, input.nav_repeat, 1.0f, 0.0f);
            ++focus_;
            break;
        case Direction::up:
            if (row == 0)
            {
                zone_ = Zone::chips;
                feedback.play(audio::Cue::focus, 1.1f, ui::pan_for_x(ring_.x.value));
                return;
            }
            focus_ -= kColumns;
            break;
        case Direction::down:
            if (focus_ + kColumns < count)
                focus_ += kColumns;
            else if ((count - 1) / kColumns > row)
                focus_ = count - 1; // a short last row: land on its last card
            else
                return refuse(feedback, input.nav_repeat, 0.0f, 1.0f);
            break;
        case Direction::none:
            return;
        }
        tick_card(feedback);
    }

    void navigate_chips(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::left:
            step_filter(-1, input.nav_repeat, feedback);
            break;
        case Direction::right:
            step_filter(1, input.nav_repeat, feedback);
            break;
        case Direction::up:
            zone_ = Zone::banner;
            feedback.play(audio::Cue::focus, 1.16f);
            break;
        case Direction::down:
            if (visible_.empty())
                return refuse(feedback, input.nav_repeat, 0.0f, 1.0f);
            zone_ = Zone::grid;
            tick_card(feedback);
            break;
        case Direction::none:
            break;
        }
    }

    void navigate_banner(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(featured_.size());
        switch (input.nav)
        {
        case Direction::left:
        case Direction::right:
        {
            // The banner is a loop: it has no ends to refuse at.
            const int step = input.nav == Direction::right ? 1 : -1;
            show_banner((banner_ + step + count) % count, static_cast<float>(step));
            feedback.play(audio::Cue::slider, 1.0f + 0.04f * static_cast<float>(banner_),
                          0.3f * static_cast<float>(step));
            break;
        }
        case Direction::up:
            refuse(feedback, input.nav_repeat, 0.0f, -1.0f);
            break;
        case Direction::down:
            zone_ = Zone::chips;
            feedback.play(audio::Cue::focus, 1.1f);
            break;
        case Direction::none:
            break;
        }
    }

    void step_filter(int delta, bool repeat, app::Feedback &feedback)
    {
        const int next = filter_ + delta;
        if (next < 0 || next >= kFilters)
        {
            if (!repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                chip_nudge_.trigger();
                chip_nudge_x_ = static_cast<float>(delta);
            }
            return;
        }
        // The cards on screen fade out as the new shelf arrives.
        previous_visible_ = visible_;
        previous_scroll_ = scroll_.value;
        filter_ = next;
        focus_ = 0;
        rebuild_visible();
        swap_.start(calm() ? 0.15f : 0.5f);
        Rect chips[kFilters];
        chip_layout(chips);
        feedback.play(audio::Cue::tab, 0.92f + 0.04f * static_cast<float>(next),
                      ui::pan_for_x(chips[next].cx()));
    }

    void show_banner(int slot, float direction)
    {
        if (slot == banner_)
            return;
        banner_previous_ = banner_;
        banner_ = slot;
        banner_direction_ = direction;
        banner_fade_.start(calm() ? 0.2f : 0.7f);
        banner_clock_ = 0.0f;
        drift_previous_ = drift_;
        drift_ = 0.0f;
    }

    // ---- input: the product page -------------------------------------------

    void open_page(int index, bool from_banner, app::Feedback &feedback)
    {
        page_open_ = true;
        page_item_ = index;
        page_from_banner_ = from_banner;
        page_focus_ = PageFocus::buy;
        shot_ = shot_previous_ = 0;
        shot_fade_ = {};
        page_ring_.snap(kBuyButton.inset(-6.0f));
        page_ring_radius_.snap(kButtonRadius + 6.0f);
        stock_.snap(stock_target());
        feedback.play(audio::Cue::open);
    }

    // 0: can be bought, 1: waiting in the cart, 2: owned.
    float stock_target() const
    {
        return owned(page_item_) ? 2.0f : in_cart(page_item_) ? 1.0f : 0.0f;
    }

    void page_refuse(app::Feedback &feedback, bool repeat)
    {
        if (repeat)
            return;
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        page_nudge_.trigger();
    }

    void show_shot(int shot)
    {
        if (shot == shot_)
            return;
        shot_previous_ = shot_;
        shot_ = shot;
        shot_fade_.start(calm() ? 0.12f : 0.32f);
    }

    void update_page(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::west))
        {
            open_drawer(feedback);
            return;
        }
        if (input.is_pressed(Action::back))
        {
            page_open_ = false;
            feedback.play(audio::Cue::back);
            return;
        }

        const float button_pan = ui::pan_for_x(kBuyButton.cx());
        switch (input.nav)
        {
        case Direction::left:
            if (page_focus_ != PageFocus::shots)
            {
                // Into the thumbnails, at the one the preview is showing.
                page_focus_ = PageFocus::shots;
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(thumb_rect(shot_).cx()));
            }
            else if (shot_ > 0)
            {
                show_shot(shot_ - 1);
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(thumb_rect(shot_).cx()));
            }
            else
                page_refuse(feedback, input.nav_repeat);
            break;
        case Direction::right:
            if (page_focus_ != PageFocus::shots)
                page_refuse(feedback, input.nav_repeat);
            else if (shot_ + 1 < kShots)
            {
                show_shot(shot_ + 1);
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(thumb_rect(shot_).cx()));
            }
            else
            {
                page_focus_ = PageFocus::buy;
                feedback.play(audio::Cue::focus, 1.0f, button_pan);
            }
            break;
        case Direction::up:
            if (page_focus_ == PageFocus::wishlist)
            {
                page_focus_ = PageFocus::buy;
                feedback.play(audio::Cue::focus, 1.04f, button_pan);
            }
            else
                page_refuse(feedback, input.nav_repeat);
            break;
        case Direction::down:
            if (page_focus_ == PageFocus::buy)
            {
                page_focus_ = PageFocus::wishlist;
                feedback.play(audio::Cue::focus, 0.96f, button_pan);
            }
            else
                page_refuse(feedback, input.nav_repeat);
            break;
        case Direction::none:
            break;
        }

        if (!input.is_pressed(Action::confirm))
            return;
        if (page_focus_ == PageFocus::buy)
        {
            add_to_cart(feedback);
        }
        else if (page_focus_ == PageFocus::wishlist)
        {
            const std::size_t index = static_cast<std::size_t>(page_item_);
            wished_[index] = !wished_[index];
            // The glass set has no "favorite" recording: its switch, pitched
            // up for on and down for off, says the same thing.
            feedback.play(audio::Cue::toggle, wished_[index] ? 1.12f : 0.9f, button_pan);
            if (wished_[index])
                wish_pop_.trigger();
        }
        else
        {
            // A thumbnail is already shown when it is focused: Cross gives
            // the preview a small push so the press is not swallowed.
            preview_pop_.trigger();
            feedback.play(audio::Cue::select, 1.1f, ui::pan_for_x(thumb_rect(shot_).cx()), 0.7f);
        }
    }

    void add_to_cart(app::Feedback &feedback)
    {
        if (owned(page_item_) || in_cart(page_item_))
        {
            // Nothing to buy twice. The line under the buttons says why.
            page_refuse(feedback, false);
            return;
        }
        Line line;
        line.item = page_item_;
        line.open.snap(1.0f);
        lines_.push_back(line);
        press_.trigger();
        // Each title in the cart sounds a step higher than the one before.
        const float step = static_cast<float>(std::min(live_lines(), 7));
        feedback.play(audio::Cue::select, 0.96f + 0.05f * step, ui::pan_for_x(kBuyButton.cx()));
        flight_item_ = page_item_;
        flight_active_ = true;
        flight_.start(calm() ? 0.2f : kFlySeconds);
    }

    // ---- input: the cart drawer and the checkout ---------------------------

    void open_drawer(app::Feedback &feedback)
    {
        drawer_open_ = true;
        // Straight to the point: the focus opens on Checkout, the lines are
        // one step up.
        drawer_focus_ = live_lines();
        drawer_ring_.snap(drawer_focus_target());
        feedback.play(audio::Cue::open, 1.0f, 0.5f);
    }

    void drawer_refuse(app::Feedback &feedback, bool repeat)
    {
        if (repeat)
            return;
        feedback.play(audio::Cue::error, 1.0f, 0.4f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        drawer_nudge_.trigger();
    }

    void update_drawer(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back) || input.is_pressed(Action::west))
        {
            drawer_open_ = false;
            feedback.play(audio::Cue::back, 1.0f, 0.5f);
            return;
        }
        const int live = live_lines();
        drawer_focus_ = std::clamp(drawer_focus_, 0, live);
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = drawer_focus_ + (input.nav == Direction::down ? 1 : -1);
            if (next < 0 || next > live)
                drawer_refuse(feedback, input.nav_repeat);
            else
            {
                drawer_focus_ = next;
                feedback.play(audio::Cue::focus,
                              1.08f - 0.03f * static_cast<float>(std::min(next, 8)), 0.45f);
            }
        }
        if (input.is_pressed(Action::north))
        {
            const int index = drawer_focus_ < live ? live_line(drawer_focus_) : -1;
            if (index < 0)
                drawer_refuse(feedback, false);
            else
            {
                lines_[static_cast<std::size_t>(index)].leaving = true;
                // The glass set has no "erase": a switch going off, low.
                feedback.play(audio::Cue::toggle, 0.84f, 0.45f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            if (drawer_focus_ < live)
            {
                // Cross on a line is "I am done looking": on to the button.
                drawer_focus_ = live;
                feedback.play(audio::Cue::focus, 0.9f, 0.45f);
            }
            else if (live == 0)
                drawer_refuse(feedback, false);
            else
            {
                dialog_open_ = true;
                dialog_choice_ = 0; // a purchase is confirmed, never defaulted
                dialog_position_.snap(0.0f);
                feedback.play(audio::Cue::modal_open);
            }
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = input.nav == Direction::right ? 1 : 0;
            if (next != dialog_choice_)
            {
                dialog_choice_ = next;
                feedback.play(audio::Cue::focus, 1.0f, next == 0 ? -0.12f : 0.12f);
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                dialog_nudge_.trigger();
            }
        }
        if (input.is_pressed(Action::back) ||
            (input.is_pressed(Action::confirm) && dialog_choice_ == 0))
        {
            dialog_open_ = false;
            feedback.play(audio::Cue::modal_close);
            return;
        }
        if (input.is_pressed(Action::confirm))
            purchase(feedback);
    }

    void purchase(app::Feedback &feedback)
    {
        dialog_open_ = false;
        bought_ = 0;
        for (Line &line : lines_)
        {
            if (line.leaving)
                continue;
            owned_[static_cast<std::size_t>(line.item)] = true;
            line.leaving = true;
            line.sold = true;
            // The rows leave one after the other, top first.
            line.delay = calm() ? 0.0f : 0.07f * static_cast<float>(bought_);
            ++bought_;
        }
        settling_ = true;
        drawer_focus_ = 0;
        // Ownership changed what the filters hold: the shelves follow.
        previous_visible_ = visible_;
        previous_scroll_ = scroll_.value;
        rebuild_visible();
        swap_.start(calm() ? 0.15f : 0.5f);
        feedback.play(audio::Cue::saved);
        feedback.rumble(0.5f, 0.12f);
    }

    // ---- animation ---------------------------------------------------------

    void apply_palette(bool snap)
    {
        // The page takes its own title's colours while it is open.
        const demo::Item &it =
            item(page_open_ ? page_item_ : featured_[static_cast<std::size_t>(banner_)]);
        const Color targets[3] = {gfx::mix(kCoal, it.dark, 0.3f),
                                  gfx::mix(kCoalLow, it.dark, 0.12f),
                                  gfx::mix(it.mid, it.accent, 0.15f)};
        for (int i = 0; i < 3; ++i)
        {
            if (snap)
                palette_[i].snap(targets[i]);
            else
                palette_[i].target(targets[i]);
        }
        const demo::Item &shown = item(featured_[static_cast<std::size_t>(banner_)]);
        const Color tone = gfx::mix(kPanel, shown.dark, 0.5f);
        if (snap)
            tone_.snap(tone);
        else
            tone_.target(tone);
    }

    void animate(float dt, app::Feedback &feedback)
    {
        // The banner turns its own pages, except while the player is on it
        // or busy with something in front of it.
        const bool covered = page_open_ || drawer_open_ || dialog_open_;
        if (zone_ != Zone::banner && !covered)
        {
            banner_clock_ += dt;
            if (banner_clock_ >= kBannerSeconds)
                show_banner((banner_ + 1) % static_cast<int>(featured_.size()), 1.0f);
        }
        if (!covered && !calm())
        {
            drift_ += dt;
            drift_previous_ += dt;
        }
        banner_fade_.update(dt);
        banner_focus_.target = zone_ == Zone::banner ? 1.0f : 0.0f;
        banner_focus_.update(dt, 16.0f);
        apply_palette(false);
        for (ui::SpringColor &colour : palette_)
            colour.update(dt, 3.0f);
        tone_.update(dt, 4.0f);

        // The page scroll, then everything that rides on it.
        scroll_.target = scroll_target();
        scroll_.update(dt, omega(11.0f));
        swap_.update(dt);
        const int focused = zone_ == Zone::grid && !visible_.empty()
                                ? visible_[static_cast<std::size_t>(focus_)]
                                : -1;
        for (int i = 0; i < static_cast<int>(lift_.size()); ++i)
        {
            lift_[static_cast<std::size_t>(i)].target = i == focused ? 1.0f : 0.0f;
            lift_[static_cast<std::size_t>(i)].update(dt, 18.0f);
        }
        ring_.target(focus_target());
        ring_.update(dt, 20.0f);
        ring_radius_.target = focus_radius();
        ring_radius_.update(dt, 20.0f);
        plate_.target = focused >= 0 ? 1.0f : 0.0f;
        plate_.update(dt, 16.0f);
        glow_.target(focused >= 0 ? item(focused).accent : kAccent);
        glow_.update(dt, 10.0f);
        Rect chips[kFilters];
        chip_layout(chips);
        chip_pill_.target(chips[filter_]);
        chip_pill_.update(dt, 20.0f);
        // The chips stick instantly: their pill must not lag behind them.
        chip_pill_.y.snap(chips[filter_].y);
        nudge_.update(dt, 9.0f);
        chip_nudge_.update(dt, 9.0f);

        // The product page.
        page_.target = page_open_ ? 1.0f : 0.0f;
        page_.update(dt, omega(page_open_ ? 9.5f : 12.0f));
        shot_fade_.update(dt);
        Rect ring = kBuyButton.inset(-6.0f);
        float radius = kButtonRadius + 6.0f;
        if (page_focus_ == PageFocus::wishlist)
            ring = kWishButton.inset(-6.0f);
        else if (page_focus_ == PageFocus::shots)
        {
            ring = thumb_rect(shot_).inset(-6.0f);
            radius = 20.0f;
        }
        page_ring_.target(ring);
        page_ring_.update(dt, 20.0f);
        page_ring_radius_.target = radius;
        page_ring_radius_.update(dt, 20.0f);
        stock_.target = stock_target();
        stock_.update(dt, 14.0f);
        press_.update(dt, 9.0f);
        page_nudge_.update(dt, 9.0f);
        wish_pop_.update(dt, 6.0f);
        preview_pop_.update(dt, 7.0f);

        // The flight into the cart, and the counter it lands in.
        if (flight_active_)
        {
            flight_.update(dt);
            if (!flight_.running)
            {
                flight_active_ = false;
                bump_.kick(calm() ? 0.0f : 9.0f);
                const float step = static_cast<float>(std::min(live_lines(), 7));
                feedback.play(audio::Cue::notify, 0.96f + 0.05f * step, ui::pan_for_x(kCartX));
            }
        }
        const int count = live_lines();
        if (!flight_active_ && count != count_shown_)
        {
            count_previous_ = count_shown_;
            count_shown_ = count;
            count_roll_.start(0.28f);
        }
        count_roll_.update(dt);
        bump_.update(dt, 20.0f, 0.42f);
        badge_.target = count_shown_ > 0 ? 1.0f : 0.0f;
        badge_.update(dt, 16.0f);

        // The drawer: rows close, the list keeps the focus in view, the
        // subtotal counts while somebody can watch it.
        drawer_.target = drawer_open_ ? 1.0f : 0.0f;
        drawer_.update(dt, omega(12.0f));
        for (Line &line : lines_)
        {
            if (line.leaving && line.delay > 0.0f)
                line.delay -= dt;
            line.open.target = line.leaving && line.delay <= 0.0f ? 0.0f : 1.0f;
            line.open.update(dt, omega(10.0f));
        }
        std::erase_if(lines_,
                      [](const Line &line) { return line.leaving && line.open.value < 0.01f; });
        const int live = live_lines();
        drawer_focus_ = std::clamp(drawer_focus_, 0, live);
        if (drawer_focus_ < live)
        {
            const float start = line_offset(live_line(drawer_focus_));
            drawer_scroll_.reveal(start, start + kLineH, kLinesView, 0.0f,
                                  line_offset(static_cast<int>(lines_.size())));
        }
        else
        {
            // On the button: show the end of the list, where the total is.
            const float end = line_offset(static_cast<int>(lines_.size()));
            drawer_scroll_.position.target = std::max(0.0f, end - kLinesView);
        }
        drawer_scroll_.update(dt, omega(14.0f));
        empty_.target = lines_.empty() ? 1.0f : 0.0f;
        empty_.update(dt, omega(10.0f));
        drawer_ring_.target(drawer_focus_target());
        drawer_ring_.update(dt, 20.0f);
        drawer_nudge_.update(dt, 9.0f);
        // A one-shot count (not a spring): it must arrive on the exact
        // number at a known moment, not creep toward it.
        if (drawer_.value > 0.5f)
        {
            if (subtotal() != subtotal_to_)
            {
                subtotal_from_ = subtotal_shown();
                subtotal_to_ = subtotal();
                subtotal_count_.start(calm() ? 0.15f : 0.55f);
            }
            subtotal_count_.update(dt);
        }

        // The checkout: when the last bought row has left, say so.
        dialog_.target = dialog_open_ ? 1.0f : 0.0f;
        dialog_.update(dt, omega(14.0f));
        dialog_position_.target = static_cast<float>(dialog_choice_);
        dialog_position_.update(dt, 22.0f);
        dialog_nudge_.update(dt, 9.0f);
        if (settling_)
        {
            bool busy = false;
            for (const Line &line : lines_)
                busy = busy || line.sold;
            if (!busy)
            {
                settling_ = false;
                toast_left_ = kToastSeconds;
                toast_age_ = 0.0f;
                toast_count_ = bought_;
                feedback.play(audio::Cue::notify, 1.12f);
            }
        }
        toast_left_ = std::max(0.0f, toast_left_ - dt);
        toast_age_ += dt;
        toast_.target = toast_left_ > 0.0f ? 1.0f : 0.0f;
        toast_.update(dt, omega(13.0f));
    }

    Rect drawer_focus_target() const
    {
        const int live = live_lines();
        if (drawer_focus_ >= live)
            return checkout_rect().inset(-6.0f);
        const float x = kWidth - kDrawerW;
        const float y = kLinesTop + line_offset(live_line(drawer_focus_)) - drawer_scroll_.offset();
        return {x + 24.0f, y + 4.0f, kDrawerW - 48.0f, kLineH - 8.0f};
    }

    // ---- drawing: prices ---------------------------------------------------

    // A price: the coin mark, then the number in the mono face, so the
    // digits keep their places while a total counts. Returns the width.
    //
    // Like every price routine it takes the layers to draw (see Layer): the
    // geometry is the same in each pass, only some of it is recorded.
    float amount(gfx::DrawList &list, int credits, float x, float baseline, float size,
                 Color colour, bool right_align = false, unsigned layers = kAllLayers) const
    {
        char text[24];
        format_credits(text, sizeof(text), credits);
        const float radius = size * 0.28f;
        const float gap = size * 0.24f;
        const float width = radius * 2.0f + gap + context_.fonts.mono.measure(text, size);
        const float left = right_align ? x - width : x;
        if (layers & kShapes)
            draw_coin(list, left + radius, baseline - size * 0.35f, radius, colour);
        if (layers & kMono)
            ui::text(list, context_.fonts.mono, text, left + radius * 2.0f + gap, baseline, size,
                     colour);
        return width;
    }

    // The price that no longer applies: dimmer, with a thin line through it.
    float struck(gfx::DrawList &list, int credits, float x, float baseline, float size,
                 Color colour, bool right_align = false, unsigned layers = kAllLayers) const
    {
        char text[24];
        format_credits(text, sizeof(text), credits);
        const float width = context_.fonts.mono.measure(text, size);
        const float left = right_align ? x - width : x;
        if (layers & kMono)
            ui::text(list, context_.fonts.mono, text, left, baseline, size, colour);
        const float y = baseline - size * 0.34f;
        if (layers & kShapes)
            list.line(left - 3.0f, y, left + width + 3.0f, y, std::max(1.6f, size * 0.08f),
                      colour.with_alpha(1.4f));
        return width;
    }

    // "-30%" on the accent. Returns the width.
    float discount_badge(gfx::DrawList &list, int percent, float x, float cy, float height,
                         float alpha, unsigned layers = kAllLayers) const
    {
        char text[8];
        std::snprintf(text, sizeof(text), "-%d%%", percent);
        const float size = height * 0.6f;
        const float width = context_.fonts.semibold.measure(text, size) + height * 0.8f;
        if (layers & kShapes)
            list.rounded_rect({x, cy - height * 0.5f, width, height}, height * 0.32f,
                              kAccent.with_alpha(alpha));
        if (layers & kSemibold)
            ui::text(list, context_.fonts.semibold, text, x + width * 0.5f, centred(cy, size), size,
                     kOnAccent.with_alpha(alpha), gfx::Align::center);
        return width;
    }

    // What a title costs, the same way everywhere: "Owned" for what the
    // player has, "Free", a plain price, or the new price in the accent
    // followed by the old one struck through. Returns the width.
    float price_line(gfx::DrawList &list, int index, float x, float baseline, float size,
                     float alpha, bool badge, unsigned layers = kAllLayers) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Offer &o = offer(index);
        if (owned(index))
        {
            const float s = size * 0.94f;
            if (layers & kShapes)
                draw_check(list, x + s * 0.45f, baseline - s * 0.36f, s * 0.78f,
                           kOwned.with_alpha(alpha));
            if (layers & kSemibold)
                ui::text(list, fonts.semibold, "Owned", x + s * 1.2f, baseline, s,
                         kOwned.with_alpha(alpha));
            return s * 1.2f + fonts.semibold.measure("Owned", s);
        }
        if (o.free())
        {
            if (layers & kSemibold)
                ui::text(list, fonts.semibold, "Free", x, baseline, size, kInk.with_alpha(alpha));
            return fonts.semibold.measure("Free", size);
        }
        if (o.percent == 0)
            return amount(list, o.price(), x, baseline, size, kInk.with_alpha(alpha), false,
                          layers);
        float cursor = x + amount(list, o.price(), x, baseline, size, kAccent.with_alpha(alpha),
                                  false, layers);
        cursor += size * 0.55f;
        cursor += struck(list, o.base, cursor, baseline, std::max(17.0f, size * 0.58f),
                         kInk.with_alpha(0.5f * alpha), false, layers);
        if (badge)
        {
            cursor += size * 0.5f;
            cursor += discount_badge(list, o.percent, cursor, baseline - size * 0.36f, size * 0.84f,
                                     alpha, layers);
        }
        return cursor - x;
    }

    // ---- drawing: the home page --------------------------------------------

    void draw_top_bar(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        list.push_opacity(in);
        const float y = kCartY - 8.0f * (1.0f - in) * travel();
        list.rotated_rect({kMargin + 2.0f, y - 11.0f, 22.0f, 22.0f}, 5.0f, 0.7854f, kAccent);
        list.rotated_rect({kMargin + 8.0f, y - 5.0f, 10.0f, 10.0f}, 2.0f, 0.7854f, kCoal);
        ui::text(list, fonts.semibold, "STOREFRONT", kMargin + 42.0f, centred(y, 22), 22, kInk,
                 gfx::Align::left, 5.0f);
        list.pop_opacity();
    }

    // One featured title's words and price, at an opacity and an offset so
    // two of them can cross-fade.
    void draw_banner_text(gfx::DrawList &list, int slot, const Rect &b, float alpha,
                          float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const int index = featured_[static_cast<std::size_t>(slot)];
        const demo::Item &it = item(index);
        const Offer &o = offer(index);
        const float x = b.x + 56.0f + slide;
        const char *kicker = owned(index)              ? "IN YOUR LIBRARY"
                             : o.percent >= 40         ? "DEAL OF THE WEEK"
                             : o.percent > 0           ? "ON SALE"
                             : o.free()                ? "FREE TO KEEP"
                             : it.year == newest_year_ ? "NEW RELEASE"
                                                       : "FEATURED";
        list.push_opacity(alpha);
        list.rounded_rect({x, b.y + 71.0f, 28.0f, 3.0f}, 1.5f, kAccent);
        ui::text(list, fonts.semibold, kicker, x + 40.0f, b.y + 80.0f, 18, kAccent,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(it.title, 66, 700.0f), x - 3.0f,
                 b.y + 164.0f, 66, kInk);
        ui::text(list, fonts.regular, fonts.regular.font->fit(pitch(index), 27, 660.0f), x,
                 b.y + 212.0f, 27, kInk.with_alpha(0.78f));
        price_line(list, index, x, b.y + 296.0f, 46, 1.0f, true);
        list.pop_opacity();
    }

    void draw_banner(gfx::DrawList &list) const
    {
        // The banner fades as the page scrolls it under the top bar; once it
        // is gone it costs nothing.
        const float scroll = scroll_.value;
        const float in = tween::stagger(age_, 1, 0.07f, 0.6f);
        const float visible = in * tween::clamp01(1.0f - scroll / 360.0f);
        if (visible <= 0.004f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        Rect b = banner_rect();
        b.y += 28.0f * (1.0f - in) * travel() - scroll;
        const int index = featured_[static_cast<std::size_t>(banner_)];
        const int before = featured_[static_cast<std::size_t>(banner_previous_)];
        const float fade = banner_fade_.running ? banner_fade_.progress() : 1.0f;
        const Color tone = tone_.value();

        list.push_opacity(visible);
        list.shadow({b.x, b.y + 18.0f, b.w, b.h}, kBannerRadius, 44, kBlack.with_alpha(0.5f));
        list.rounded_rect(b, kBannerRadius, tone);

        // The artwork: the old one stays whole under the new one fading in,
        // so the plate never shows through in the middle of the change.
        const Rect art{b.x + b.w - kBannerArtW, b.y, kBannerArtW, b.h};
        if (fade < 1.0f)
            list.image(item(before).cover, art, banner_uv(banner_previous_, drift_previous_),
                       kWhite, kBannerRadius);
        list.image(item(index).cover, art, banner_uv(banner_, drift_),
                   kWhite.with_alpha(tween::smoothstep(fade)), kBannerRadius);
        // The plate's own colour runs into the picture, so the words sit on
        // a calm surface and the artwork has no left edge.
        list.gradient_rect_h({art.x - 1.0f, b.y, 360.0f, b.h}, 0, tone, tone.with_alpha(0.0f));
        // A little of the title's own light behind the words.
        list.glow({b.x + 180.0f, b.y + 170.0f, 420.0f, 80.0f}, 40, 130,
                  item(index).accent.with_alpha(0.07f * tween::smoothstep(fade)));
        list.bordered_rect(b, kBannerRadius, kClear, 1.5f, kInk.with_alpha(0.1f));

        if (fade < 1.0f)
        {
            // The old words leave quickly; the new ones arrive a beat later.
            const float side = banner_direction_ * travel();
            draw_banner_text(list, banner_previous_, b, 1.0f - tween::smoothstep(fade * 2.4f),
                             -40.0f * side * tween::cubic_in(tween::clamp01(fade * 2.4f)));
            const float arrive = tween::clamp01((fade - 0.3f) / 0.7f);
            draw_banner_text(list, banner_, b, tween::smoothstep(arrive),
                             48.0f * side * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_banner_text(list, banner_, b, 1.0f, 0.0f);
        }

        // The call to action lights up when the banner has the focus.
        const float lit = banner_focus_.value;
        const Rect action{b.x + 56.0f, b.y + 324.0f, 214.0f, 52.0f};
        list.bordered_rect(action, 26, gfx::mix(kInk.with_alpha(0.06f), kAccent, lit), 1.5f,
                           gfx::mix(kInk.with_alpha(0.3f), kAccent, lit));
        ui::text(list, fonts.semibold, "View details", action.cx(), centred(action.cy(), 22), 22,
                 gfx::mix(kInk.with_alpha(0.86f), kOnAccent, lit), gfx::Align::center);

        // Page dots: the current one is a small bar that fills as its six
        // seconds pass, and stops filling while the banner waits.
        const int count = static_cast<int>(featured_.size());
        const float dots_w = static_cast<float>(count - 1) * 20.0f + 44.0f;
        float x = b.x + b.w - 44.0f - dots_w;
        const float cy = b.y + b.h - 34.0f;
        list.rounded_rect({x - 14.0f, cy - 13.0f, dots_w + 28.0f, 26.0f}, 13,
                          kBlack.with_alpha(0.38f));
        for (int i = 0; i < count; ++i)
        {
            if (i == banner_)
            {
                list.rounded_rect({x, cy - 4.0f, 44.0f, 8.0f}, 4, kInk.with_alpha(0.3f));
                const float filled = 8.0f + 36.0f * tween::clamp01(banner_clock_ / kBannerSeconds);
                list.rounded_rect({x, cy - 4.0f, filled, 8.0f}, 4, kInk);
                x += 56.0f;
            }
            else
            {
                list.circle(x + 4.0f, cy, 4.0f, kInk.with_alpha(0.42f));
                x += 20.0f;
            }
        }
        list.pop_opacity();
    }

    void draw_chips(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle glyphs = ui::GlyphStyle::dark();
        const float in = tween::stagger(age_, 3, 0.07f, 0.5f);
        Rect chips[kFilters];
        chip_layout(chips);
        const float cy = chips_y() + kChipH * 0.5f;
        char number[8];
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, 16.0f * (1.0f - in) * travel());
        ui::draw_button(list, fonts, glyphs, ui::Button::l2, kMargin, cy, 30);

        // The active filter is one pill that glides between the chips.
        Rect pill = chip_pill_.value();
        pill.x += ui::shake(chip_nudge_.value, clock_) * chip_nudge_x_;
        list.rounded_rect(pill, kChipH * 0.5f, kInk);
        // Outlines, then labels, then counts: three draw calls for the row.
        for (int pass = 0; pass < 3; ++pass)
        {
            for (int i = 0; i < kFilters; ++i)
            {
                const Rect &chip = chips[i];
                // How much of this chip the pill covers decides its ink.
                const float overlap =
                    std::min(pill.x + pill.w, chip.x + chip.w) - std::max(pill.x, chip.x);
                const float on = tween::clamp01(overlap / chip.w);
                const Color ink = gfx::mix(kInk.with_alpha(0.78f), kCoal, on);
                const float label = fonts.semibold.measure(kFilterNames[i], 22);
                if (pass == 0)
                    list.bordered_rect(chip, kChipH * 0.5f, kClear, 1.5f,
                                       kInk.with_alpha(0.2f * (1.0f - on)));
                else if (pass == 1)
                    ui::text(list, fonts.semibold, kFilterNames[i], chip.x + 24.0f, centred(cy, 22),
                             22, ink);
                else
                {
                    std::snprintf(number, sizeof(number), "%d", counts_[i]);
                    ui::text(list, fonts.mono, number, chip.x + 34.0f + label, centred(cy, 18), 18,
                             ink.with_alpha(0.6f));
                }
            }
        }
        const Rect &last = chips[kFilters - 1];
        ui::draw_button(list, fonts, glyphs, ui::Button::r2, last.x + last.w + 16.0f, cy, 30);
        list.pop_transform();
        list.pop_opacity();
    }

    // The grid's window: from just under the chips to above the hint row.
    float window_top() const
    {
        return chips_y() + kChipH + 4.0f;
    }

    // How visible something at screen height y is inside the grid's window:
    // it fades in under the chips and out toward the hint row.
    float fade_at(float y) const
    {
        return std::min(tween::clamp01((y - window_top()) / kFadeHead),
                        tween::clamp01((kViewBottom - y) / kFadeFoot));
    }
    // A line of text: gone when its top reaches the window's top edge or its
    // foot the bottom one.
    float fade_text(float baseline, float size) const
    {
        return std::min(fade_at(baseline - size * 0.8f), fade_at(baseline + 4.0f));
    }
    // The same for a picture spanning y0..y1, as the opacity at its two ends.
    // An image's tint can only run linearly from top to bottom, so the line
    // is laid through "nothing at the window's edge": the far end may come
    // out below zero, but that part lies outside the clip and is never drawn.
    void fade_span(float y0, float y1, float *a0, float *a1) const
    {
        const float top = window_top();
        *a0 = *a1 = 1.0f;
        if (y0 < top + kFadeHead)
        {
            const float full = std::max(y1, top + kFadeHead);
            *a0 = (y0 - top) / (full - top);
            *a1 = (y1 - top) / (full - top);
        }
        else if (y1 > kViewBottom - kFadeFoot)
        {
            const float full = std::min(y0, kViewBottom - kFadeFoot);
            *a0 = (kViewBottom - y0) / (kViewBottom - full);
            *a1 = (kViewBottom - y1) / (kViewBottom - full);
        }
    }

    // One product card: cover, badges, title, price. The cover's tint runs
    // from the visibility at its top to the one at its bottom, which gives
    // the grid a per-pixel soft edge without painting over the backdrop.
    void draw_card(gfx::DrawList &list, int index, const Rect &r, float alpha,
                   unsigned layers) const
    {
        float top = 1.0f, foot = 1.0f;
        fade_span(r.y, r.y + kCoverH, &top, &foot);
        if (top <= 0.01f && foot <= 0.01f)
            return;
        top *= alpha;
        foot *= alpha;
        const ui::Fonts &fonts = context_.fonts;
        const Offer &o = offer(index);
        const float lift = lift_[static_cast<std::size_t>(index)].value;
        const Rect cover{r.x, r.y, r.w, kCoverH};
        // While the page is open the cover has left its card: an empty slot.
        const bool travelling =
            page_.value > 0.01f && !page_from_banner_ && index == page_item_ && !calm();
        if (travelling && (layers & kShapes))
        {
            list.gradient_rect(cover, kCardRadius, kInk.with_alpha(0.05f * top),
                               kInk.with_alpha(0.05f * foot));
        }
        else if (!travelling && (layers & kImages))
        {
            const float light = 0.84f + 0.16f * lift; // resting covers sit back a little
            list.image_gradient(item(index).cover, cover, kCardCrop,
                                Color{light, light, light, top}, Color{light, light, light, foot},
                                kCardRadius);
        }
        const float marks = alpha * fade_at(r.y + 8.0f);
        if (marks > 0.01f)
        {
            if (o.percent > 0 && !owned(index))
                discount_badge(list, o.percent, r.x + 10.0f, r.y + 26.0f, 32.0f, marks, layers);
            float mark = r.x + r.w - 26.0f;
            if (in_cart(index) && (layers & kShapes))
            {
                list.circle(mark, r.y + 26.0f, 17.0f, kAccent.with_alpha(marks));
                draw_cart(list, mark, r.y + 26.0f, 17.0f, kOnAccent.with_alpha(marks));
                mark -= 40.0f;
            }
            if (wished_[static_cast<std::size_t>(index)] && (layers & kShapes))
            {
                list.circle(mark, r.y + 26.0f, 17.0f, kBlack.with_alpha(0.5f * marks));
                list.star(mark, r.y + 26.0f, 10.0f, kStar.with_alpha(marks));
            }
        }
        // On the focus plate the words step in from its edge.
        const float x = r.x + 2.0f + 10.0f * lift;
        const float title = alpha * fade_text(r.y + kCoverH + 38.0f, 24);
        if (title > 0.01f && (layers & kSemibold))
            ui::text(list, fonts.semibold, card_titles_[static_cast<std::size_t>(index)], x,
                     r.y + kCoverH + 38.0f, 24, kInk.with_alpha(title * (0.84f + 0.16f * lift)));
        const float price = alpha * fade_text(r.y + kCoverH + 74.0f, 22);
        if (price > 0.01f)
            price_line(list, index, x, r.y + kCoverH + 74.0f, 22, price, false, layers);
    }

    void draw_grid(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float scroll = scroll_.value;
        const Rect window{0, window_top(), kWidth, kViewBottom - window_top()};
        const int count = static_cast<int>(visible_.size());
        const int focused = zone_ == Zone::grid && count > 0 ? focus_ : -1;
        // A change of filter: the old shelf fades where it stood while the
        // new one arrives card by card.
        const bool swapping = swap_.running;
        const float swap_age = swapping ? swap_.elapsed - 0.1f : 10.0f;

        list.push_clip(window);
        if (swapping)
        {
            const float leave = 1.0f - tween::smoothstep(swap_.progress() * 3.0f);
            for (int k = 0; k < static_cast<int>(previous_visible_.size()); ++k)
            {
                Rect r = card_rect(k);
                r.y -= previous_scroll_;
                if (r.y < kViewBottom && r.y + r.h > window.y)
                    draw_card(list, previous_visible_[static_cast<std::size_t>(k)], r, leave,
                              kAllLayers);
            }
        }
        // Layer by layer across the cards, not card by card (see Layer).
        for (const unsigned layer : {kImages, kShapes, kSemibold, kMono})
        {
            for (int k = 0; k < count; ++k)
            {
                if (k != focused) // that one is drawn last, on top of its neighbours
                    draw_grid_card(list, k, scroll, swap_age, layer);
            }
        }
        if (count == 0)
        {
            const float in = tween::clamp01(swap_age / 0.3f) * tween::stagger(age_, 5, 0.07f, 0.5f);
            ui::text(list, fonts.semibold, "Nothing on this shelf right now", 960, 760, 30,
                     kInk.with_alpha(0.86f * in), gfx::Align::center);
            ui::text(list, fonts.regular,
                     "Everything it held is already yours. Try another filter.", 960, 802, 24,
                     kInk.with_alpha(0.6f * in), gfx::Align::center);
        }
        list.pop_clip();

        // The focus highlight: one object that glides between the banner,
        // the chips and the cards. On a card it is also the plate, the
        // shadow and the light under it.
        const float in = tween::stagger(age_, 4, 0.07f, 0.5f);
        Rect ring = ring_.value();
        const float shake = ui::shake(nudge_.value, clock_);
        ring.x += shake * nudge_x_;
        ring.y += shake * nudge_y_ - scroll;
        const float radius = ring_radius_.value;
        const float plate = plate_.value * in;
        if (plate > 0.01f)
        {
            list.shadow({ring.x, ring.y + 16.0f, ring.w, ring.h}, radius, 38,
                        kBlack.with_alpha(0.55f * plate));
            list.glow(ring, radius, 30, glow_.value((0.26f + 0.12f * breathe()) * plate));
            list.rounded_rect(ring, radius, kPanel.with_alpha(plate));
        }
        list.bordered_rect(ring, radius, kClear, 3.0f, kInk.with_alpha(0.94f * in));
        if (focused >= 0)
        {
            list.push_clip(window);
            draw_grid_card(list, focused, scroll, swap_age, kAllLayers);
            list.pop_clip();
        }
    }

    void draw_grid_card(gfx::DrawList &list, int k, float scroll, float swap_age,
                        unsigned layers) const
    {
        Rect r = card_rect(k);
        r.y -= scroll;
        if (r.y > kViewBottom || r.y + r.h < kPageTop)
            return;
        const int index = visible_[static_cast<std::size_t>(k)];
        const float in =
            tween::stagger(age_ - 0.3f, k % kColumns + 2 * (k / kColumns), 0.05f, 0.45f) *
            tween::stagger(swap_age, k, 0.03f, 0.3f);
        const float lift = lift_[static_cast<std::size_t>(index)].value;
        const float nudge =
            k == focus_ && zone_ == Zone::grid ? ui::shake(nudge_.value, clock_) : 0.0f;
        list.push_transform(1.0f + kLift * lift, r.cx(), r.cy(), nudge * nudge_x_,
                            nudge * nudge_y_ + 26.0f * (1.0f - in) * travel());
        draw_card(list, index, r, in, layers);
        list.pop_transform();
    }

    // ---- drawing: the product page -----------------------------------------

    void draw_page(gfx::DrawList &list, std::uint32_t glass) const
    {
        const float t = page_.value;
        if (t <= 0.004f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(page_item_);
        const Offer &o = offer(page_item_);
        char text[96];

        // The whole home page, blurred and darkened, is the page's wall.
        const Rect full{0, 0, kWidth, kHeight};
        const float veil = tween::clamp01(t * 1.7f);
        list.glass(glass, full, 0, kWhite.with_alpha(veil));
        list.rounded_rect(full, 0, gfx::mix(kCoal, it.dark, 0.3f).with_alpha(0.87f * veil));
        list.glow(kPreview.inset(60.0f), 60, 220, it.accent.with_alpha(0.1f * veil));

        // Everything but the cover fades in once the cover is well on its way.
        const float content = tween::smoothstep((t - 0.45f) / 0.55f);

        // Thumbnails rise in one after the other once the cover is nearly
        // home; the shown one is lit.
        const float settled = tween::clamp01((t - 0.6f) / 0.4f);
        for (int i = 0; i < kShots; ++i)
        {
            const float in = tween::stagger(settled, i, 0.14f, 0.7f);
            Rect thumb = thumb_rect(i);
            thumb.y += 22.0f * (1.0f - in) * travel();
            const float light = i == shot_ ? 1.0f : 0.7f;
            const Color tint = shot_tint(i);
            list.image(it.cover, thumb, shot_uv(i, true),
                       Color{tint.r * light, tint.g * light, tint.b * light, in * content}, 14);
            if (i == shot_)
                list.rounded_rect(
                    {thumb.x + thumb.w * 0.5f - 18.0f, thumb.y + thumb.h + 10.0f, 36.0f, 4.0f}, 2,
                    kInk.with_alpha(0.8f * in * content));
        }

        // The words slide in a little from the right.
        const float slide = calm() ? 0.0f : 56.0f * (1.0f - tween::cubic_out(t));
        list.push_opacity(content);
        list.push_transform(1.0f, 0, 0, slide, 0);
        ui::text(list, fonts.semibold, ui::upper(it.genre), kInfoX, 170, 20, it.accent,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(it.title, 76, kRight - kInfoX),
                 kInfoX - 4.0f, 252, 76, kInk);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %d", it.studio, it.year);
        ui::text(list, fonts.regular, text, kInfoX, 302, 26, kInk.with_alpha(0.72f));

        // Rating: five outlines, filled as far as the rating reaches. The
        // partly filled star is a full one behind a clip.
        float x = kInfoX + 14.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float fill = tween::clamp01(it.rating - static_cast<float>(i));
            list.star(x, 346, 14, kStar.with_alpha(0.3f), 2.0f);
            if (fill >= 0.99f)
                list.star(x, 346, 14, kStar);
            else if (fill > 0.01f)
            {
                list.push_clip({x - 14.0f, 332, 28.0f * fill, 28});
                list.star(x, 346, 14, kStar);
                list.pop_clip();
            }
            x += 34.0f;
        }
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
        x += ui::text(list, fonts.semibold, text, x + 2.0f, 355, 26, kInk) + 16.0f;
        char number[16];
        format_credits(number, sizeof(number), o.ratings);
        std::snprintf(text, sizeof(text), "%s ratings", number);
        ui::text(list, fonts.regular, text, x, 355, 22, kInk.with_alpha(0.56f));

        // Tags as chips.
        char players[32];
        if (it.players == 1)
            std::snprintf(players, sizeof(players), "Single player");
        else
            std::snprintf(players, sizeof(players), "1\xE2\x80\x93%d players", it.players);
        const char *tags[] = {it.genre, players,
                              it.year == newest_year_ ? "New this year" : nullptr};
        for (const bool words : {false, true}) // the pills, then their labels
        {
            x = kInfoX;
            for (const char *tag : tags)
            {
                if (tag == nullptr)
                    continue;
                const float w = fonts.semibold.measure(tag, 20) + 36.0f;
                if (words)
                    ui::text(list, fonts.semibold, tag, x + 18.0f, centred(420, 20), 20,
                             kInk.with_alpha(0.86f));
                else
                    list.bordered_rect({x, 400, w, 40}, 20, kInk.with_alpha(0.07f), 1.5f,
                                       kInk.with_alpha(0.2f));
                x += w + 10.0f;
            }
        }

        // The copy: the pitch as a lead line, then the blurb. The block sits
        // centred between the tags and the facts, however its lines wrap.
        const Copy &copy = copy_[static_cast<std::size_t>(page_item_)];
        const float block = static_cast<float>(copy.pitch_lines + copy.blurb_lines) * 40.0f + 12.0f;
        float y = 602.0f - block * 0.5f + 30.0f;
        y = ui::paragraph(list, fonts.semibold, pitch(page_item_), kInfoX, y, 30, copy.pitch_width,
                          40, kInk, 2);
        ui::paragraph(list, fonts.regular, it.blurb, kInfoX, y + 12.0f, 26, kInfoW, 40,
                      kInk.with_alpha(0.74f), 4);

        // Three facts along the foot of the column.
        list.rounded_rect({kInfoX, 764, kInfoW, 1.5f}, 0, kInk.with_alpha(0.12f));
        char released[16], size[16];
        std::snprintf(released, sizeof(released), "%d", it.year);
        std::snprintf(size, sizeof(size), "%d GB", o.size_gb);
        const char *labels[] = {"RELEASED", "DOWNLOAD", "STUDIO"};
        const std::string studio = fonts.semibold.font->fit(it.studio, 24, 216.0f);
        const char *values[] = {released, size, studio.c_str()};
        const float columns[] = {kInfoX, kInfoX + 150.0f, kInfoX + 300.0f};
        for (int i = 0; i < 3; ++i)
        {
            ui::text(list, fonts.semibold, labels[i], columns[i], 808, 15, kInk.with_alpha(0.5f),
                     gfx::Align::left, 3.0f);
            ui::text(list, fonts.semibold, values[i], columns[i], 846, 24, kInk.with_alpha(0.92f));
        }
        list.pop_transform();
        list.pop_opacity();

        draw_buy_box(list, glass, content);

        // The shared element, drawn last so it passes over the words: the
        // cover travels from where it was opened to the preview, and its crop
        // opens up to the whole picture on the way.
        Rect from, from_uv;
        float from_radius = kPreviewRadius;
        const bool shared = !calm() && page_source(&from, &from_uv, &from_radius);
        const float way = shared ? t : 1.0f;
        const Rect cover = shared ? mix_rect(from, kPreview, way) : kPreview;
        const Rect uv = shared ? mix_rect(from_uv, kCoverUv, way) : kCoverUv;
        const float radius = tween::lerp(from_radius, kPreviewRadius, way);
        const float cover_alpha = shared ? tween::clamp01(t * 14.0f) : content;
        const float pop = 1.0f + 0.022f * preview_pop_.value;
        list.push_transform(pop, kPreview.cx(), kPreview.cy(), 0, 0);
        list.shadow({cover.x, cover.y + 26.0f * way, cover.w, cover.h}, radius, 56,
                    kBlack.with_alpha(0.55f * veil));
        list.image(it.cover, cover, uv, kWhite.with_alpha(cover_alpha), radius);
        // The other screenshots cross-fade over the landed cover.
        const float landed = tween::clamp01((t - 0.9f) / 0.1f);
        if (landed > 0.0f)
        {
            const float fade = shot_fade_.running ? tween::smoothstep(shot_fade_.progress()) : 1.0f;
            if (fade < 1.0f && shot_previous_ != 0)
                list.image(it.cover, kPreview, shot_uv(shot_previous_, false),
                           shot_tint(shot_previous_).with_alpha(landed), kPreviewRadius);
            if (shot_ != 0 || fade < 1.0f)
                list.image(it.cover, kPreview, shot_uv(shot_, false),
                           shot_tint(shot_).with_alpha(landed * fade), kPreviewRadius);
        }
        list.bordered_rect(cover, radius, kClear, 1.5f, kInk.with_alpha(0.16f * cover_alpha));
        list.pop_transform();
    }

    void draw_buy_box(gfx::DrawList &list, std::uint32_t glass, float content) const
    {
        if (content <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(page_item_);
        const Offer &o = offer(page_item_);
        const bool mine = owned(page_item_);
        const bool carted = !mine && in_cart(page_item_);
        const float rise = 30.0f * (1.0f - content) * travel();
        Rect box = kBuyBox;
        box.y += rise;

        list.push_opacity(content);
        list.shadow({box.x, box.y + 20.0f, box.w, box.h}, 30, 50, kBlack.with_alpha(0.45f));
        // Frosted glass: the blurred screen, a tint, then a hairline of light.
        list.glass(glass, box, 30, kWhite);
        list.rounded_rect(box, 30, gfx::mix(kPanel, it.mid, 0.12f).with_alpha(0.6f));
        list.bordered_rect(box, 30, kClear, 1.5f, kInk.with_alpha(0.2f));

        // The price block.
        const float x = box.x + 32.0f;
        ui::text(list, fonts.semibold, "PRICE", x, box.y + 52.0f, 16, kInk.with_alpha(0.55f),
                 gfx::Align::left, 3.5f);
        const float line = box.y + 130.0f;
        const float note = box.y + 172.0f;
        const Color quiet = kInk.with_alpha(0.62f);
        if (mine)
        {
            draw_check(list, x + 20.0f, line - 17.0f, 34.0f, kOwned);
            ui::text(list, fonts.display, "Owned", x + 52.0f, line, 48, kOwned);
            ui::text(list, fonts.regular, "In your library, ready to play", x, note, 21, quiet);
        }
        else if (o.free())
        {
            ui::text(list, fonts.display, "Free", x - 2.0f, line, 56, kInk);
            ui::text(list, fonts.regular, "No credits needed", x, note, 21, quiet);
        }
        else if (o.percent > 0)
        {
            const float w = amount(list, o.price(), x, line, 56, kAccent);
            struck(list, o.base, x + w + 20.0f, line, 26, kInk.with_alpha(0.5f));
            char text[8];
            std::snprintf(text, sizeof(text), "-%d%%", o.percent);
            const float badge = fonts.semibold.measure(text, 21) + 28.0f;
            discount_badge(list, o.percent, box.x + box.w - 32.0f - badge, box.y + 46.0f, 35.0f,
                           1.0f);
            float cursor = x + ui::text(list, fonts.regular, "You save ", x, note, 21, quiet);
            char saved[24];
            format_credits(saved, sizeof(saved), o.base - o.price());
            cursor += ui::text(list, fonts.mono, saved, cursor, note, 20, kInk.with_alpha(0.86f));
            ui::text(list, fonts.regular, " credits", cursor, note, 21, quiet);
        }
        else
        {
            amount(list, o.price(), x, line, 56, kInk);
            ui::text(list, fonts.regular, "Credits, paid once", x, note, 21, quiet);
        }

        // The primary button has three faces that cross-fade: buy, in cart,
        // owned. A press dips it; a refused press shakes it.
        const float buy = tween::clamp01(1.0f - stock_.value);
        const float waiting = tween::clamp01(1.0f - std::fabs(stock_.value - 1.0f));
        const float kept = tween::clamp01(stock_.value - 1.0f);
        Rect button = kBuyButton;
        button.y += rise;
        button.x += ui::shake(page_nudge_.value, clock_, 10.0f);
        list.push_transform(1.0f - 0.035f * press_.value, button.cx(), button.cy(), 0, 0);
        if (buy > 0.01f)
        {
            if (page_focus_ == PageFocus::buy)
                list.glow(button, kButtonRadius, 22,
                          kAccent.with_alpha((0.22f + 0.1f * breathe()) * buy));
            list.rounded_rect(button, kButtonRadius, kAccent.with_alpha(buy));
            draw_cart(list, button.x + 44.0f, button.cy(), 24.0f, kOnAccent.with_alpha(buy));
            ui::text(list, fonts.semibold, o.free() ? "Add to cart, free" : "Add to cart",
                     button.x + 78.0f, centred(button.cy(), 28), 28, kOnAccent.with_alpha(buy));
        }
        if (waiting > 0.01f)
        {
            list.bordered_rect(button, kButtonRadius, kAccent.with_alpha(0.1f * waiting), 2.0f,
                               kAccent.with_alpha(waiting));
            draw_check(list, button.x + 44.0f, button.cy(), 24.0f, kAccent.with_alpha(waiting));
            ui::text(list, fonts.semibold, "In your cart", button.x + 78.0f,
                     centred(button.cy(), 28), 28, kAccent.with_alpha(waiting));
        }
        if (kept > 0.01f)
        {
            list.rounded_rect(button, kButtonRadius, kInk.with_alpha(0.08f * kept));
            ui::text(list, fonts.semibold, "In your library", button.x + 32.0f,
                     centred(button.cy(), 28), 28, kInk.with_alpha(0.45f * kept));
        }
        if (press_.value > 0.01f)
            list.rounded_rect(button, kButtonRadius, kWhite.with_alpha(0.3f * press_.value));
        list.pop_transform();

        Rect wish = kWishButton;
        wish.y += rise;
        const bool wished = wished_[static_cast<std::size_t>(page_item_)];
        list.bordered_rect(wish, kButtonRadius, kInk.with_alpha(0.06f), 1.5f,
                           kInk.with_alpha(0.28f));
        list.star(wish.x + 44.0f, wish.cy(), 14.0f + 7.0f * wish_pop_.value,
                  wished ? kStar : kInk.with_alpha(0.86f), wished ? 0.0f : 2.2f);
        ui::text(list, fonts.semibold, wished ? "On your wishlist" : "Wishlist", wish.x + 78.0f,
                 centred(wish.cy(), 26), 26, kInk.with_alpha(0.92f));

        // A button that cannot be used says why while it has the focus.
        const char *reason = "Demo shop: no real money changes hands";
        Color reason_ink = kInk.with_alpha(0.45f);
        if (page_focus_ == PageFocus::buy && (mine || carted))
        {
            reason = mine ? "You already own this title" : "Already in your cart: Square opens it";
            reason_ink = kInk.with_alpha(0.86f);
        }
        ui::text(list, fonts.regular, reason, box.x + 32.0f, box.y + 422.0f, 20, reason_ink);

        // The page's own focus highlight glides between buttons and thumbs.
        Rect ring = page_ring_.value();
        if (page_focus_ != PageFocus::shots)
            ring.y += rise;
        ring.x += ui::shake(page_nudge_.value, clock_, 10.0f);
        list.bordered_rect(ring, page_ring_radius_.value, kClear, 3.0f, kInk.with_alpha(0.95f));
        list.pop_opacity();
    }

    // ---- drawing: the cart -------------------------------------------------

    void draw_cart_counter(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        char text[8];
        list.push_opacity(in);
        // The bump: a kicked, underdamped spring, so it overshoots and rings.
        list.push_transform(1.0f + bump_.value, kCartX, kCartY, 0, 0);
        draw_cart(list, kCartX, kCartY, 30.0f, kInk.with_alpha(0.92f));
        const float badge = badge_.value;
        if (badge > 0.01f)
        {
            const float bx = kCartX + 20.0f;
            const float by = kCartY - 18.0f;
            list.circle(bx, by, 15.0f * badge, kAccent);
            // The number rolls: the old one leaves upward as the new one
            // comes up from below. Mono digits keep the roll in place.
            const float roll =
                count_roll_.running ? tween::cubic_out(count_roll_.progress()) : 1.0f;
            const float base = centred(by, 18);
            if (roll < 1.0f && count_previous_ > 0)
            {
                std::snprintf(text, sizeof(text), "%d", count_previous_);
                ui::text(list, fonts.mono, text, bx, base - 9.0f * roll, 18,
                         kOnAccent.with_alpha((1.0f - roll) * badge), gfx::Align::center);
            }
            if (count_shown_ > 0)
            {
                std::snprintf(text, sizeof(text), "%d", count_shown_);
                ui::text(list, fonts.mono, text, bx, base + 9.0f * (1.0f - roll), 18,
                         kOnAccent.with_alpha(roll * badge), gfx::Align::center);
            }
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // The small copy of the cover on its way to the cart: a quadratic curve
    // that rises first, shrinking as it goes, with two fainter echoes behind.
    void draw_flight(gfx::DrawList &list) const
    {
        if (!flight_active_)
            return;
        const demo::Item &it = item(flight_item_);
        const float t = flight_.progress();
        if (calm())
        {
            // No flight: the copy just fades where it stands.
            const Rect still = scaled(kPreview, 0.3f);
            list.image(it.cover, still, kCoverUv, kWhite.with_alpha(1.0f - t), 16);
            return;
        }
        const float from_x = kPreview.cx(), from_y = kPreview.cy();
        const float bend_x = tween::lerp(from_x, kCartX, 0.45f), bend_y = kCartY - 30.0f;
        for (int echo = 3; echo >= 0; --echo)
        {
            const float u =
                tween::cubic_in_out(tween::clamp01(t - 0.014f * static_cast<float>(echo)));
            const float v = 1.0f - u;
            const float x = v * v * from_x + 2.0f * v * u * bend_x + u * u * kCartX;
            const float y = v * v * from_y + 2.0f * v * u * bend_y + u * u * kCartY;
            const float size = tween::lerp(190.0f, 26.0f, u);
            const float alpha = tween::clamp01(t * 10.0f) * tween::clamp01((1.0f - u) * 9.0f) *
                                (echo == 0 ? 1.0f : 0.26f / static_cast<float>(echo));
            const Rect r{x - size * 0.5f, y - size * 0.5f, size, size};
            if (echo == 0)
            {
                list.shadow({r.x, r.y + size * 0.1f, r.w, r.h}, size * 0.16f, size * 0.3f,
                            kBlack.with_alpha(0.5f * alpha));
                list.glow(r, size * 0.16f, size * 0.25f, it.accent.with_alpha(0.3f * alpha));
            }
            list.image(it.cover, r, kCoverUv, kWhite.with_alpha(alpha), size * 0.16f);
        }
    }

    // One cart row centred on cy: cover, title, genre and the price at the
    // right edge. A removed row closes like a drawer; a bought one also
    // slides away to the right, toward the library.
    void draw_line(gfx::DrawList &list, const Line &line, float x0, float cy, unsigned layers) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(line.item);
        const Offer &o = offer(line.item);
        const float open = line.open.value;
        const float slide = line.sold ? 90.0f * (1.0f - open) * travel() : 0.0f;
        const float x = x0 + 44.0f + slide;
        const float right = x0 + kDrawerW - 44.0f + slide;
        list.push_opacity(tween::clamp01(open * 1.4f - 0.4f));
        if (layers & kImages)
            list.image(it.cover, {x, cy - 38.0f, 76, 76}, kCoverUv, kWhite, 12);
        if (layers & kSemibold)
            ui::text(list, fonts.semibold, line_titles_[static_cast<std::size_t>(line.item)],
                     x + 96.0f, cy - 4.0f, 24, kInk);
        if (layers & kRegular)
            ui::text(list, fonts.regular, it.genre, x + 96.0f, cy + 26.0f, 20,
                     kInk.with_alpha(0.58f));
        if (o.free())
        {
            if (layers & kSemibold)
                ui::text(list, fonts.semibold, "Free", right, cy + 8.0f, 24, kInk,
                         gfx::Align::right);
        }
        else if (o.percent > 0)
        {
            amount(list, o.price(), right, cy - 2.0f, 24, kAccent, true, layers);
            struck(list, o.base, right, cy + 26.0f, 17, kInk.with_alpha(0.5f), true, layers);
        }
        else
        {
            amount(list, o.price(), right, cy + 8.0f, 24, kInk, true, layers);
        }
        list.pop_opacity();
    }

    void draw_drawer(gfx::DrawList &list, std::uint32_t glass) const
    {
        const float t = drawer_.value;
        if (t <= 0.004f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const float x0 = kWidth - kDrawerW * (calm() ? 1.0f : t);
        const Rect panel{x0, 0, kDrawerW + 80.0f, kHeight};
        const float right = x0 + kDrawerW - 44.0f;

        list.rounded_rect({0, 0, kWidth, kHeight}, 0, kBlack.with_alpha(0.5f * t));
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.shadow({panel.x - 10.0f, panel.y, panel.w, panel.h}, 0, 70, kBlack.with_alpha(0.55f));
        list.glass(glass, panel, 0, kWhite);
        list.rounded_rect(panel, 0, gfx::mix(kCoal, kPanel, 0.6f).with_alpha(0.74f));
        list.rounded_rect({x0, 0, 1.5f, kHeight}, 0, kInk.with_alpha(0.22f));

        const int live = live_lines();
        ui::text(list, fonts.display, "Your cart", x0 + 44.0f, 130, 42, kInk);
        if (live == 1)
            std::snprintf(text, sizeof(text), "One title");
        else if (live == 0)
            std::snprintf(text, sizeof(text), "Empty");
        else
            std::snprintf(text, sizeof(text), "%d titles", live);
        ui::text(list, fonts.regular, text, x0 + 46.0f, 168, 22, kInk.with_alpha(0.6f));

        // The focus highlight, under the rows it lights.
        Rect ring = drawer_ring_.value();
        ring.x += x0 - (kWidth - kDrawerW) + ui::shake(drawer_nudge_.value, clock_, 10.0f);
        const bool on_button = drawer_focus_ >= live;
        if (!on_button)
            list.rounded_rect(ring, 18, kInk.with_alpha(0.08f));

        // The rows, layer by layer like the grid's cards (see Layer).
        list.push_clip({x0, kLinesTop, kDrawerW, kLinesView});
        for (const unsigned layer : {kImages, kShapes, kSemibold, kRegular, kMono})
        {
            float y = kLinesTop - drawer_scroll_.offset();
            for (const Line &line : lines_)
            {
                const float height = kLineH * line.open.value;
                if (y + height > kLinesTop && y < kLinesTop + kLinesView && line.open.value > 0.02f)
                    draw_line(list, line, x0, y + height * 0.5f, layer);
                y += height;
            }
        }
        list.pop_clip();
        if (!on_button)
            list.bordered_rect(ring, 18, kClear, 2.5f, kInk.with_alpha(0.9f));

        const float empty = empty_.value;
        if (empty > 0.01f)
        {
            const float cx = x0 + kDrawerW * 0.5f;
            const float rise = 16.0f * (1.0f - empty) * travel();
            draw_cart(list, cx, 420 + rise, 64, kInk.with_alpha(0.3f * empty));
            ui::text(list, fonts.semibold, "Your cart is empty", cx, 520 + rise, 28,
                     kInk.with_alpha(0.9f * empty), gfx::Align::center);
            ui::text(list, fonts.regular, "Titles you add will wait here.", cx, 560 + rise, 24,
                     kInk.with_alpha(0.6f * empty), gfx::Align::center);
        }

        // The total counts toward its new value instead of jumping to it.
        list.rounded_rect({x0 + 44.0f, 820, kDrawerW - 88.0f, 1.5f}, 0, kInk.with_alpha(0.14f));
        ui::text(list, fonts.semibold, "SUBTOTAL", x0 + 44.0f, 872, 17, kInk.with_alpha(0.6f),
                 gfx::Align::left, 3.5f);
        amount(list, subtotal_shown(), right, 878, 40, kInk, true);

        Rect button = checkout_rect();
        button.x += x0 - (kWidth - kDrawerW);
        if (on_button)
            button.x += ui::shake(drawer_nudge_.value, clock_, 10.0f);
        if (live > 0)
        {
            if (on_button)
                list.glow(button, kButtonRadius, 22, kAccent.with_alpha(0.22f + 0.1f * breathe()));
            list.rounded_rect(button, kButtonRadius, kAccent);
            ui::text(list, fonts.semibold, "Checkout", button.cx(), centred(button.cy(), 28), 28,
                     kOnAccent, gfx::Align::center);
        }
        else
        {
            list.rounded_rect(button, kButtonRadius, kInk.with_alpha(0.08f));
            ui::text(list, fonts.semibold, "Checkout", button.cx(), centred(button.cy(), 28), 28,
                     kInk.with_alpha(0.4f), gfx::Align::center);
        }
        if (on_button)
            list.bordered_rect(button.inset(-6.0f), kButtonRadius + 6.0f, kClear, 3.0f,
                               kInk.with_alpha(0.95f));
        list.pop_opacity();
    }

    void draw_dialog(gfx::DrawList &list, std::uint32_t glass) const
    {
        const float t = dialog_.value;
        if (t <= 0.004f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[64];
        list.rounded_rect({0, 0, kWidth, kHeight}, 0, kBlack.with_alpha(0.5f * t));
        const Rect d = kDialog;
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.push_transform(calm() ? 1.0f : 0.94f + 0.06f * t, d.cx(), d.cy(), 0, 0);
        list.shadow({d.x, d.y + 24.0f, d.w, d.h}, 34, 60, kBlack.with_alpha(0.55f));
        list.glass(glass, d, 34, kWhite);
        list.rounded_rect(d, 34, gfx::mix(kCoal, kPanel, 0.7f).with_alpha(0.8f));
        list.bordered_rect(d, 34, kClear, 1.5f, kInk.with_alpha(0.22f));

        const int live = live_lines();
        ui::text(list, fonts.display, "Complete the purchase?", d.x + 56.0f, d.y + 88.0f, 42, kInk);
        if (live == 1)
            std::snprintf(text, sizeof(text), "One title joins your library.");
        else
            std::snprintf(text, sizeof(text), "%d titles join your library.", live);
        ui::text(list, fonts.regular, text, d.x + 56.0f, d.y + 140.0f, 26, kInk.with_alpha(0.8f));
        list.rounded_rect({d.x + 56.0f, d.y + 172.0f, d.w - 112.0f, 1.5f}, 0,
                          kInk.with_alpha(0.14f));
        ui::text(list, fonts.semibold, "TOTAL", d.x + 56.0f, d.y + 226.0f, 17,
                 kInk.with_alpha(0.6f), gfx::Align::left, 3.5f);
        amount(list, subtotal(), d.x + d.w - 56.0f, d.y + 232.0f, 40, kInk, true);

        // Two choices and one highlight that slides between them. It opens
        // on Cancel: spending is never the default.
        const Rect cancel{d.x + 56.0f, d.y + 284.0f, 292.0f, 68.0f};
        const Rect buy{d.x + 372.0f, d.y + 284.0f, 292.0f, 68.0f};
        const float position = dialog_position_.value;
        Rect pill = mix_rect(cancel, buy, position);
        pill.x += ui::shake(dialog_nudge_.value, clock_, 10.0f);
        list.bordered_rect(cancel, 34, kInk.with_alpha(0.05f), 1.5f, kInk.with_alpha(0.22f));
        list.bordered_rect(buy, 34, kAccent.with_alpha(0.08f), 1.5f, kAccent.with_alpha(0.6f));
        list.rounded_rect(pill, 34, gfx::mix(kInk, kAccent, position));
        ui::text(list, fonts.semibold, "Cancel", cancel.cx(), centred(cancel.cy(), 26), 26,
                 gfx::mix(kCoal, kInk.with_alpha(0.9f), position), gfx::Align::center);
        ui::text(list, fonts.semibold, "Buy now", buy.cx(), centred(buy.cy(), 26), 26,
                 gfx::mix(kAccent, kOnAccent, position), gfx::Align::center);
        list.pop_transform();
        list.pop_opacity();
    }

    // "Purchase complete": a frosted pill that drops in under the top edge,
    // with a check mark that draws itself.
    void draw_toast(gfx::DrawList &list, std::uint32_t glass) const
    {
        const float t = toast_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[64];
        if (toast_count_ == 1)
            std::snprintf(text, sizeof(text), "One title added to your library");
        else
            std::snprintf(text, sizeof(text), "%d titles added to your library", toast_count_);
        const float w = 600.0f;
        const Rect pill{960.0f - w * 0.5f, 34.0f - (calm() ? 0.0f : 110.0f * (1.0f - t)), w, 84.0f};
        list.push_opacity(tween::clamp01(t * 1.3f));
        list.shadow({pill.x, pill.y + 14.0f, pill.w, pill.h}, 42, 36, kBlack.with_alpha(0.5f));
        list.glass(glass, pill, 42, kWhite);
        list.rounded_rect(pill, 42, gfx::mix(kCoal, kPanel, 0.7f).with_alpha(0.8f));
        list.bordered_rect(pill, 42, kClear, 1.5f, kOwned.with_alpha(0.5f));
        const float cx = pill.x + 46.0f;
        const float cy = pill.cy();
        list.circle(cx, cy, 22, kOwned);
        const float stroke = tween::clamp01(toast_age_ / 0.3f);
        const float first = tween::clamp01(stroke / 0.4f);
        const float second = tween::clamp01((stroke - 0.4f) / 0.6f);
        list.line(cx - 9.5f, cy + 0.5f, cx - 9.5f + 6.5f * first, cy + 0.5f + 6.5f * first, 3.6f,
                  kCoal);
        if (second > 0.0f)
            list.line(cx - 3.0f, cy + 7.0f, cx - 3.0f + 12.5f * second, cy + 7.0f - 13.5f * second,
                      3.6f, kCoal);
        ui::text(list, fonts.semibold, "Purchase complete", pill.x + 86.0f, cy - 4.0f, 26, kInk);
        ui::text(list, fonts.regular, text, pill.x + 86.0f, cy + 24.0f, 20, kInk.with_alpha(0.66f));
        list.pop_opacity();
    }

    // The hint row of one layer: 0 home, 1 product page, 2 cart, 3 dialog.
    void draw_hints(gfx::DrawList &list, int layer, float alpha) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        list.push_opacity(alpha);
        if (layer == 0)
        {
            const char *confirm = zone_ == Zone::chips ? "Browse" : "Details";
            const ui::Hint hints[] = {{ui::Button::cross, confirm},
                                      {ui::Button::square, "Cart"},
                                      {ui::Button::l2, "Filter", ui::Button::r2}};
            ui::draw_hints(list, fonts, style, hints, 3, kRight, true);
        }
        else if (layer == 1)
        {
            const char *confirm = page_focus_ == PageFocus::shots      ? "Look"
                                  : page_focus_ == PageFocus::wishlist ? "Wishlist"
                                                                       : "Add to cart";
            const ui::Hint hints[] = {{ui::Button::cross, confirm},
                                      {ui::Button::square, "Cart"},
                                      {ui::Button::circle, "Back"}};
            ui::draw_hints(list, fonts, style, hints, 3, kRight, true);
        }
        else if (layer == 2)
        {
            // Only what works from where the focus is: Triangle on a line,
            // Cross on the button, and nothing but Close in an empty cart.
            const int live = live_lines();
            const ui::Hint on_line[] = {{ui::Button::triangle, "Remove"},
                                        {ui::Button::cross, "Checkout"},
                                        {ui::Button::circle, "Close"}};
            const ui::Hint on_button[] = {{ui::Button::cross, "Checkout"},
                                          {ui::Button::circle, "Close"}};
            if (drawer_focus_ < live)
                ui::draw_hints(list, fonts, style, on_line, 3, kRight, true);
            else
                ui::draw_hints(list, fonts, style, on_button + (live > 0 ? 0 : 1), live > 0 ? 2 : 1,
                               kRight, true);
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Choose"},
                                      {ui::Button::circle, "Cancel"}};
            ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
        }
        list.pop_opacity();
    }

    // ---- state -------------------------------------------------------------

    app::Context &context_;
    std::vector<Offer> offers_; // per catalogue entry
    std::vector<bool> owned_;
    std::vector<bool> wished_;
    std::vector<std::string> card_titles_; // fitted to a card once
    std::vector<std::string> line_titles_; // ... and to a cart row
    std::vector<Copy> copy_;               // how each title's text wraps on its page
    std::vector<int> featured_;            // catalogue indices the banner shows
    int newest_year_ = 0;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running: idle glows and shakes

    // The home page.
    Zone zone_ = Zone::grid;
    int filter_ = kFilterAll;
    int counts_[kFilters] = {};
    std::vector<int> visible_;          // what the grid shows, in order
    std::vector<int> previous_visible_; // ... and what it showed before the filter changed
    float previous_scroll_ = 0.0f;
    tween::Timer swap_;
    int focus_ = 0; // index into visible_
    tween::Spring scroll_;
    ui::SpringRect ring_; // the focus highlight, in page coordinates
    tween::Spring ring_radius_;
    tween::Spring plate_; // 1 while the highlight is a card's plate
    ui::SpringColor glow_;
    std::vector<tween::Spring> lift_; // per catalogue entry: 1 when its card is focused
    ui::SpringRect chip_pill_;
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    ui::Pulse chip_nudge_;
    float chip_nudge_x_ = 0.0f;
    ui::SpringColor palette_[3];
    ui::SpringColor tone_; // the banner's plate

    // The banner.
    int banner_ = 0; // index into featured_
    int banner_previous_ = 0;
    float banner_direction_ = 1.0f;
    float banner_clock_ = 0.0f; // seconds this banner has been up
    tween::Timer banner_fade_;
    float drift_ = 0.0f;          // seconds of artwork drift, current banner
    float drift_previous_ = 0.0f; // ... and the one fading out
    tween::Spring banner_focus_;

    // The product page.
    bool page_open_ = false;
    bool page_from_banner_ = false;
    int page_item_ = 0;
    tween::Spring page_;
    PageFocus page_focus_ = PageFocus::buy;
    int shot_ = 0;
    int shot_previous_ = 0;
    tween::Timer shot_fade_;
    ui::SpringRect page_ring_;
    tween::Spring page_ring_radius_;
    tween::Spring stock_; // 0 buy, 1 in cart, 2 owned: the button's face
    ui::Pulse press_;
    ui::Pulse page_nudge_;
    ui::Pulse wish_pop_;
    ui::Pulse preview_pop_;

    // The cart.
    std::vector<Line> lines_;
    bool flight_active_ = false;
    int flight_item_ = 0;
    tween::Timer flight_;
    int count_shown_ = 0; // what the counter says (it waits for the flight)
    int count_previous_ = 0;
    tween::Timer count_roll_;
    tween::Bounce bump_;
    tween::Spring badge_;
    bool drawer_open_ = false;
    tween::Spring drawer_;
    int drawer_focus_ = 0; // nth live line; the line count means the button
    ui::Scroller drawer_scroll_;
    ui::SpringRect drawer_ring_;
    ui::Pulse drawer_nudge_;
    tween::Spring empty_;   // the "cart is empty" note
    int subtotal_from_ = 0; // the count the drawer shows runs from here ...
    int subtotal_to_ = 0;   // ... to here
    tween::Timer subtotal_count_;

    // The checkout.
    bool dialog_open_ = false;
    tween::Spring dialog_;
    int dialog_choice_ = 0; // 0 Cancel, 1 Buy now
    tween::Spring dialog_position_;
    ui::Pulse dialog_nudge_;
    bool settling_ = false; // bought rows are still leaving
    int bought_ = 0;
    int toast_count_ = 0;
    float toast_left_ = 0.0f;
    float toast_age_ = 10.0f;
    tween::Spring toast_;
};

} // namespace

std::unique_ptr<app::Concept> make_store(app::Context &context)
{
    return std::make_unique<Store>(context);
}

} // namespace hui::concepts
