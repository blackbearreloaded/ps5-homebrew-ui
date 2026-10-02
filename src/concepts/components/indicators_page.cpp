// ps5-homebrew-ui - Component Library page: the indicators, alive on one dashboard.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Indicators show state, so the page gives them state to show: a pretend
// download that runs, stalls now and then, finishes and starts over; a signal
// that swings and spikes; a score and a clock that tick. Seven captioned
// groups hold every component of the group. Four things take input (the
// rating and one button under each of the lower groups); one focus ring
// glides between them. Square restyles everything at once: spinner kind, bar
// and ring geometry, counter style, rating glyph, badge fill.

#include "concepts/components/page.hpp"

#include "ui/components/badge.hpp"
#include "ui/components/counter.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/rating.hpp"
#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout: two rows of panels, a row of buttons under the second ----
constexpr float kCaption1 = 268.0f; // caption baselines
constexpr float kCaption2 = 610.0f;
constexpr float kRow1 = 284.0f;
constexpr float kRow1Height = 288.0f;
constexpr float kRow2 = 626.0f;
constexpr float kRow2Height = 236.0f;
constexpr float kButtonY = 884.0f;
constexpr float kButtonWidth = 220.0f;
constexpr float kButtonHeight = 56.0f;
constexpr float kPad = 22.0f;
constexpr float kNarrow = 414.0f; // four columns in the first row
constexpr float kWide = 560.0f;   // three in the second
constexpr float kTile = 268.0f;   // two tiles share a wide column
constexpr float kColumns4[4] = {96.0f, 534.0f, 972.0f, 1410.0f};
constexpr float kColumns3[3] = {96.0f, 680.0f, 1264.0f};
constexpr float kInner = kNarrow - 2.0f * kPad;

constexpr const char *kCaptions[7] = {
    "Progress bars",
    "Rings and spinners",
    "Meters",
    "Rating and counters",
    "Badges, chips, avatars",
    "Stat tiles",
    "Skeleton and empty state",
};

// What can take the focus. The three buttons sit in a row; the rating is
// above them, so up and down change rows and left and right stay free for
// the rating's own value.
enum Item : int
{
    kAdd,
    kRandomise,
    kLoad,
    kRating,
    kItems,
};
constexpr int kButtons = 3;
constexpr const char *kButtonNotes[kButtons] = {
    "Pops when it changes",
    "New values and series",
    "Cross-fades to content",
};

struct Variant
{
    const char *name;
    ui::SpinnerKind spinner;
    float bar_height;
    ui::RadiusSource radius;
    ui::TrackStyle track;
    bool sheen;
    float ring_thickness;
    int ring_ticks;
    ui::RingCaps caps;
    bool rolling;
    ui::CounterFace face;
    ui::RatingGlyph glyph;
    float rating_step;
    ui::BadgeFill badge_fill;
    bool statuses; // bars, rings and badges take the status colours
    ui::SkeletonKind skeleton;
    ui::AvatarShape avatar;
};
constexpr Variant kVariants[] = {
    {"Standard", ui::SpinnerKind::arc, 14.0f, ui::RadiusSource::theme, ui::TrackStyle::well, true,
     12.0f, 0, ui::RingCaps::theme, false, ui::CounterFace::heading, ui::RatingGlyph::star, 1.0f,
     ui::BadgeFill::solid, false, ui::SkeletonKind::list_row, ui::AvatarShape::circle},
    {"Slim, hearts, rolling digits", ui::SpinnerKind::dots, 8.0f, ui::RadiusSource::pill,
     ui::TrackStyle::flat, false, 7.0f, 12, ui::RingCaps::round, true, ui::CounterFace::heading,
     ui::RatingGlyph::heart, 1.0f, ui::BadgeFill::tinted, false, ui::SkeletonKind::card,
     ui::AvatarShape::circle},
    {"Chunky, dots, outlined badges", ui::SpinnerKind::bars, 22.0f, ui::RadiusSource::square,
     ui::TrackStyle::well, false, 20.0f, 0, ui::RingCaps::flat, false, ui::CounterFace::label,
     ui::RatingGlyph::dot, 1.0f, ui::BadgeFill::outline, false, ui::SkeletonKind::list_row,
     ui::AvatarShape::rounded},
    {"Status colours, half stars", ui::SpinnerKind::orbit, 14.0f, ui::RadiusSource::theme,
     ui::TrackStyle::none, true, 12.0f, 8, ui::RingCaps::theme, true, ui::CounterFace::label,
     ui::RatingGlyph::star, 0.5f, ui::BadgeFill::solid, true, ui::SkeletonKind::card,
     ui::AvatarShape::rounded},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Press"},
    {ui::Button::square, "Variant"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kSquare = action_bit(Action::west);
constexpr app::TourStep kTour[] = {
    {1.6f, 0, Direction::right},
    {0.4f, 0, Direction::left},
    {0.4f, 0, Direction::left},
    {0.9f, 0, Direction::down, "indicators"},
    {0.4f, kConfirm},
    {0.5f, 0, Direction::left},
    {0.3f, kConfirm},
    {0.4f, 0, Direction::left},
    {0.3f, kConfirm},
    {0.25f, kConfirm},
    {0.25f, kConfirm},
    {1.0f, kSquare, Direction::none, "indicators-loaded"},
    {1.3f, kSquare, Direction::none, "indicators-slim"},
    {1.3f, kSquare, Direction::none, "indicators-chunky"},
    {1.3f, kSquare, Direction::none, "indicators-status"},
    {0.4f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.3f, kConfirm},
    {0.4f, 0, Direction::up},
    {0.3f, 0, Direction::right},
};

constexpr const char *kPeople[] = {
    "Mara Voss", "Tobi Okafor", "Lin Chen", "Ada Brandt", "Rui Sato", "Noor Haddad", "Eli Marsh",
};

class IndicatorsPage final : public Page
{
  public:
    explicit IndicatorsPage(app::Context &context) : context_(context)
    {
        layout();
        fill();
        restyle(ui::default_theme(), false);
        highlight_.snap(item_rect(focus_));
    }

    const char *title() const override
    {
        return "Indicators";
    }
    const char *summary() const override
    {
        return "Progress, meters, badges, ratings, counters and placeholders, in motion";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        const auto base = [&](ui::ComponentStyle &style)
        {
            style.theme = theme;
            style.reduced_motion = reduced_motion;
        };
        base(base_);
        for (ui::ProgressBar &bar : bars_)
            base(bar.style);
        for (ui::ProgressRing &ring : rings_)
            base(ring.style);
        for (ui::Spinner &spinner : spinners_)
            base(spinner.style);
        base(featured_.style);
        for (ui::Meter &meter : meters_)
            base(meter.style);
        base(rating_.style);
        base(review_.style);
        for (ui::Counter &counter : counters_)
            base(counter.style);
        base(inbox_.style);
        for (ui::Badge &badge : badges_)
            base(badge.style);
        for (ui::Chip &chip : chips_)
            base(chip.style);
        for (ui::Avatar &avatar : avatars_)
            base(avatar.style);
        base(stack_.style);
        for (ui::StatTile &tile : tiles_)
            base(tile.style);
        base(skeleton_.style);
        base(empty_.style);
        apply_variant();
    }

    void enter() override
    {
        age_ = 0.0f;
        empty_.enter();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        age_ += dt;
        const bool for_rating =
            focus_ == kRating &&
            (input.nav == Direction::left || input.nav == Direction::right ||
             input.is_pressed(Action::confirm) || input.is_pressed(Action::back));
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, base_, base_.sounds.change);
        }
        else if (for_rating)
            rating_.handle(input, feedback);
        else if (input.nav != Direction::none)
            move(input, feedback);
        else if (input.is_pressed(Action::confirm))
            press(feedback);

        advance(dt);
        place_small(); // counts changed, so widths may have

        for (ui::ProgressBar &bar : bars_)
            bar.update(dt);
        for (ui::ProgressRing &ring : rings_)
            ring.update(dt);
        for (ui::Spinner &spinner : spinners_)
            spinner.update(dt);
        featured_.update(dt);
        for (ui::Meter &meter : meters_)
            meter.update(dt);
        rating_.update(dt);
        review_.update(dt);
        for (ui::Counter &counter : counters_)
            counter.update(dt);
        inbox_.update(dt);
        for (ui::Badge &badge : badges_)
            badge.update(dt);
        for (ui::Chip &chip : chips_)
            chip.update(dt);
        for (ui::Avatar &avatar : avatars_)
            avatar.update(dt);
        stack_.update(dt);
        for (ui::StatTile &tile : tiles_)
            tile.update(dt);
        skeleton_.update(dt);
        empty_.update(dt);

        highlight_.target(item_rect(focus_));
        highlight_.update(dt, base_);
        for (ui::Pulse &pulse : pressed_)
            pulse.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        // Back to front: captions and panels, the focus ring, the content.
        // The ring goes under the content because some themes' rings light
        // the area inside them; a bevelled theme draws its ring inside the
        // control, so there it must come last.
        constexpr bool kPanels[7] = {true, true, true, true, true, false, false};
        for (int group = 0; group < 7; ++group)
        {
            const bool first_row = group < 4;
            const float x = first_row ? kColumns4[group] : kColumns3[group - 4];
            begin(canvas, group);
            paint.label(kCaptions[group], x, first_row ? kCaption1 : kCaption2, 20.0f,
                        paint.page_text_muted());
            if (kPanels[group])
                paint.panel({x, first_row ? kRow1 : kRow2, first_row ? kNarrow : kWide,
                             first_row ? kRow1Height : kRow2Height});
            end(canvas);
        }
        ui::HighlightStyle ring;
        ring.kind = ui::HighlightKind::ring;
        const bool inside = theme_.style == ui::SurfaceStyle::bevel;
        if (!inside)
            highlight_.draw(canvas, base_, ring, tween::cubic_out(age_ / 0.4f));
        draw_bars(canvas);
        draw_rings(canvas, paint);
        draw_meters(canvas);
        draw_numbers(canvas, paint);
        draw_small(canvas);
        draw_stats(canvas);
        draw_loading(canvas);
        draw_buttons(canvas, paint);
        if (inside)
            highlight_.draw(canvas, base_, ring, tween::cubic_out(age_ / 0.4f));
    }

    std::span<const ui::Hint> hints() const override
    {
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- set-up ------------------------------------------------------------

    void layout()
    {
        const float x1 = kColumns4[0] + kPad;
        bars_[0].set_bounds({x1, 306.0f, kInner, 44.0f});
        bars_[1].set_bounds({x1, 360.0f, kInner, 24.0f});
        bars_[2].set_bounds({x1, 400.0f, kInner, 44.0f});
        bars_[3].set_bounds({x1, 458.0f, kInner, 30.0f});
        bars_[4].set_bounds({x1, 504.0f, kInner, 44.0f});
        featured_.set_bounds({x1 + kInner - 24.0f, 496.0f, 24.0f, 24.0f});

        const float x2 = kColumns4[1] + kPad;
        for (int i = 0; i < 3; ++i)
            rings_[static_cast<std::size_t>(i)].set_bounds(
                {x2 + static_cast<float>(i) * 133.0f, 304.0f, 104.0f, 104.0f});
        for (int i = 0; i < 4; ++i)
            spinners_[static_cast<std::size_t>(i)].set_bounds(
                {x2 + kInner * (static_cast<float>(i) + 0.5f) / 4.0f - 22.0f, 464.0f, 44.0f,
                 44.0f});

        const float x3 = kColumns4[2] + kPad;
        meters_[0].set_bounds({x3, 304.0f, 156.0f, 156.0f});
        meters_[1].set_bounds({x3 + 182.0f, 310.0f, kInner - 182.0f, 54.0f});
        meters_[2].set_bounds({x3 + 182.0f, 392.0f, kInner - 182.0f, 54.0f});
        meters_[3].set_bounds({x3, 492.0f, kInner, 56.0f});

        const float x4 = kColumns4[3] + kPad;
        rating_.set_bounds({x4 + 12.0f, 312.0f, 250.0f, 44.0f});
        review_.set_bounds({x4, 376.0f, 150.0f, 28.0f});
        counters_[0].set_bounds({x4, 440.0f, kInner, 52.0f});
        counters_[1].set_bounds({x4, 518.0f, 170.0f, 34.0f});
        counters_[2].set_bounds({x4 + 190.0f, 518.0f, kInner - 190.0f, 34.0f});

        for (int i = 0; i < 2; ++i)
            tiles_[static_cast<std::size_t>(i)].set_bounds(
                {kColumns3[1] + static_cast<float>(i) * (kTile + 24.0f), kRow2, kTile,
                 kRow2Height});
        skeleton_.set_bounds({kColumns3[2], kRow2, kTile, kRow2Height});
        empty_.set_bounds({kColumns3[2] + kTile + 24.0f, kRow2, kTile, kRow2Height});
    }

    void fill()
    {
        bars_[1].label = "Stream";
        bars_[3].label = "Level 7";
        bars_[4].label = "Waiting for players";
        rings_[1].label = "";
        rings_[1].center = [this](ui::Canvas &canvas, const Rect &inner, float)
        {
            // A slot: the page draws a play mark where the number would be.
            const float size = inner.w * 0.5f;
            canvas.list.triangle({inner.cx() - size * 0.4f, inner.cy() - size * 0.5f, size, size},
                                 theme_.text, 0.0f, 1.5708f);
        };
        rings_[2].label = "Heat";
        meters_[0].label = "Load";
        meters_[1].label = "Signal";
        meters_[2].label = "Level";
        meters_[3].label = "Engine";
        for (ui::Meter &meter : meters_)
            meter.set_value(0.4f, true);

        rating_.set_value(4.0f, true);
        review_.set_value(context_.catalog[0].rating, true);
        counters_[0].set_value(12480.0, true);
        counters_[1].set_value(0.0, true);
        counters_[2].set_value(0.0, true);

        inbox_.set_count(3);
        badges_[0].set_text("NEW");
        badges_[1].set_count(3);
        badges_[2].set_text("LIVE");
        badges_[3].set_text("v1.4");
        badges_[4].set_count(12);
        chips_[0].label = "Online";
        chips_[1].label = "Co-op";
        chips_[2].label = "Racing";
        chips_[3].label = "4 players";
        chips_[1].set_selected(true, true);
        avatars_[0].set_name(kPeople[0]);
        avatars_[0].set_presence(ui::Presence::online);
        avatars_[1].set_name(kPeople[1]);
        avatars_[1].set_presence(ui::Presence::busy);
        avatars_[2].set_name(context_.catalog[1].studio);
        avatars_[2].set_image(context_.catalog[1].cover);
        avatars_[2].set_presence(ui::Presence::away);
        std::vector<ui::AvatarPerson> people;
        for (const char *name : kPeople)
            people.push_back({name});
        stack_.set_people(std::move(people));

        tiles_[0].label = "Players online";
        tiles_[1].label = "Frame time";
        randomise(true);

        skeleton_.content = [this](ui::Canvas &canvas, const Rect &area, float)
        { draw_content(canvas, area); };
        empty_.title = "No saves yet";
        empty_.body = "Finish a chapter to see it here.";
        empty_.action = "New game";
        restart_download();
    }

    // Badges and chips are as wide as their text, and that depends on the
    // theme's face: their rows are laid out again whenever a width may have
    // changed. Measuring needs a painter, and a painter needs a list to draw
    // into; nothing is drawn into this one.
    void place_small()
    {
        ui::Painter paint(scratch_, context_.fonts, theme_);
        const float left = kColumns3[0] + kPad;
        float x = left;
        for (ui::Badge &badge : badges_)
        {
            const float width = badge.width(paint);
            badge.set_bounds({x, 652.0f, width, 30.0f});
            x += width + (badge.style.dot ? 18.0f : 12.0f);
        }
        x = left;
        chips_shown_ = 0;
        for (ui::Chip &chip : chips_)
        {
            // A wide face may not fit them all: the row ends with the panel.
            const float width = chip.width(paint);
            if (x + width > kColumns3[0] + kWide - kPad)
                break;
            chip.set_bounds({x, 704.0f, width, 40.0f});
            x += width + 12.0f;
            ++chips_shown_;
        }
        x = left;
        for (ui::Avatar &avatar : avatars_)
        {
            avatar.set_bounds({x, 772.0f, 64.0f, 64.0f});
            x += 64.0f + 18.0f;
        }
        stack_.set_bounds({x + 24.0f, 778.0f, 260.0f, 52.0f});
        const Rect add = button_rect(kAdd);
        inbox_.set_bounds({add.x + add.w - 26.0f, add.y - 14.0f, 40.0f, 30.0f});
    }

    // The same components, another set of knobs.
    void apply_variant()
    {
        const Variant &v = kVariants[variant_];
        using ui::Status;

        for (ui::ProgressBar &bar : bars_)
        {
            bar.style.height = v.bar_height;
            bar.style.radius_source = v.radius;
            bar.style.track = v.track;
            bar.style.sheen = false;
        }
        bars_[0].style.sheen = v.sheen;
        bars_[1].style.mode = ui::ProgressMode::buffered;
        bars_[1].style.placement = ui::LabelPlacement::right;
        bars_[1].style.status = v.statuses ? Status::success : Status::accent;
        bars_[2].style.mode = ui::ProgressMode::segmented;
        bars_[2].style.percent = false;
        bars_[2].style.status = v.statuses ? Status::warning : Status::primary;
        bars_[3].style.placement = ui::LabelPlacement::inside;
        bars_[3].style.height = std::max(v.bar_height + 8.0f, 26.0f);
        bars_[3].style.text_size = 18.0f;
        // Text inside a bar needs a track to read on.
        bars_[3].style.track = ui::TrackStyle::well;
        bars_[3].style.status = v.statuses ? Status::danger : Status::accent;
        bars_[4].style.mode = ui::ProgressMode::indeterminate;
        featured_.style.kind = v.spinner;

        for (ui::ProgressRing &ring : rings_)
        {
            ring.style.thickness = v.ring_thickness;
            ring.style.caps = v.caps;
            ring.style.track = v.track == ui::TrackStyle::none ? ui::TrackStyle::flat : v.track;
            ring.style.ticks = 0;
            ring.style.start_angle = 0.0f;
            ring.style.sweep = 6.2831853f;
        }
        rings_[0].style.mode =
            variant_ == 2 ? ui::ProgressMode::segmented : ui::ProgressMode::determinate;
        rings_[0].style.status = v.statuses ? Status::success : Status::accent;
        rings_[1].style.mode = ui::ProgressMode::buffered;
        rings_[1].style.status = v.statuses ? Status::warning : Status::accent;
        rings_[2].style.two_tone = true;
        rings_[2].style.ticks = v.ring_ticks;
        rings_[2].style.status = v.statuses ? Status::danger : Status::accent;
        // The tip fades toward the text colour: a gradient in any palette.
        rings_[2].style.color_to =
            gfx::mix(ui::status_color(theme_, rings_[2].style.status), theme_.text, 0.55f);
        if (variant_ == 3)
        {
            // A ring that is not a whole turn is a gauge.
            rings_[2].style.start_angle = -2.3561945f;
            rings_[2].style.sweep = 4.712389f;
        }

        constexpr ui::SpinnerKind kKinds[4] = {ui::SpinnerKind::arc, ui::SpinnerKind::dots,
                                               ui::SpinnerKind::bars, ui::SpinnerKind::orbit};
        constexpr Status kTones[4] = {Status::success, Status::warning, Status::danger,
                                      Status::accent};
        for (std::size_t i = 0; i < 4; ++i)
        {
            spinners_[i].style.kind = kKinds[i];
            spinners_[i].style.status = v.statuses ? kTones[i] : Status::accent;
            spinners_[i].style.speed = variant_ == 1 ? 0.7f : 1.0f;
            spinners_[i].style.size = variant_ == 2 ? 44.0f : 38.0f;
        }

        meters_[0].style.shape = ui::MeterShape::radial;
        meters_[0].style.thickness = v.ring_thickness + 2.0f;
        meters_[0].style.text_size = 18.0f;
        meters_[1].style.height = v.bar_height;
        meters_[1].style.radius_source = v.radius;
        meters_[2].style.segments = variant_ == 2 ? 8 : 12;
        meters_[2].style.height = std::max(v.bar_height, 14.0f);
        meters_[2].style.zone_strip = false;
        meters_[3].style.height = v.bar_height;
        meters_[3].style.radius_source = v.radius;
        meters_[3].style.value_scale = 9000.0f;
        meters_[3].style.unit = " rpm";
        meters_[3].style.zone_strip = variant_ != 1;

        rating_.style.glyph = v.glyph;
        rating_.style.step = v.rating_step;
        rating_.style.size = 40.0f;
        rating_.style.gap = 10.0f;
        rating_.style.show_value = true;
        rating_.style.value_size = 26.0f;
        rating_.style.value_gap = 32.0f; // clear of the page's focus ring
        rating_.style.value_decimals = v.rating_step < 1.0f ? 1 : 0;
        rating_.style.status =
            v.glyph == ui::RatingGlyph::heart
                ? Status::danger
                : (v.glyph == ui::RatingGlyph::dot ? Status::accent : Status::warning);
        review_.style = rating_.style;
        review_.style.interactive = false;
        review_.style.size = 24.0f;
        review_.style.gap = 5.0f;
        review_.style.value_size = 21.0f;
        review_.style.value_decimals = 1;

        for (ui::Counter &counter : counters_)
        {
            counter.style.rolling = v.rolling;
            counter.style.face = v.face;
        }
        counters_[0].style.size = 44.0f;
        counters_[0].style.suffix = "pts";
        counters_[0].style.rate = 0.7f;
        counters_[1].style.format = ui::CounterFormat::time;
        counters_[1].style.size = 28.0f;
        counters_[1].style.rate = 1.0f; // a clock's second must land within its second
        counters_[2].style.format = ui::CounterFormat::percent;
        counters_[2].style.size = 28.0f;
        counters_[2].style.decimals = 0;
        counters_[2].style.rate = 1.0f;

        inbox_.style.kind = Status::danger;
        inbox_.style.cutout = 3.0f;
        inbox_.style.backing = Color{theme_.page.r, theme_.page.g, theme_.page.b, 1.0f};
        constexpr Status kKindsA[5] = {Status::primary, Status::success, Status::danger,
                                       Status::neutral, Status::warning};
        constexpr Status kKindsB[5] = {Status::danger, Status::warning, Status::success,
                                       Status::primary, Status::neutral};
        for (std::size_t i = 0; i < badges_.size(); ++i)
        {
            // Five pills, then two dots.
            if (i < 5)
                badges_[i].style.kind = v.statuses ? kKindsB[i] : kKindsA[i];
            badges_[i].style.fill = v.badge_fill;
            badges_[i].style.align = gfx::Align::left;
        }
        badges_[5].style.dot = true;
        badges_[5].style.kind = Status::success;
        badges_[6].style.dot = true;
        badges_[6].style.pulse = true;
        badges_[6].style.kind = Status::danger;
        chips_[0].style.leading_dot = true;
        chips_[2].style.removable = true;
        chips_[3].style.leading_dot = v.statuses;
        chips_[3].style.dot = Status::warning;
        for (ui::Avatar &avatar : avatars_)
            avatar.style.shape = v.avatar;
        stack_.style.shape = v.avatar;

        tiles_[0].style.rolling = v.rolling;
        tiles_[0].style.delta_note = "this week";
        tiles_[1].style.rolling = v.rolling;
        tiles_[1].style.format = ui::CounterFormat::decimal;
        tiles_[1].style.suffix = "ms";
        tiles_[1].style.up_is_good = false;
        tiles_[1].style.spark_status = v.statuses ? Status::warning : Status::primary;
        tiles_[1].style.spark_dot = variant_ != 2;

        skeleton_.style.kind = v.skeleton;
        skeleton_.style.panel = true;
        skeleton_.style.padding = 20.0f;
        skeleton_.style.rows = 3;
        skeleton_.style.row_height = 52.0f;
        skeleton_.style.row_gap = 20.0f;
        skeleton_.style.lines = 2;
        skeleton_.style.picture = 0.46f;
        empty_.style.panel = true;
        empty_.style.padding = 16.0f;
        empty_.style.icon_size = 58.0f;
        empty_.style.title_size = 24.0f;
        empty_.style.body_size = 20.0f;
        empty_.style.hint_size = 20.0f;
        empty_.style.gap = 10.0f;
        empty_.style.max_lines = 2;
        place_small();
    }

    // ---- the pretend world -------------------------------------------------

    float random()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) / 16777216.0f;
    }

    void restart_download()
    {
        download_ = 0.0f;
        hold_ = 0.0f;
        stall_ = 0.0f;
        ++run_;
        // Every second run stalls once, somewhere in the middle.
        stall_at_ = run_ % 2 == 0 ? 0.35f + 0.4f * random() : 2.0f;
        rate_ = 0.1f + 0.06f * random();
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(run_)];
        review_.set_value(item.rating);
    }

    void randomise(bool snap)
    {
        std::array<float, 16> series{};
        float level = 0.5f;
        for (float &value : series)
        {
            level = std::clamp(level + (random() - 0.45f) * 0.3f, 0.05f, 1.0f);
            value = level;
        }
        tiles_[0].set_value(std::floor(8000.0f + 16000.0f * random()), snap);
        tiles_[0].set_delta(std::round((random() * 21.0f - 9.0f) * 10.0f) / 10.0f);
        tiles_[0].set_series(series);
        for (float &value : series)
        {
            level = std::clamp(level + (random() - 0.5f) * 0.4f, 0.05f, 1.0f);
            value = level;
        }
        tiles_[1].set_value(14.0 + std::round(random() * 59.0f) / 10.0, snap);
        tiles_[1].set_delta(std::round((random() * 14.0f - 7.0f) * 10.0f) / 10.0f);
        tiles_[1].set_series(series);
    }

    void advance(float dt)
    {
        // The download: runs, stalls once on some runs, rests when done.
        bool stalled = false;
        if (hold_ > 0.0f)
        {
            hold_ -= dt;
            if (hold_ <= 0.0f)
                restart_download();
        }
        else if (stall_ > 0.0f)
        {
            stall_ -= dt;
            stalled = true;
        }
        else
        {
            download_ += rate_ * dt;
            if (download_ >= stall_at_)
            {
                stall_at_ = 2.0f;
                stall_ = 2.4f;
            }
            if (download_ >= 1.0f)
            {
                download_ = 1.0f;
                hold_ = 2.2f;
            }
        }
        const bool done = download_ >= 1.0f;
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(run_)];
        bars_[0].label = done ? std::string("Installed ") + item.title
                              : (stalled ? std::string("Stalled, retrying")
                                         : std::string("Downloading ") + item.title);
        bars_[0].style.status =
            done ? ui::Status::success : (stalled ? ui::Status::warning : ui::Status::accent);
        bars_[0].set_value(download_);
        rings_[0].set_value(download_);
        const float step = std::floor(download_ * 5.0f + 0.001f);
        char text[32];
        std::snprintf(text, sizeof(text), "Step %d of 5", std::min(static_cast<int>(step) + 1, 5));
        bars_[2].label = text;
        bars_[2].set_value(done ? 1.0f : (step + 0.5f) / 5.0f);

        // A stream: the played part and, ahead of it, the loaded part.
        const float played = std::fmod(clock_ / 16.0f, 1.0f);
        bars_[1].set_value(played);
        bars_[1].set_buffer(played + 0.22f + 0.08f * std::sin(clock_ * 0.9f));
        rings_[1].set_value(played);
        rings_[1].set_buffer(played + 0.22f + 0.08f * std::sin(clock_ * 0.9f));
        bars_[3].set_value(std::fmod(0.32f + clock_ * 0.045f, 1.0f));
        rings_[2].set_value(std::fmod(clock_ / 7.0f, 1.0f));

        // A signal: a slow swing, a faster ripple, and a spike now and then.
        spike_ *= std::exp(-2.6f * dt);
        spike_timer_ -= dt;
        if (spike_timer_ <= 0.0f)
        {
            spike_ = 0.45f;
            spike_timer_ = 2.6f + 2.0f * random();
        }
        const auto signal = [&](float phase)
        {
            return tween::clamp01(0.42f + 0.26f * std::sin(clock_ * 1.1f + phase) +
                                  0.08f * std::sin(clock_ * 3.1f + phase * 2.0f) + spike_);
        };
        meters_[0].set_value(signal(0.0f));
        meters_[1].set_value(signal(1.4f));
        meters_[2].set_value(signal(2.6f));
        meters_[3].set_value(signal(4.1f));

        // Figures that tick.
        score_timer_ -= dt;
        if (score_timer_ <= 0.0f)
        {
            score_timer_ = 1.1f;
            counters_[0].set_value(counters_[0].value() + std::floor(40.0f + 900.0f * random()));
            if (random() > 0.6f)
                badges_[4].set_count(badges_[4].count() % 40 + 1);
        }
        counters_[1].set_value(std::floor(clock_));
        counters_[2].set_value(std::floor(download_ * 100.0f));

        // One person comes and goes.
        constexpr ui::Presence kCycle[4] = {ui::Presence::online, ui::Presence::away,
                                            ui::Presence::busy, ui::Presence::offline};
        avatars_[0].set_presence(kCycle[static_cast<int>(clock_ / 3.0f) % 4]);
    }

    // ---- focus -------------------------------------------------------------

    static Rect button_rect(int index)
    {
        return {kColumns3[index], kButtonY, kButtonWidth, kButtonHeight};
    }

    Rect item_rect(int item) const
    {
        if (item != kRating)
            return button_rect(item);
        const Rect &r = rating_.bounds();
        return {r.x - 12.0f, r.cy() - 29.0f, rating_.width() + 24.0f, 58.0f};
    }

    void move(const InputFrame &input, ui::Feedback &feedback)
    {
        int next = -1;
        if (focus_ == kRating)
            next = input.nav == Direction::down ? last_button_ : -1;
        else if (input.nav == Direction::up)
            next = kRating;
        else if (input.nav == Direction::left && focus_ > 0)
            next = focus_ - 1;
        else if (input.nav == Direction::right && focus_ < kButtons - 1)
            next = focus_ + 1;
        const float x = item_rect(focus_).cx();
        if (next < 0)
        {
            ui::refuse(feedback, base_, input, highlight_.refusal(), x);
            return;
        }
        focus_ = next;
        if (focus_ != kRating)
            last_button_ = focus_;
        ui::play_cue(feedback, base_, base_.sounds.move, item_rect(focus_).cx(),
                     focus_ == kRating ? 1.05f : 0.97f);
    }

    void press(ui::Feedback &feedback)
    {
        if (focus_ == kRating)
            return;
        const float x = button_rect(focus_).cx();
        pressed_[static_cast<std::size_t>(focus_)].trigger();
        if (focus_ == kAdd)
        {
            inbox_.set_count(inbox_.count() >= 104 ? 1 : inbox_.count() + 1);
            // The cue climbs with the count, like a streak.
            ui::play_cue(feedback, base_, base_.sounds.step, x,
                         0.9f + 0.03f * static_cast<float>(inbox_.count() % 12));
        }
        else if (focus_ == kRandomise)
        {
            randomise(false);
            ui::play_cue(feedback, base_, base_.sounds.activate, x);
        }
        else
        {
            skeleton_.set_loaded(!skeleton_.loaded());
            ui::play_cue(feedback, base_, base_.sounds.change, x,
                         skeleton_.loaded() ? 1.06f : 0.94f);
        }
    }

    // ---- drawing -----------------------------------------------------------

    // Groups arrive one after the other when the page opens.
    float entrance(int group) const
    {
        if (base_.reduced_motion)
            return tween::cubic_out(age_ / 0.2f);
        return tween::stagger(age_, group, 0.05f, 0.36f);
    }

    void begin(ui::Canvas &canvas, int group) const
    {
        const float shown = entrance(group);
        canvas.list.push_opacity(shown);
        canvas.list.push_transform(1.0f, 0.0f, 0.0f, 0.0f,
                                   base_.reduced_motion ? 0.0f : 16.0f * (1.0f - shown));
    }

    static void end(ui::Canvas &canvas)
    {
        canvas.list.pop_transform();
        canvas.list.pop_opacity();
    }

    void draw_bars(ui::Canvas &canvas) const
    {
        begin(canvas, 0);
        for (const ui::ProgressBar &bar : bars_)
            bar.draw(canvas);
        featured_.draw(canvas);
        end(canvas);
    }

    void draw_rings(ui::Canvas &canvas, ui::Painter &paint) const
    {
        begin(canvas, 1);
        constexpr const char *kRings[3] = {"Value", "Buffered", "Two-tone"};
        constexpr const char *kSpinners[4] = {"Arc", "Dots", "Bars", "Orbit"};
        for (std::size_t i = 0; i < rings_.size(); ++i)
        {
            rings_[i].draw(canvas);
            paint.label(kRings[i], rings_[i].bounds().cx(), 438.0f, 17.0f, theme_.text_muted,
                        gfx::Align::center);
        }
        for (std::size_t i = 0; i < spinners_.size(); ++i)
        {
            spinners_[i].draw(canvas);
            paint.label(kSpinners[i], spinners_[i].bounds().cx(), 540.0f, 17.0f, theme_.text_muted,
                        gfx::Align::center);
        }
        end(canvas);
    }

    void draw_meters(ui::Canvas &canvas) const
    {
        begin(canvas, 2);
        for (const ui::Meter &meter : meters_)
            meter.draw(canvas);
        end(canvas);
    }

    void draw_numbers(ui::Canvas &canvas, ui::Painter &paint) const
    {
        begin(canvas, 3);
        const float x = kColumns4[3] + kPad;
        rating_.draw(canvas);
        review_.draw(canvas);
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(run_)];
        const float room = kInner - 222.0f;
        paint.body(ui::fit_body(paint, item.title, 20.0f, room), x + kInner, 397.0f, 20.0f,
                   theme_.text_muted, gfx::Align::right);
        paint.label("Score", x, 434.0f, 17.0f, theme_.text_muted);
        paint.label("Session", x, 512.0f, 17.0f, theme_.text_muted);
        paint.label("Download", x + 190.0f, 512.0f, 17.0f, theme_.text_muted);
        for (const ui::Counter &counter : counters_)
            counter.draw(canvas);
        end(canvas);
    }

    void draw_small(ui::Canvas &canvas) const
    {
        begin(canvas, 4);
        for (const ui::Badge &badge : badges_)
            badge.draw(canvas);
        for (int i = 0; i < chips_shown_; ++i)
            chips_[static_cast<std::size_t>(i)].draw(canvas);
        for (const ui::Avatar &avatar : avatars_)
            avatar.draw(canvas);
        stack_.draw(canvas);
        end(canvas);
    }

    void draw_stats(ui::Canvas &canvas) const
    {
        begin(canvas, 5);
        for (const ui::StatTile &tile : tiles_)
            tile.draw(canvas);
        end(canvas);
    }

    void draw_loading(ui::Canvas &canvas) const
    {
        begin(canvas, 6);
        skeleton_.draw(canvas);
        empty_.draw(canvas);
        end(canvas);
    }

    // What the skeleton turns into: the same shapes, now with content.
    void draw_content(ui::Canvas &canvas, const Rect &area) const
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const auto cover = [&](const demo::Item &item, const Rect &r, const Rect &uv, float radius)
        {
            if (item.cover != 0)
                canvas.list.image(item.cover, r, uv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(r, radius, item.mid, item.dark);
        };
        const float radius = std::min(theme_.radius, 10.0f);
        if (skeleton_.style.kind == ui::SkeletonKind::card)
        {
            const demo::Item &item = context_.catalog[2];
            const Rect picture{area.x, area.y, area.w, area.h * skeleton_.style.picture};
            // The cover is square: show a centred band of it, undistorted.
            const float band = picture.h / picture.w;
            cover(item, picture, {0.0f, 0.5f + band * 0.5f, 1.0f, -band}, radius);
            float y = picture.y + picture.h + 34.0f;
            paint.label(ui::fit_label(paint, item.title, 22.0f, area.w), area.x, y, 22.0f,
                        theme_.text);
            y += 28.0f;
            paint.body(ui::fit_body(paint, item.genre, 19.0f, area.w), area.x, y, 19.0f,
                       theme_.text_muted);
            y += 26.0f;
            paint.body(ui::fit_body(paint, item.studio, 19.0f, area.w), area.x, y, 19.0f,
                       theme_.text_muted);
            return;
        }
        const float size = skeleton_.style.row_height;
        for (int i = 0; i < skeleton_.style.rows; ++i)
        {
            const demo::Item &item = context_.catalog[static_cast<std::size_t>(i + 2)];
            const float top = area.y + static_cast<float>(i) * (size + skeleton_.style.row_gap);
            cover(item, {area.x, top, size, size}, gfx::kCanvasUv, radius);
            const float x = area.x + size + 16.0f;
            const float room = area.w - size - 16.0f;
            paint.label(ui::fit_label(paint, item.title, 21.0f, room), x, top + 22.0f, 21.0f,
                        theme_.text);
            paint.body(ui::fit_body(paint, item.genre, 18.0f, room), x, top + 46.0f, 18.0f,
                       theme_.text_muted);
        }
    }

    void draw_buttons(ui::Canvas &canvas, ui::Painter &paint) const
    {
        const float shown = entrance(7);
        canvas.list.push_opacity(shown);
        const char *labels[kButtons] = {"Add one", "Randomise",
                                        skeleton_.loaded() ? "Unload" : "Load"};
        for (int i = 0; i < kButtons; ++i)
        {
            const Rect r = button_rect(i);
            ui::Look look;
            look.press = pressed_[static_cast<std::size_t>(i)].value;
            paint.button(r, labels[i], ui::ButtonKind::secondary, look);
            const float room = kWide - kButtonWidth - 48.0f;
            paint.body(ui::fit_body(paint, kButtonNotes[i], 20.0f, room), r.x + r.w + 24.0f,
                       r.cy() + 7.0f, 20.0f, paint.page_text_muted());
        }
        // The count sits on the button's corner, parted from it by a rim in
        // the page's colour.
        inbox_.draw(canvas);
        canvas.list.pop_opacity();
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    ui::ComponentStyle base_; // the page's own cues and its focus ring
    int variant_ = 0;
    int focus_ = kRating;
    int last_button_ = kLoad;
    float clock_ = 0.0f;
    float age_ = 0.0f;
    ui::Highlight highlight_;
    std::array<ui::Pulse, kButtons> pressed_;
    gfx::DrawList scratch_; // for measuring only

    std::array<ui::ProgressBar, 5> bars_;
    std::array<ui::ProgressRing, 3> rings_;
    std::array<ui::Spinner, 4> spinners_;
    ui::Spinner featured_;
    std::array<ui::Meter, 4> meters_;
    ui::Rating rating_;
    ui::Rating review_;
    std::array<ui::Counter, 3> counters_;
    ui::Badge inbox_;
    std::array<ui::Badge, 7> badges_;
    std::array<ui::Chip, 4> chips_;
    int chips_shown_ = 0;
    std::array<ui::Avatar, 3> avatars_;
    ui::AvatarStack stack_;
    std::array<ui::StatTile, 2> tiles_;
    ui::Skeleton skeleton_;
    ui::EmptyState empty_;

    // The pretend world's state.
    std::uint32_t rng_ = 0x2f6e2b1u;
    int run_ = -1;
    float download_ = 0.0f;
    float rate_ = 0.12f;
    float stall_at_ = 2.0f;
    float stall_ = 0.0f;
    float hold_ = 0.0f;
    float spike_ = 0.0f;
    float spike_timer_ = 1.5f;
    float score_timer_ = 1.0f;
};

} // namespace

std::unique_ptr<Page> make_indicators_page(app::Context &context)
{
    return std::make_unique<IndicatorsPage>(context);
}

} // namespace hui::concepts::gallery
