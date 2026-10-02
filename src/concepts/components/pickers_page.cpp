// ps5-homebrew-ui - Component Library page: the pickers, as a "create your profile" board.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every component that chooses a value from a set, in use on one board:
// Select (region, language), RadioGroup, CheckGroup, TagSelect, DatePicker,
// TimePicker, RangeSlider, ColorPicker and Slider, with a card that sums up
// the profile as it is edited.
//
// The design problem of such a board is the D-pad: it has to move between ten
// components, and most of them want it for themselves. The rule here:
//
//   - groups, chips and swatches use the directions inside and let the focus
//     out through their edges (ui::EdgeExits);
//   - sliders use left and right, so up and down move on;
//   - the wheels and the colour field use all four with no edge to leave by,
//     so they are entered with Cross and left with Circle; until then the
//     D-pad passes over them;
//   - an open Select is modal.
//
// The hint row always says which of these applies to the focused component.

#include "concepts/components/page.hpp"

#include "ui/components/choice_group.hpp"
#include "ui/components/color_picker.hpp"
#include "ui/components/range_slider.hpp"
#include "ui/components/select.hpp"
#include "ui/components/tag_select.hpp"
#include "ui/components/wheel_picker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout -----------------------------------------------------------------

constexpr float kTop = 290.0f;
constexpr float kBottom = 880.0f;
constexpr float kPad = 24.0f; // inside a panel
constexpr float kGap = 20.0f; // between components of a column
constexpr Rect kColumn[3] = {
    {96.0f, kTop, 500.0f, kBottom - kTop},
    {620.0f, kTop, 600.0f, kBottom - kTop},
    {1244.0f, kTop, 580.0f, kBottom - kTop},
};
constexpr float kTagsHeight = 182.0f;  // the title line and three rows of chips
constexpr float kColorHeight = 186.0f; // the title line and three rows of swatches
constexpr float kDateWidth = 300.0f;
constexpr float kCaption = 268.0f;
constexpr float kStatus = 930.0f;

constexpr const char *kCaptions[3] = {
    "Select, RadioGroup, CheckGroup",
    "TagSelect, DatePicker, TimePicker, RangeSlider",
    "ColorPicker, Slider, the profile so far",
};

// ---- the board --------------------------------------------------------------

enum Node
{
    kRegion,
    kLanguage,
    kStyle,
    kNotify,
    kTags,
    kDate,
    kTime,
    kRange,
    kColor,
    kVolume,
    kNodes,
};

constexpr const char *kNodeNames[kNodes] = {
    "Select",     "Select",     "RadioGroup",  "CheckGroup",  "TagSelect",
    "DatePicker", "TimePicker", "RangeSlider", "ColorPicker", "Slider",
};

// Where each direction leads from a component (-1: the board ends there).
struct Links
{
    int up, down, left, right;
};
constexpr Links kLinks[kNodes] = {
    {-1, kLanguage, -1, kTags},      // region
    {kRegion, kStyle, -1, kTags},    // language
    {kLanguage, kNotify, -1, kDate}, // play style
    {kStyle, -1, -1, kRange},        // notifications
    {-1, kDate, kRegion, kColor},    // genres
    {kTags, kRange, kStyle, kTime},  // birthday
    {kTags, kRange, kDate, kVolume}, // reminder
    {kDate, -1, kNotify, kVolume},   // play window
    {-1, kVolume, kTags, -1},        // colour
    {kColor, -1, kTime, -1},         // volume
};

constexpr const char *kRegions[] = {
    "Northern Isles", "Amber Coast",   "Lantern Pass", "Glass Harbour", "Saltmarsh",
    "High Steppe",    "Ember Reach",   "Quiet Fjords", "Old Capital",   "Windward Keys",
    "Copper Hills",   "Mistral Bay",   "Thornwood",    "Sunken Vale",   "Starfall Ridge",
    "Low Country",    "Granite Shore", "Meadowlands",  "Far Atolls",    "Iron Delta",
    "Cloud Terraces", "Outer Banks",
};
constexpr const char *kRegionNotes[] = {
    "Cold, quiet, fast servers",
    "Warm and crowded",
    "Mountain relay",
    "The newest data centre",
    "Low tide, low ping",
    "Wide open plains",
    "Late-night players",
    "Small and friendly",
    "The busiest of all",
    "Island hopping",
    "Old mines, new cables",
    "A steady sea breeze",
    "Deep forest relay",
    "Slow but scenic",
    "Clear skies most nights",
    "Flat, fast and central",
    "Hard rock, soft landing",
    "Family friendly",
    "Closed for the season",
    "Heavy industry",
    "Above the weather",
    "The edge of the map",
};
constexpr const char *kLanguages[] = {
    "English", "German",    "Spanish",  "French",    "Italian",    "Portuguese",
    "Dutch",   "Polish",    "Swedish",  "Norwegian", "Danish",     "Finnish",
    "Czech",   "Hungarian", "Romanian", "Turkish",   "Greek",      "Japanese",
    "Korean",  "Arabic",    "Hindi",    "Thai",      "Vietnamese", "Indonesian",
};
constexpr const char *kStyles[] = {"Solo", "Co-op", "Versus"};
constexpr const char *kStyleNotes[] = {"On your own", "With friends", "Against them"};
constexpr const char *kNotices[] = {"Friends", "Invites", "Updates", "Events"};
constexpr const char *kGenres[] = {
    "Action", "Puzzle", "Racing", "RPG",    "Indie",   "Co-op",
    "Horror", "Sports", "Arcade", "Rhythm", "Stealth", "Sim",
};

struct Variant
{
    const char *name;
};
constexpr Variant kVariants[] = {
    {"Lists, swatches, 24 h"},
    {"Rows, pills, HSV, 12 h, wrap"},
    {"Compact, fill, square chips"},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

// ---- hints ------------------------------------------------------------------

constexpr ui::Hint kHintsMove[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsEnter[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Enter"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsWheel[] = {
    {ui::Button::dpad, "Turn, column"},
    {ui::Button::circle, "Leave"},
};
constexpr ui::Hint kHintsField[] = {
    {ui::Button::dpad, "Adjust"},
    {ui::Button::left_stick, "Fine"},
    {ui::Button::cross, "Field / hue"},
    {ui::Button::circle, "Leave"},
};
constexpr ui::Hint kHintsList[] = {
    {ui::Button::dpad, "Pick"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};
constexpr ui::Hint kHintsSlider[] = {
    {ui::Button::dpad, "Adjust, move"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsRange[] = {
    {ui::Button::dpad, "Adjust, move"},
    {ui::Button::cross, "Other thumb"},
    {ui::Button::square, "Variant"},
};

constexpr app::TourStep kTour[] = {
    {0.6f, action_bit(Action::confirm)}, // open the region list
    {0.35f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.8f, action_bit(Action::confirm), Direction::none, "pickers-select"},
    {0.4f, 0, Direction::right}, // to the genres
    {0.3f, action_bit(Action::confirm)},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},          // out, to the birthday
    {0.3f, action_bit(Action::confirm)}, // enter the wheels
    {0.25f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::up},
    {0.9f, action_bit(Action::back), Direction::none, "pickers"}, // and leave them
    {0.3f, action_bit(Action::west)},
    {0.9f, action_bit(Action::west), Direction::none, "pickers-hsv"},
    {0.9f, action_bit(Action::west), Direction::none, "pickers-compact"},
};

std::string hours(float value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%02d:00", static_cast<int>(std::lround(value)));
    return text;
}

class PickersPage final : public Page
{
  public:
    explicit PickersPage(app::Context &context) : context_(context)
    {
        build();
        restyle(ui::default_theme(), false);
        summary_color_.snap(color_.color());
    }

    const char *title() const override
    {
        return "Pickers";
    }
    const char *summary() const override
    {
        return "Choosing from a set: Select, groups, chips, colours, wheels and ranges";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        region_.dismiss();
        language_.dismiss();
        entered_ = false;
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        if (region_.is_open() || language_.is_open())
            update_list(input, feedback);
        else if (input.is_pressed(Action::west))
            next_variant(feedback);
        else if (entered_)
            update_entered(input, feedback);
        else
            update_board(input, feedback);

        region_.set_active(focus_ == kRegion);
        language_.set_active(focus_ == kLanguage);
        style_.set_active(focus_ == kStyle);
        notify_.set_active(focus_ == kNotify);
        tags_.set_active(focus_ == kTags);
        date_.set_active(focus_ == kDate);
        date_.set_engaged(entered_);
        time_.set_active(focus_ == kTime);
        time_.set_engaged(entered_);
        range_.set_active(focus_ == kRange);
        color_.set_active(focus_ == kColor);
        color_.set_engaged(!enters(kColor) || entered_);
        volume_.set_active(focus_ == kVolume);

        region_.update(dt);
        language_.update(dt);
        style_.update(dt);
        notify_.update(dt);
        tags_.update(dt);
        date_.update(dt);
        time_.update(dt);
        range_.update(dt);
        color_.update(dt);
        volume_.update(dt);
        edge_.update(dt, 9.0f);
        summary_color_.target(color_.color());
        summary_color_.update(dt, reduced_ ? 60.0f : 12.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = reduced_ ? 0.0f : ui::shake(edge_.value, clock_, 10.0f);
        const int column = column_of(focus_);
        for (int i = 0; i < 3; ++i)
        {
            const bool active = i == column;
            paint.label(ui::fit_label(paint, kCaptions[i], 20.0f, kColumn[i].w),
                        kColumn[i].x + (active ? nudge : 0.0f), kCaption, 20.0f,
                        active ? paint.page_text() : paint.page_text_muted());
        }
        paint.panel(kColumn[0]);
        paint.panel(kColumn[1]);
        paint.panel(tools_);
        paint.panel(card_);

        region_.draw(canvas);
        language_.draw(canvas);
        style_.draw(canvas);
        notify_.draw(canvas);
        tags_.draw(canvas);
        date_.draw(canvas);
        time_.draw(canvas);
        range_.draw(canvas);
        color_.draw(canvas);
        volume_.draw(canvas);
        draw_card(canvas, paint);

        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, kColumn[1].x + kColumn[1].w - 96.0f),
                       kPageArea.x, kStatus, 22.0f, paint.page_text_muted());
        paint.body(ui::fit_body(paint, "Cross enters wheels and field, Circle leaves", 20.0f,
                                kColumn[2].w),
                   kColumn[2].x, kStatus, 20.0f, paint.page_text_muted());
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        region_.draw_popover(canvas);
        language_.draw_popover(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (region_.is_open() || language_.is_open())
            return kHintsList;
        if (entered_)
            return focus_ == kColor ? std::span<const ui::Hint>(kHintsField)
                                    : std::span<const ui::Hint>(kHintsWheel);
        if (enters(focus_))
            return kHintsEnter;
        if (focus_ == kRange)
            return kHintsRange;
        if (focus_ == kVolume)
            return kHintsSlider;
        return kHintsMove;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- content ----

    void build()
    {
        std::vector<ui::SelectOption> regions;
        for (std::size_t i = 0; i < std::size(kRegions); ++i)
        {
            ui::SelectOption option;
            option.label = kRegions[i];
            option.disabled = i == 18; // "Closed for the season"
            regions.push_back(option);
        }
        region_.set_label("Region");
        region_.set_options(std::move(regions));
        region_.set_index(1);

        std::vector<ui::SelectOption> languages;
        for (const char *name : kLanguages)
        {
            ui::SelectOption option;
            option.label = name;
            languages.push_back(option);
        }
        language_.set_label("Language");
        language_.set_options(std::move(languages));
        language_.set_index(0);

        std::vector<ui::ChoiceItem> styles;
        for (std::size_t i = 0; i < std::size(kStyles); ++i)
            styles.push_back({kStyles[i]});
        style_.set_title("Play style");
        style_.set_items(std::move(styles));
        style_.set_selected(1);

        std::vector<ui::ChoiceItem> notices;
        for (const char *name : kNotices)
            notices.push_back({name});
        notify_.set_title("Notify me about");
        notify_.set_items(std::move(notices));
        notify_.set_checked(0, true);
        notify_.set_checked(1, true);

        std::vector<ui::TagOption> genres;
        for (const char *name : kGenres)
            genres.push_back({name});
        tags_.set_title("Favourite genres");
        tags_.set_options(std::move(genres));
        tags_.set_selected(1, true);
        tags_.set_selected(4, true);

        date_.set_title("Birthday");
        date_.set_years(1950, 2030);
        date_.set_date(1998, 3, 14);
        time_.set_title("Daily reminder");
        time_.set_time(19, 30);

        range_.set_label("Play window");
        range_.set_range(0.0f, 24.0f, 1.0f);
        range_.set_min_gap(1.0f);
        range_.set_values(18.0f, 23.0f);
        range_.format = hours;

        color_.set_title("Accent colour");
        color_.set_index(8);

        volume_.set_label("Voice volume");
        volume_.set_range(0.0f, 100.0f, 5.0f);
        volume_.set_unit(" %");
        volume_.set_value(70.0f);
    }

    // The region list gains its second line only in the last variant: the
    // options are data, so they are set again, keeping the choice.
    void describe_regions(bool described)
    {
        if (described == regions_described_)
            return;
        regions_described_ = described;
        std::vector<ui::SelectOption> regions = region_.options();
        for (std::size_t i = 0; i < regions.size(); ++i)
            regions[i].description = described ? kRegionNotes[i] : "";
        region_.set_options(std::move(regions));
    }

    // Square: the same components with other knobs. Every style starts from
    // its defaults again, so a variant only names what it changes; values and
    // focus are not part of a style and survive.
    void apply_variant()
    {
        ui::SelectStyle select;
        ui::ChoiceGroupStyle radio;
        ui::ChoiceGroupStyle check;
        ui::TagSelectStyle tags;
        ui::DatePickerStyle date;
        ui::TimePickerStyle time;
        ui::SliderStyle range;
        ui::SliderStyle volume;
        ui::ColorPickerStyle color;

        const ui::EdgeExits all{true, true, true, true};
        select.label = ui::SelectLabel::inside;
        select.field_height = 56.0f;
        select.scrim = 0.25f;
        radio.exits = all;
        radio.row_height = 44.0f;
        check.exits = all;
        check.row_height = 44.0f;
        check.layout = ui::ChoiceLayout::grid;
        check.select_all = true;
        tags.exits = all;
        tags.max_selected = 5;
        // Twelve chips in three rows, also in the faces with wide capitals.
        tags.chip_padding = 14.0f;
        tags.gap = 8.0f;
        tags.text_size = 19.0f;
        tags.check_size = 14.0f;
        date.row_height = 34.0f;
        time.row_height = 34.0f;
        time.minute_step = 5;
        color.exits = all;

        switch (variant_)
        {
        case 1:
            select.label = ui::SelectLabel::above;
            select.max_rows = 8;
            select.row_height = 48.0f;
            radio.layout = ui::ChoiceLayout::horizontal;
            radio.padding = 8.0f;
            radio.box_gap = 10.0f;
            radio.column_gap = 4.0f;
            radio.label_size = 22.0f;
            check.layout = ui::ChoiceLayout::vertical;
            check.select_all = false;
            check.highlight.kind = ui::HighlightKind::bar;
            radio.highlight.kind = ui::HighlightKind::bar;
            tags.shape = ui::TagShape::pill;
            tags.align = gfx::Align::center;
            tags.max_selected = 0;
            date.wrap = true;
            date.order = ui::DateOrder::month_day_year;
            time.wrap = true;
            time.twelve_hour = true;
            range.bubble = ui::BubbleMode::focused;
            range.ticks = 13;
            volume.bubble = ui::BubbleMode::focused;
            volume.ticks = 11;
            color.kind = ui::ColorPickerKind::hsv;
            color.exits = {};
            break;
        case 2:
            select.step_closed = true;
            select.max_rows = 5;
            select.highlight.kind = ui::HighlightKind::fill;
            radio.highlight.kind = ui::HighlightKind::fill;
            radio.select_on_move = true;
            radio.row_height = 40.0f;
            check.highlight.kind = ui::HighlightKind::ring;
            check.gap = 8.0f;
            tags.shape = ui::TagShape::square;
            tags.check = false;
            tags.flow = true;
            tags.max_selected = 4;
            tags.chip_height = 40.0f;
            date.visible = 3;
            date.row_height = 44.0f;
            date.highlight.kind = ui::HighlightKind::fill;
            date.order = ui::DateOrder::year_month_day;
            date.months = ui::MonthStyle::number;
            time.visible = 3;
            time.row_height = 44.0f;
            time.highlight.kind = ui::HighlightKind::fill;
            time.minute_step = 1;
            range.bubble = ui::BubbleMode::never;
            range.end_labels = true;
            volume.bubble = ui::BubbleMode::never;
            volume.end_labels = true;
            volume.ticks = 5;
            color.columns = 9;
            color.swatch_size = 34.0f;
            color.swatch_gap = 8.0f;
            color.swatch_radius = 100.0f;
            break;
        default:
            break;
        }
        describe_regions(variant_ == 2);

        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(select);
        themed(radio);
        themed(check);
        themed(tags);
        themed(date);
        themed(time);
        themed(range);
        themed(volume);
        themed(color);
        region_.style = select;
        language_.style = select;
        style_.style = radio;
        notify_.style = check;
        tags_.style = tags;
        date_.style = date;
        time_.style = time;
        range_.style = range;
        volume_.style = volume;
        color_.style = color;
        // A picker that is not entered in this variant cannot stay entered.
        if (entered_ && !enters(focus_))
            entered_ = false;
        place();
    }

    // Stacks every column from the heights its components ask for.
    void place()
    {
        const Rect limits{kPageArea.x, kPageArea.y, kPageArea.w, kPageArea.h};
        region_.set_limits(limits);
        language_.set_limits(limits);

        // ---- first column ----
        float x = kColumn[0].x + kPad;
        float w = kColumn[0].w - 2.0f * kPad;
        float y = kTop + kPad;
        region_.set_bounds({x, y, w, region_.preferred_height()});
        y += region_.preferred_height() + 12.0f;
        language_.set_bounds({x, y, w, language_.preferred_height()});
        y += language_.preferred_height() + kGap;
        style_.set_bounds({x, y, w, 1.0f});
        style_.set_bounds({x, y, w, style_.preferred_height()});
        y += style_.preferred_height() + kGap;
        notify_.set_bounds({x, y, w, 1.0f});
        notify_.set_bounds({x, y, w, notify_.preferred_height()});

        // ---- second column ----
        x = kColumn[1].x + kPad;
        w = kColumn[1].w - 2.0f * kPad;
        y = kTop + kPad;
        tags_.set_bounds({x, y, w, kTagsHeight});
        y += kTagsHeight + kGap;
        date_.set_bounds({x, y, kDateWidth, date_.preferred_height()});
        time_.set_bounds(
            {x + kDateWidth + kGap, y, w - kDateWidth - kGap, time_.preferred_height()});
        // The wheels of the first variant are the tallest: the slider under
        // them keeps its place in the others.
        y += 30.0f + 5.0f * 34.0f + 12.0f + kGap;
        range_.set_bounds({x, y, w, range_.preferred_height()});

        // ---- third column: the tools, then the card in what is left ----
        x = kColumn[2].x + kPad;
        w = kColumn[2].w - 2.0f * kPad;
        y = kTop + kPad;
        color_.set_bounds({x, y, w, kColorHeight});
        y += kColorHeight + kGap;
        volume_.set_bounds({x, y, w, volume_.preferred_height()});
        y += volume_.preferred_height() + kPad;
        tools_ = {kColumn[2].x, kTop, kColumn[2].w, y - kTop};
        card_ = {kColumn[2].x, y + 16.0f, kColumn[2].w, kBottom - y - 16.0f};
    }

    // ---- input ----

    // Components the player enters before the D-pad edits them.
    bool enters(int node) const
    {
        return node == kDate || node == kTime ||
               (node == kColor && color_.style.kind == ui::ColorPickerKind::hsv);
    }

    static int column_of(int node)
    {
        return node <= kNotify ? 0 : (node <= kRange ? 1 : 2);
    }

    Rect rect_of(int node) const
    {
        switch (node)
        {
        case kRegion:
            return region_.bounds();
        case kLanguage:
            return language_.bounds();
        case kStyle:
            return style_.bounds();
        case kNotify:
            return notify_.bounds();
        case kTags:
            return tags_.bounds();
        case kDate:
            return date_.bounds();
        case kTime:
            return time_.bounds();
        case kRange:
            return range_.bounds();
        case kColor:
            return color_.bounds();
        default:
            return volume_.bounds();
        }
    }

    const ui::ComponentStyle &voice() const
    {
        return region_.style;
    }

    void next_variant(ui::Feedback &feedback)
    {
        variant_ = (variant_ + 1) % kVariantCount;
        apply_variant();
        ui::play_cue(feedback, voice(), voice().sounds.change);
    }

    // Moves the board's focus one component in a direction.
    void go(Direction direction, const InputFrame &input, ui::Feedback &feedback)
    {
        const Links &links = kLinks[focus_];
        const int next = direction == Direction::up     ? links.up
                         : direction == Direction::down ? links.down
                         : direction == Direction::left ? links.left
                                                        : links.right;
        const float x = rect_of(focus_).cx();
        if (next < 0)
        {
            ui::refuse(feedback, voice(), input, edge_, x);
            return;
        }
        focus_ = next;
        ui::play_cue(feedback, voice(), voice().sounds.move, rect_of(focus_).cx());
        status_ = std::string("Focus  \xC2\xB7  ") + kNodeNames[focus_];
    }

    void report(const char *name, ui::Event event, const std::string &detail)
    {
        const char *what = event == ui::Event::changed     ? "Event::changed"
                           : event == ui::Event::moved     ? "Event::moved"
                           : event == ui::Event::activated ? "Event::activated"
                           : event == ui::Event::refused   ? "Event::refused"
                           : event == ui::Event::cancelled ? "Event::cancelled"
                                                           : nullptr;
        if (what == nullptr)
            return;
        status_ = std::string(name) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    void update_list(const InputFrame &input, ui::Feedback &feedback)
    {
        ui::Select &select = region_.is_open() ? region_ : language_;
        const ui::Event event = select.handle(input, feedback);
        if (event == ui::Event::changed)
            report("Select", event, select.value());
        else if (event == ui::Event::cancelled)
            report("Select", event, "closed without a change");
        else if (event == ui::Event::refused)
            report("Select", event, "");
    }

    void update_entered(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.is_pressed(Action::back))
        {
            entered_ = false;
            ui::play_cue(feedback, voice(), voice().sounds.cancel, rect_of(focus_).cx());
            status_ = std::string(kNodeNames[focus_]) + "  \xC2\xB7  left";
            return;
        }
        if (focus_ == kDate)
        {
            const ui::Event event = date_.handle(input, feedback);
            report("DatePicker", event, date_.text());
            if (event == ui::Event::activated)
                entered_ = false;
        }
        else if (focus_ == kTime)
        {
            const ui::Event event = time_.handle(input, feedback);
            report("TimePicker", event, time_.text());
            if (event == ui::Event::activated)
                entered_ = false;
        }
        else
        {
            report("ColorPicker", color_.handle(input, feedback), color_.hex());
        }
    }

    void update_board(const InputFrame &input, ui::Feedback &feedback)
    {
        const Direction nav = input.nav;
        const bool vertical = nav == Direction::up || nav == Direction::down;
        if (enters(focus_))
        {
            if (nav != Direction::none)
            {
                go(nav, input, feedback);
            }
            else if (input.is_pressed(Action::confirm))
            {
                entered_ = true;
                ui::play_cue(feedback, voice(), voice().sounds.activate, rect_of(focus_).cx());
                status_ = std::string(kNodeNames[focus_]) + "  \xC2\xB7  entered";
            }
            return;
        }
        switch (focus_)
        {
        case kRegion:
        case kLanguage:
        {
            ui::Select &select = focus_ == kRegion ? region_ : language_;
            const bool steps =
                select.style.step_closed && (nav == Direction::left || nav == Direction::right);
            if (nav != Direction::none && !steps)
            {
                go(nav, input, feedback);
                break;
            }
            const ui::Event event = select.handle(input, feedback);
            report("Select", event, event == ui::Event::activated ? "list opened" : select.value());
            break;
        }
        case kStyle:
        {
            const ui::Event event = style_.handle(input, feedback);
            if (style_.exit() != Direction::none)
                go(style_.exit(), input, feedback);
            else
                report("RadioGroup", event,
                       style_.selected() >= 0 ? kStyles[style_.selected()] : "none");
            break;
        }
        case kNotify:
        {
            const ui::Event event = notify_.handle(input, feedback);
            if (notify_.exit() != Direction::none)
                go(notify_.exit(), input, feedback);
            else
                report("CheckGroup", event, std::to_string(notify_.checked_count()) + " checked");
            break;
        }
        case kTags:
        {
            const ui::Event event = tags_.handle(input, feedback);
            if (tags_.exit() != Direction::none)
                go(tags_.exit(), input, feedback);
            else
                report("TagSelect", event,
                       event == ui::Event::refused && nav == Direction::none
                           ? "the limit is reached"
                           : kGenres[tags_.focus()]);
            break;
        }
        case kColor:
        {
            const ui::Event event = color_.handle(input, feedback);
            if (color_.exit() != Direction::none)
                go(color_.exit(), input, feedback);
            else
                report("ColorPicker", event, color_.hex());
            break;
        }
        case kRange:
            if (vertical)
                go(nav, input, feedback);
            else
                report("RangeSlider", range_.handle(input, feedback), range_.text());
            break;
        default:
            if (vertical)
                go(nav, input, feedback);
            else
                report("Slider", volume_.handle(input, feedback), volume_.text());
            break;
        }
    }

    // ---- the card ----

    void draw_card(ui::Canvas &canvas, ui::Painter &paint) const
    {
        gfx::DrawList &list = canvas.list;
        const Rect in = card_.inset(22.0f);
        const float tile = std::min(84.0f, in.h);
        const Rect art{in.x, in.y, tile, tile};
        const Color accent = summary_color_.value();
        const float radius = std::min(theme_.radius_card, tile * 0.3f);
        paint.fill(art, radius, accent);
        paint.stroke(art, radius, std::max(theme_.border, 1.5f),
                     theme_.outline.a > 0.2f ? theme_.outline : theme_.text_muted.with_alpha(0.4f));
        const demo::Item &title = context_.catalog[0];
        const Rect cover = art.inset(10.0f);
        if (title.cover != 0)
            list.image(title.cover, cover, gfx::kCanvasUv, Color::rgb(0xffffff),
                       std::max(radius - 6.0f, 0.0f));
        else
            list.gradient_rect(cover, std::max(radius - 6.0f, 0.0f), title.mid, title.dark);

        const float x = in.x + tile + 20.0f;
        const float room = std::max(in.x + in.w - x, 60.0f);
        float y = in.y + 24.0f;
        paint.label(
            ui::fit_label(paint, region_.value() + "  \xC2\xB7  " + language_.value(), 24.0f, room),
            x, y, 24.0f, theme_.text);
        const auto line = [&](const std::string &text)
        {
            y += 28.0f;
            if (y > card_.y + card_.h - 14.0f)
                return;
            paint.body(ui::fit_body(paint, text, 21.0f, room), x, y, 21.0f, theme_.text_muted);
        };
        std::string genres;
        for (const int index : tags_.selection())
            genres += (genres.empty() ? "" : ", ") + std::string(kGenres[index]);
        const int style = style_.selected();
        line(std::string(style >= 0 ? kStyleNotes[style] : "No play style") + ", " +
             std::to_string(notify_.checked_count()) + " kinds of notice");
        line(genres.empty() ? "No genres yet" : genres);
        line("Born " + date_.text() + ", reminder " + time_.text());
        line("Plays " + range_.text() + ", voice " + volume_.text());
        // Under the tile, when the card is tall enough: the colour by name.
        if (in.h >= tile + 30.0f)
            paint.label(ui::fit_label(paint, color_.hex(), 19.0f, tile + 14.0f), art.cx(),
                        art.y + tile + 26.0f, 19.0f, theme_.text_muted, gfx::Align::center);
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::Select region_;
    ui::Select language_;
    ui::RadioGroup style_;
    ui::CheckGroup notify_;
    ui::TagSelect tags_;
    ui::DatePicker date_;
    ui::TimePicker time_;
    ui::RangeSlider range_;
    ui::ColorPicker color_;
    ui::Slider volume_;
    Rect tools_ = kColumn[2];
    Rect card_ = kColumn[2];
    int focus_ = kRegion;
    int variant_ = 0;
    bool entered_ = false;
    bool regions_described_ = false;
    float clock_ = 0.0f;
    ui::Pulse edge_;
    ui::SpringColor summary_color_;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_pickers_page(app::Context &context)
{
    return std::make_unique<PickersPage>(context);
}

} // namespace hui::concepts::gallery
