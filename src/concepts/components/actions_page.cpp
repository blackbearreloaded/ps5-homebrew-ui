// ps5-homebrew-ui - Component Library page: buttons, hold-to-confirm, banners and a tour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A board of everything a player presses: PushButton in every kind, size and
// state; IconButton with tooltips, toggles and a badge; ButtonGroup as a
// joined toolbar, a "one of" selector and a set of filters; SplitButton with
// its menu; HoldButton in its three variants; a QuickActionBar on Triangle;
// a Banner that pushes the board down; and a CoachMark tour over the board's
// own controls, started by its first button.
//
// The page moves the focus between components (up and down change rows, left
// and right walk a row); inside a group, a split button or the banner the
// component moves it and hands it back through its edge exits.

#include "concepts/components/page.hpp"

#include "ui/components/banner.hpp"
#include "ui/components/button.hpp"
#include "ui/components/button_group.hpp"
#include "ui/components/coach_mark.hpp"
#include "ui/components/hold_button.hpp"
#include "ui/components/quick_action.hpp"

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

// ---- the stops the focus can rest on ----------------------------------------
enum Stop : int
{
    kBanner,
    kPush,             // eight push buttons
    kIcon = kPush + 8, // five icon buttons
    kSplit = kIcon + 5,
    kToolbar,
    kView,
    kFilters,
    kBannerToggle,
    kBannerKind,
    kHold, // three hold buttons
    kStopCount = kHold + 3,
};
constexpr int kPushCount = 8;
constexpr int kIconCount = 5;
constexpr int kHoldCount = 3;
constexpr int kTourButton = 0;
constexpr int kSaveButton = 6;
constexpr int kBellIcon = 2;
constexpr int kRows = 5;

int row_of(int stop)
{
    if (stop == kBanner)
        return 0;
    if (stop < kIcon)
        return 1;
    if (stop <= kToolbar)
        return 2;
    if (stop < kHold)
        return 3;
    return 4;
}

// ---- layout -----------------------------------------------------------------
constexpr float kTop = 244.0f;      // where the banner lies
constexpr float kBannerGap = 18.0f; // between the banner and the board
constexpr float kRowStep = 130.0f;
constexpr float kCaption = 16.0f;  // caption baseline, from a row's top
constexpr float kControls = 34.0f; // controls, from a row's top
constexpr float kBand = 80.0f;     // the tallest control
constexpr float kStatusY = 930.0f;
constexpr float kPushWidths[kPushCount] = {260.0f, 200.0f, 150.0f, 190.0f,
                                           150.0f, 250.0f, 210.0f, 206.0f};
constexpr float kPushGap = 16.0f;
constexpr float kIconPitch = 88.0f;
constexpr float kSplitX = 560.0f;
constexpr float kSplitW = 360.0f;
constexpr float kToolbarX = 984.0f;
constexpr float kToolbarW = 840.0f;
constexpr float kViewW = 480.0f;
constexpr float kFiltersX = 610.0f;
constexpr float kFiltersW = 564.0f;
constexpr float kToggleX = 1238.0f;
constexpr float kToggleW = 270.0f;
constexpr float kKindX = 1532.0f;
constexpr float kKindW = 292.0f;
constexpr float kHoldW = 400.0f;
constexpr float kHoldPitch = 432.0f;

struct Caption
{
    float x;
    int row;
    const char *text;
    int first; // the stops it names
    int last;
};
constexpr Caption kCaptions[] = {
    {96.0f, 1, "PushButton: kinds, sizes, loading, disabled", kPush, kPush + kPushCount - 1},
    {96.0f, 2, "IconButton", kIcon, kIcon + kIconCount - 1},
    {kSplitX, 2, "SplitButton", kSplit, kSplit},
    {kToolbarX, 2, "ButtonGroup: actions", kToolbar, kToolbar},
    {96.0f, 3, "ButtonGroup: exclusive", kView, kView},
    {kFiltersX, 3, "ButtonGroup: multiple", kFilters, kFilters},
    {kToggleX, 3, "Banner", kBannerToggle, kBannerKind},
    {96.0f, 4, "HoldButton: fill, ring, underline", kHold, kHold + kHoldCount - 1},
};

// ---- content ----------------------------------------------------------------
struct Notice
{
    ui::StatusKind kind;
    const char *name;
    const char *title;
    const char *body;
    const char *first;
    const char *second; // nullptr: one action
};
constexpr Notice kNotices[] = {
    {ui::StatusKind::info, "info", "Update ready",
     "Version 2.4 adds a photo mode. It installs when you close the game.", "Install", "Notes"},
    {ui::StatusKind::success, "success", "Sync complete",
     "Your saves are up to date on this console and in the cloud.", "View", nullptr},
    {ui::StatusKind::warning, "warning", "Storage almost full",
     "2.1 GB left. New captures stop saving when it runs out.", "Manage", "Later"},
    {ui::StatusKind::danger, "danger", "Connection lost",
     "Online features are paused. Trying again in 10 seconds.", "Retry", nullptr},
};
constexpr int kNoticeCount = static_cast<int>(std::size(kNotices));

constexpr const char *kIconTips[kIconCount] = {
    "Favourite", "Mute", "Clear alerts", "Sound settings", "Not available offline",
};

constexpr const char *kVariants[] = {
    "Default",
    "Compact, joined, outlined banner",
    "Large, ring hold, filled banner",
    "Ghost toolbar, accent banner",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Press"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kMenuHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};
// The tour's hints say what the bubble says: skip, back, next, done.
constexpr ui::Hint kTourFirst[] = {
    {ui::Button::cross, "Next"},
    {ui::Button::circle, "Skip"},
};
constexpr ui::Hint kTourHints[] = {
    {ui::Button::cross, "Next"},
    {ui::Button::circle, "Back"},
};
constexpr ui::Hint kTourLast[] = {
    {ui::Button::cross, "Done"},
    {ui::Button::circle, "Back"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kWest = action_bit(Action::west);
constexpr std::uint32_t kNorth = action_bit(Action::north);

constexpr app::TourStep kTour[] = {
    // "Start tour": the first step is photographed, then skipped.
    {0.6f, kConfirm},
    {1.0f, kBack, Direction::none, "actions-tour"},
    // Triangle adds an alert, an icon toggles, the view selector picks "List".
    {0.5f, kNorth},
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    {0.3f, 0, Direction::down},
    {0.25f, 0, Direction::right},
    {0.25f, kConfirm},
    // The first hold button: photographed half held, then held to the end.
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    {0.7f, 0, Direction::none, "actions", 0.0f, 0.0f, kConfirm},
    {0.7f, 0, Direction::none, nullptr, 0.0f, 0.0f, kConfirm},
    // Compact: a tap on the second hold button shows its hint.
    {0.5f, kWest},
    {0.3f, 0, Direction::right},
    {0.3f, kConfirm},
    {0.7f, kWest, Direction::none, "actions-compact"},
    // Large: up to the split button, its chevron and its menu.
    {0.4f, 0, Direction::up},
    {0.25f, 0, Direction::up},
    {0.25f, 0, Direction::right},
    {0.3f, kConfirm},
    {0.9f, kBack, Direction::none, "actions-menu"},
    // Ghost toolbar and accent banner: a disabled icon and its tooltip, then
    // back to the default.
    {0.4f, kWest},
    {0.25f, 0, Direction::left},
    {0.25f, 0, Direction::left},
    {0.9f, kWest, Direction::none, "actions-accent"},
};

// ---- icons, drawn from shapes -------------------------------------------------
void icon_spot(gfx::DrawList &list, const Rect &box, Color ink)
{
    list.ring(box.cx(), box.cy(), box.w * 0.46f, std::max(box.w * 0.11f, 2.5f), ink);
    list.circle(box.cx(), box.cy(), box.w * 0.15f, ink);
}

void icon_star(gfx::DrawList &list, const Rect &box, Color ink, float on)
{
    list.star(box.cx(), box.cy() + box.h * 0.03f, box.w * 0.56f, ink.with_alpha(1.0f - on),
              std::max(box.w * 0.09f, 2.5f));
    if (on > 0.01f)
        list.star(box.cx(), box.cy() + box.h * 0.03f, box.w * 0.56f, ink.with_alpha(on));
}

void icon_speaker(gfx::DrawList &list, const Rect &box, Color ink, float on)
{
    const float w = box.w;
    const float cy = box.cy();
    list.rounded_rect({box.x, cy - w * 0.17f, w * 0.24f, w * 0.34f}, 2.0f, ink);
    const float cone[] = {box.x + w * 0.2f, cy - w * 0.17f, box.x + w * 0.5f, cy - w * 0.42f,
                          box.x + w * 0.5f, cy + w * 0.42f, box.x + w * 0.2f, cy + w * 0.17f};
    list.polygon(cone, 4, ink);
    const float pen = std::max(w * 0.09f, 2.5f);
    // Sound while it is off, a cross once it is muted.
    const float waves = 1.0f - on;
    if (waves > 0.01f)
    {
        list.arc(box.x + w * 0.5f, cy, w * 0.26f, pen, 0.75f, 1.64f, ink.with_alpha(waves));
        list.arc(box.x + w * 0.5f, cy, w * 0.48f, pen, 0.75f, 1.64f, ink.with_alpha(waves));
    }
    if (on > 0.01f)
    {
        const float x = box.x + w * 0.82f;
        const float reach = w * 0.15f;
        list.line(x - reach, cy - reach, x + reach, cy + reach, pen, ink.with_alpha(on));
        list.line(x + reach, cy - reach, x - reach, cy + reach, pen, ink.with_alpha(on));
    }
}

void icon_bell(gfx::DrawList &list, const Rect &box, Color ink)
{
    const float w = box.w;
    list.rounded_rect({box.cx() - w * 0.3f, box.y + w * 0.06f, w * 0.6f, w * 0.66f}, w * 0.3f, ink);
    list.rounded_rect({box.cx() - w * 0.3f, box.y + w * 0.4f, w * 0.6f, w * 0.32f}, 0.0f, ink);
    list.rounded_rect({box.cx() - w * 0.44f, box.y + w * 0.68f, w * 0.88f, w * 0.12f}, w * 0.06f,
                      ink);
    list.circle(box.cx(), box.y + w * 0.92f, w * 0.1f, ink);
}

void icon_sliders(gfx::DrawList &list, const Rect &box, Color ink)
{
    constexpr float kKnobs[3] = {0.3f, 0.72f, 0.46f};
    const float pen = std::max(box.w * 0.08f, 2.5f);
    for (int i = 0; i < 3; ++i)
    {
        const float y = box.y + box.h * (0.18f + 0.32f * static_cast<float>(i));
        list.line(box.x + pen, y, box.x + box.w - pen, y, pen, ink.with_alpha(0.55f));
        list.circle(box.x + box.w * kKnobs[i], y, box.w * 0.13f, ink);
    }
}

void icon_dots(gfx::DrawList &list, const Rect &box, Color ink)
{
    for (int i = -1; i <= 1; ++i)
        list.circle(box.cx() + static_cast<float>(i) * box.w * 0.36f, box.cy(), box.w * 0.12f, ink);
}

// Grid, list and shelf: what the view selector switches between.
void icon_view(gfx::DrawList &list, const Rect &box, int index, Color ink)
{
    const float w = box.w;
    if (index == 0)
    {
        const float cell = w * 0.42f;
        for (int i = 0; i < 4; ++i)
            list.rounded_rect({box.x + static_cast<float>(i % 2) * (w - cell),
                               box.y + static_cast<float>(i / 2) * (w - cell), cell, cell},
                              2.0f, ink);
    }
    else if (index == 1)
    {
        for (int i = 0; i < 3; ++i)
            list.rounded_rect(
                {box.x, box.y + w * (0.08f + 0.34f * static_cast<float>(i)), w, w * 0.16f}, 2.0f,
                ink);
    }
    else
    {
        for (int i = 0; i < 3; ++i)
            list.rounded_rect(
                {box.x + w * (0.02f + 0.36f * static_cast<float>(i)), box.y, w * 0.24f, w}, 2.0f,
                ink);
    }
}

float distance_to(const Rect &r, float x)
{
    if (x < r.x)
        return r.x - x;
    if (x > r.x + r.w)
        return x - (r.x + r.w);
    return 0.0f;
}

class ActionsPage final : public Page
{
  public:
    explicit ActionsPage(app::Context &context) : context_(context)
    {
        const char *labels[kPushCount] = {"Start tour", "Details", "Skip", "Delete",
                                          "Small",      "Launch",  "Save", "Locked"};
        for (int i = 0; i < kPushCount; ++i)
            push_[static_cast<std::size_t>(i)].label = labels[i];
        push_[kTourButton].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float)
        { icon_spot(canvas.list, box, ink); };
        push_[5].glyph = ui::Button::cross;
        push_[7].set_disabled(true);

        icons_[0].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float, float on)
        { icon_star(canvas.list, box, ink, on); };
        icons_[1].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float, float on)
        { icon_speaker(canvas.list, box, ink, on); };
        icons_[2].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float, float)
        { icon_bell(canvas.list, box, ink); };
        icons_[3].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float, float)
        { icon_sliders(canvas.list, box, ink); };
        icons_[4].icon = [](ui::Canvas &canvas, const Rect &box, Color ink, float, float)
        { icon_dots(canvas.list, box, ink); };
        for (int i = 0; i < kIconCount; ++i)
        {
            icons_[static_cast<std::size_t>(i)].tip = kIconTips[i];
            icons_[static_cast<std::size_t>(i)].set_tip_bounds(kPageArea);
        }
        icons_[4].set_disabled(true);
        icons_[kBellIcon].set_badge(alerts_);

        split_.label = "Save";
        std::vector<ui::MenuItem> alternatives(3);
        alternatives[0].label = "Save as copy";
        alternatives[1].label = "Save and quit";
        alternatives[2].label = "Export";
        split_.set_alternatives(std::move(alternatives));
        split_.set_menu_bounds(kPageArea);

        std::vector<ui::GroupItem> tools = {{"Cut"}, {"Copy"}, {"Paste"}, {"Undo"}};
        tools[2].disabled = true; // nothing copied yet
        toolbar_.set_items(std::move(tools));
        view_.set_items({{"Grid"}, {"List"}, {"Shelf"}});
        view_.icon = [](ui::Canvas &canvas, const Rect &box, const ui::GroupItem &, int index,
                        Color ink, float) { icon_view(canvas.list, box.inset(3.0f), index, ink); };
        filters_.set_items({{"Owned"}, {"Online"}, {"Starred"}});

        const char *holds[kHoldCount] = {"Delete save", "Reset progress", "Sign out"};
        const char *hints[kHoldCount] = {"Hold to delete", "Hold to reset", "Hold to sign out"};
        for (int i = 0; i < kHoldCount; ++i)
        {
            holds_[static_cast<std::size_t>(i)].label = holds[i];
            holds_[static_cast<std::size_t>(i)].hint = hints[i];
        }

        quick_.set_actions({{"Add alert", ui::Button::triangle, Action::north}});
        quick_.set_bounds(kPageArea);
        banner_kind_.label = "Kind";
        coach_.set_bounds({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});

        restyle(ui::default_theme(), false);
        view_.set_selected(0);
        filters_.set_selected(1);
        apply_notice();
        banner_.set_shown(true, true);
        layout();
    }

    const char *title() const override
    {
        return "Actions";
    }
    const char *summary() const override
    {
        return "Buttons, groups, hold to confirm, quick actions, a banner, a tour";
    }
    const char *variant() const override
    {
        return kVariants[variant_];
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        age_ = 0.0f;
        coach_.dismiss();
        split_.dismiss();
        for (ui::HoldButton &hold : holds_)
            hold.reset();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        age_ += dt;
        if (coach_.is_open())
            update_coach(input, feedback);
        else if (split_.menu_open())
            report_split(split_.handle(input, feedback));
        else
            update_board(input, feedback);

        if (saving_ > 0.0f)
        {
            saving_ -= dt;
            if (saving_ <= 0.0f)
            {
                push_[kSaveButton].set_loading(false);
                push_[kSaveButton].label = "Save";
                say("PushButton", "set_loading(false)", "saved");
            }
        }
        banner_toggle_.label = banner_.is_shown() ? "Hide banner" : "Show banner";
        // The banner left with the focus in it: the button that brings it back.
        if (stop_ == kBanner && !banner_.is_shown())
            stop_ = kBannerToggle;

        const bool modal = coach_.is_open() || split_.menu_open();
        for (int i = 0; i < kPushCount; ++i)
            push_[static_cast<std::size_t>(i)].set_active(!modal && stop_ == kPush + i);
        for (int i = 0; i < kIconCount; ++i)
            icons_[static_cast<std::size_t>(i)].set_active(!modal && stop_ == kIcon + i);
        for (int i = 0; i < kHoldCount; ++i)
            holds_[static_cast<std::size_t>(i)].set_active(!modal && stop_ == kHold + i);
        split_.set_active(stop_ == kSplit && !coach_.is_open());
        toolbar_.set_active(!modal && stop_ == kToolbar);
        view_.set_active(!modal && stop_ == kView);
        filters_.set_active(!modal && stop_ == kFilters);
        banner_toggle_.set_active(!modal && stop_ == kBannerToggle);
        banner_kind_.set_active(!modal && stop_ == kBannerKind);
        banner_.set_active(!modal && stop_ == kBanner);

        banner_.update(dt);
        layout();
        for (ui::PushButton &button : push_)
            button.update(dt);
        for (ui::IconButton &button : icons_)
            button.update(dt);
        for (ui::HoldButton &hold : holds_)
            hold.update(dt);
        split_.update(dt);
        toolbar_.update(dt);
        view_.update(dt);
        filters_.update(dt);
        banner_toggle_.update(dt);
        banner_kind_.update(dt);
        quick_.update(dt);
        coach_.update(dt);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);

        banner_.draw(canvas);
        for (int row = 1; row < kRows; ++row)
        {
            const float arrive = reduced_ ? tween::cubic_out(age_ / 0.2f)
                                          : tween::stagger(age_, row - 1, 0.05f, 0.34f);
            list.push_opacity(arrive);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, reduced_ ? 0.0f : 18.0f * (1.0f - arrive));
            for (const Caption &caption : kCaptions)
            {
                if (caption.row != row)
                    continue;
                const bool active = stop_ >= caption.first && stop_ <= caption.last;
                paint.label(caption.text, caption.x + (active ? nudge : 0.0f),
                            row_top(row) + kCaption, 20.0f,
                            active ? paint.page_text() : paint.page_text_muted());
            }
            switch (row)
            {
            case 1:
                for (const ui::PushButton &button : push_)
                    button.draw(canvas);
                break;
            case 2:
                for (const ui::IconButton &button : icons_)
                    button.draw(canvas);
                split_.draw(canvas);
                toolbar_.draw(canvas);
                break;
            case 3:
                view_.draw(canvas);
                filters_.draw(canvas);
                banner_toggle_.draw(canvas);
                banner_kind_.draw(canvas);
                break;
            default:
                for (const ui::HoldButton &hold : holds_)
                    hold.draw(canvas);
                break;
            }
            list.pop_transform();
            list.pop_opacity();
        }

        const Rect pill = quick_.rect(canvas.fonts, 0);
        paint.label("QuickActionBar", kPageArea.x + kPageArea.w, pill.y - 22.0f, 20.0f,
                    paint.page_text_muted(), gfx::Align::right);
        quick_.draw(canvas);
        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, 1200.0f), kPageArea.x, kStatusY, 22.0f,
                       paint.page_text_muted());

        // Last: the things that float over the board.
        for (const ui::IconButton &button : icons_)
            button.draw_tooltip(canvas);
        split_.draw_menu(canvas);
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        coach_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (coach_.is_open())
        {
            if (coach_.step() == 0)
                return kTourFirst;
            return coach_.step() + 1 >= coach_.count() ? kTourLast : kTourHints;
        }
        if (split_.menu_open())
            return kMenuHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- layout ----
    float row_top(int row) const
    {
        return board_top_ + static_cast<float>(row - 1) * kRowStep;
    }
    Rect band(int row, float x, float w) const
    {
        return {x, row_top(row) + kControls, w, kBand};
    }

    void layout()
    {
        banner_.set_bounds({kPageArea.x, kTop, kPageArea.w, 0.0f});
        // The banner pushes the board down as it slides in. Measured once a
        // frame here: draw() only reads the result.
        const float pushed = banner_.pushed(context_.fonts);
        board_top_ = kTop + pushed + kBannerGap * tween::clamp01(pushed / 60.0f);
        float x = kPageArea.x;
        for (int i = 0; i < kPushCount; ++i)
        {
            push_[static_cast<std::size_t>(i)].set_bounds(band(1, x, kPushWidths[i]));
            x += kPushWidths[i] + kPushGap;
        }
        const Rect icons = band(2, kPageArea.x, kIconPitch);
        for (int i = 0; i < kIconCount; ++i)
        {
            // As wide as the button itself, so the row starts on the margin.
            ui::IconButton &button = icons_[static_cast<std::size_t>(i)];
            button.set_bounds(
                {icons.x + static_cast<float>(i) * kIconPitch, icons.y, button.style.size, kBand});
        }
        split_.set_bounds(band(2, kSplitX, kSplitW));
        toolbar_.set_bounds(band(2, kToolbarX, kToolbarW));
        view_.set_bounds(band(3, kPageArea.x, kViewW));
        filters_.set_bounds(band(3, kFiltersX, kFiltersW));
        banner_toggle_.set_bounds(band(3, kToggleX, kToggleW));
        banner_kind_.set_bounds(band(3, kKindX, kKindW));
        for (int i = 0; i < kHoldCount; ++i)
            holds_[static_cast<std::size_t>(i)].set_bounds(
                band(4, kPageArea.x + static_cast<float>(i) * kHoldPitch, kHoldW));
    }

    // Where a stop is on screen, for moving between rows and for the tour.
    Rect stop_rect(int stop) const
    {
        if (stop == kBanner)
            return banner_.rect(context_.fonts);
        if (stop < kIcon)
            return push_[static_cast<std::size_t>(stop - kPush)].rect(context_.fonts);
        if (stop < kSplit)
            return icons_[static_cast<std::size_t>(stop - kIcon)].rect();
        switch (stop)
        {
        case kSplit:
            return split_.rect();
        case kToolbar:
            return toolbar_.rect();
        case kView:
            return view_.rect();
        case kFilters:
            return filters_.rect();
        case kBannerToggle:
            return banner_toggle_.rect(context_.fonts);
        case kBannerKind:
            return banner_kind_.rect(context_.fonts);
        default:
            return holds_[static_cast<std::size_t>(stop - kHold)].rect();
        }
    }

    ui::ButtonGroup *group_at(int stop)
    {
        if (stop == kToolbar)
            return &toolbar_;
        if (stop == kView)
            return &view_;
        return stop == kFilters ? &filters_ : nullptr;
    }

    // The x of the thing that has the focus inside the current stop.
    float focus_x()
    {
        if (const ui::ButtonGroup *group = group_at(stop_))
            return group->item_rect(group->focus()).cx();
        if (stop_ == kSplit)
            return split_.part_rect(split_.part()).cx();
        if (stop_ == kBanner)
            return banner_.control_rect(context_.fonts, banner_.focus()).cx();
        return stop_rect(stop_).cx();
    }

    bool usable(int stop) const
    {
        return stop != kBanner || (banner_.is_shown() && banner_.controls() > 0);
    }

    // The stop a direction leads to, or -1 at an edge of the board.
    int neighbour(Direction direction)
    {
        if (direction == Direction::left || direction == Direction::right)
        {
            const int next = stop_ + (direction == Direction::right ? 1 : -1);
            if (next < 0 || next >= kStopCount || row_of(next) != row_of(stop_))
                return -1;
            return next;
        }
        const float x = focus_x();
        const int step = direction == Direction::down ? 1 : -1;
        for (int row = row_of(stop_) + step; row >= 0 && row < kRows; row += step)
        {
            int best = -1;
            float nearest = 1e9f;
            for (int stop = 0; stop < kStopCount; ++stop)
            {
                if (row_of(stop) != row || !usable(stop))
                    continue;
                const float distance = distance_to(stop_rect(stop), x);
                if (distance < nearest)
                {
                    nearest = distance;
                    best = stop;
                }
            }
            if (best >= 0)
                return best;
        }
        return -1;
    }

    void move(Direction direction, const InputFrame &input, ui::Feedback &feedback)
    {
        const float x = focus_x();
        const int next = neighbour(direction);
        if (next < 0)
        {
            ui::refuse(feedback, style_, input, edge_, x);
            return;
        }
        // A component entered from the side takes the focus on that side;
        // from above or below, on whatever is nearest.
        const bool sideways = direction == Direction::left || direction == Direction::right;
        const bool from_left = direction == Direction::right;
        if (ui::ButtonGroup *group = group_at(next))
        {
            int index = from_left ? 0 : group->count() - 1;
            if (!sideways)
            {
                float nearest = 1e9f;
                for (int i = 0; i < group->count(); ++i)
                {
                    const float distance = distance_to(group->item_rect(i), x);
                    if (distance < nearest)
                    {
                        nearest = distance;
                        index = i;
                    }
                }
            }
            group->set_focus(index);
        }
        else if (next == kSplit)
        {
            if (sideways)
                split_.set_part(from_left ? 0 : 1);
            else
                split_.set_part(x >= split_.part_rect(1).x ? 1 : 0);
        }
        else if (next == kBanner)
        {
            banner_.set_focus(sideways && !from_left ? banner_.controls() - 1 : 0);
        }
        stop_ = next;
        ui::play_cue(feedback, style_, style_.sounds.move, focus_x());
    }

    // ---- input ----
    void say(const char *who, const char *what, const std::string &detail = {})
    {
        status_ = std::string(who) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    void update_board(const InputFrame &input, ui::Feedback &feedback)
    {
        // The quick action belongs to Triangle, wherever the focus is.
        if (quick_.handle(input, feedback) == ui::Event::activated)
        {
            ++alerts_;
            icons_[kBellIcon].set_badge(alerts_);
            quick_.at(0).set_badge(alerts_);
            say("QuickActionBar", "Event::activated", "fired() = 0");
            return;
        }
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, style_, style_.sounds.change);
            return;
        }

        if (ui::ButtonGroup *group = group_at(stop_))
        {
            const ui::Event event = group->handle(input, feedback);
            if (group->exit() != Direction::none)
                move(group->exit(), input, feedback);
            else
                report_group(*group, event);
            return;
        }
        if (stop_ == kSplit)
        {
            const ui::Event event = split_.handle(input, feedback);
            if (split_.exit() != Direction::none)
                move(split_.exit(), input, feedback);
            else
                report_split(event);
            return;
        }
        if (stop_ == kBanner)
        {
            const ui::Event event = banner_.handle(input, feedback);
            if (banner_.exit() != Direction::none)
                move(banner_.exit(), input, feedback);
            else
                report_banner(event, feedback);
            return;
        }
        if (input.nav != Direction::none)
        {
            move(input.nav, input, feedback);
            return;
        }
        if (stop_ >= kHold)
        {
            ui::HoldButton &hold = holds_[static_cast<std::size_t>(stop_ - kHold)];
            const ui::Event event = hold.handle(input, feedback);
            if (event == ui::Event::activated)
                say("HoldButton", "Event::activated", hold.label);
            else if (event == ui::Event::refused)
                say("HoldButton", "Event::refused", "a tap shows the hint");
            return;
        }
        if (stop_ >= kIcon && stop_ < kSplit)
        {
            const int index = stop_ - kIcon;
            ui::IconButton &button = icons_[static_cast<std::size_t>(index)];
            const ui::Event event = button.handle(input, feedback);
            if (event == ui::Event::changed)
                say("IconButton", "Event::changed", button.on() ? "on() = true" : "on() = false");
            else if (event == ui::Event::refused)
                say("IconButton", "Event::refused", kIconTips[index]);
            else if (event == ui::Event::activated)
            {
                if (index == kBellIcon)
                {
                    alerts_ = 0;
                    button.set_badge(0);
                    quick_.at(0).set_badge(0);
                }
                say("IconButton", "Event::activated", kIconTips[index]);
            }
            return;
        }
        ui::PushButton &button =
            stop_ == kBannerToggle
                ? banner_toggle_
                : (stop_ == kBannerKind ? banner_kind_
                                        : push_[static_cast<std::size_t>(stop_ - kPush)]);
        const ui::Event event = button.handle(input, feedback);
        if (event == ui::Event::refused)
            say("PushButton", "Event::refused", button.loading() ? "loading" : "disabled");
        else if (event == ui::Event::activated)
            pushed(button, feedback);
    }

    void pushed(ui::PushButton &button, ui::Feedback &feedback)
    {
        say("PushButton", "Event::activated", button.label);
        if (stop_ == kPush + kTourButton)
        {
            start_tour(feedback);
        }
        else if (stop_ == kPush + kSaveButton)
        {
            button.set_loading(true);
            button.label = "Saving";
            saving_ = 2.2f;
        }
        else if (stop_ == kBannerToggle)
        {
            if (banner_.is_shown())
                banner_.hide(feedback);
            else
                banner_.show(feedback);
        }
        else if (stop_ == kBannerKind)
        {
            notice_ = (notice_ + 1) % kNoticeCount;
            apply_notice();
            banner_.show(feedback);
        }
    }

    void report_group(const ui::ButtonGroup &group, ui::Event event)
    {
        if (event == ui::Event::none || event == ui::Event::moved)
            return;
        const std::string &label = group.items()[static_cast<std::size_t>(group.focus())].label;
        if (event == ui::Event::refused)
        {
            say("ButtonGroup", "Event::refused", label);
            return;
        }
        if (&group == &toolbar_)
        {
            // Copying or cutting gives Paste something to do.
            if (group.focus() < 2)
                toolbar_.item(2).disabled = false;
            say("ButtonGroup", "Event::activated", label);
            return;
        }
        char text[64];
        if (&group == &view_)
            std::snprintf(text, sizeof(text), "selected() = %d", group.selected());
        else
            std::snprintf(text, sizeof(text), "%s is %s", label.c_str(),
                          group.is_selected(group.focus()) ? "on" : "off");
        say("ButtonGroup", "Event::changed", text);
    }

    void report_split(ui::Event event)
    {
        if (event == ui::Event::activated)
        {
            char text[96];
            if (split_.choice() < 0)
                std::snprintf(text, sizeof(text), "choice() = -1, \"%s\"", split_.label.c_str());
            else
                std::snprintf(
                    text, sizeof(text), "choice() = %d, \"%s\"", split_.choice(),
                    split_.menu().items()[static_cast<std::size_t>(split_.choice())].label.c_str());
            say("SplitButton", "Event::activated", text);
        }
        else if (event == ui::Event::cancelled)
        {
            say("SplitButton", "Event::cancelled", "the menu closed");
        }
        else if (event == ui::Event::refused)
        {
            say("SplitButton", "Event::refused");
        }
    }

    void report_banner(ui::Event event, ui::Feedback &feedback)
    {
        if (event == ui::Event::activated)
        {
            const int choice = banner_.choice();
            say("Banner", "Event::activated",
                "\"" + banner_.actions()[static_cast<std::size_t>(choice)] + "\"");
            // The second action is the "not now" of every notice.
            if (choice == 1)
                banner_.hide(feedback);
        }
        else if (event == ui::Event::cancelled)
        {
            say("Banner", "Event::cancelled", "dismissed");
        }
        else if (event == ui::Event::refused)
        {
            say("Banner", "Event::refused");
        }
    }

    void start_tour(ui::Feedback &feedback)
    {
        using P = ui::TooltipPlacement;
        // A hard shadow is part of a button: the spotlight takes it in.
        const float shadow = theme_.style == ui::SurfaceStyle::hard ? theme_.shadow_offset : 0.0f;
        const auto span = [this, shadow](int first, int last)
        {
            Rect all = stop_rect(first);
            for (int stop = first + 1; stop <= last; ++stop)
            {
                const Rect r = stop_rect(stop);
                const float bottom = std::max(all.y + all.h, r.y + r.h);
                all.y = std::min(all.y, r.y);
                all.h = bottom - all.y;
                all.w = r.x + r.w - all.x;
            }
            all.w += shadow;
            all.h += shadow;
            return all;
        };
        std::vector<ui::CoachStep> steps;
        steps.push_back({span(kPush, kPush + kPushCount - 1), "Buttons",
                         "Four kinds and three sizes. A loading button refuses input until "
                         "its work is done.",
                         P::below});
        steps.push_back({span(kIcon, kIcon + kIconCount - 1), "Icon buttons",
                         "Their names live in tooltips. Two are toggles and the bell "
                         "carries a count.",
                         P::below});
        steps.push_back({span(kSplit, kToolbar), "Groups",
                         "A toolbar is one stop for the focus. The chevron opens a menu "
                         "of alternatives.",
                         P::below});
        steps.push_back({span(kHold, kHold + kHoldCount - 1), "Hold to confirm",
                         "Keep the button down until it is full. A tap only shows a hint.",
                         P::above});
        steps.push_back({quick_.rect(context_.fonts, 0), "Quick action",
                         "Triangle fires it from anywhere on the page.", P::left});
        coach_.start(std::move(steps), feedback);
    }

    void update_coach(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = coach_.handle(input, feedback);
        char text[48];
        std::snprintf(text, sizeof(text), "step() = %d of %d", coach_.step() + 1, coach_.count());
        if (event == ui::Event::changed)
            say("CoachMark", "Event::changed", text);
        else if (event == ui::Event::activated)
            say("CoachMark", "Event::activated", "the tour was finished");
        else if (event == ui::Event::cancelled)
            say("CoachMark", "Event::cancelled", "the tour was skipped");
    }

    // ---- style ----
    void apply_notice()
    {
        const Notice &notice = kNotices[notice_];
        banner_.style.kind = notice.kind;
        banner_.title = notice.title;
        banner_.body = notice.body;
        std::vector<std::string> actions = {notice.first};
        if (notice.second != nullptr)
            actions.emplace_back(notice.second);
        banner_.set_actions(std::move(actions));
        banner_kind_.label = std::string("Kind: ") + notice.name;
    }

    template <typename Style> void themed(Style &style) const
    {
        style.theme = theme_;
        style.reduced_motion = reduced_;
    }

    // Every variant starts from the components' defaults, so what it does not
    // name is the default. State (focus, selections, what is open) is kept.
    void apply_variant()
    {
        themed(style_);
        const ui::ButtonSize size =
            variant_ == 1 ? ui::ButtonSize::small
                          : (variant_ == 2 ? ui::ButtonSize::large : ui::ButtonSize::medium);
        const ui::EdgeExits all{true, true, true, true};

        // ---- push buttons: the kinds and sizes stay, they are the point ----
        const ui::ButtonRole roles[kPushCount] = {
            ui::ButtonRole::primary,   ui::ButtonRole::secondary, ui::ButtonRole::ghost,
            ui::ButtonRole::danger,    ui::ButtonRole::secondary, ui::ButtonRole::primary,
            ui::ButtonRole::secondary, ui::ButtonRole::secondary,
        };
        for (int i = 0; i < kPushCount; ++i)
        {
            ui::PushButtonStyle style;
            themed(style);
            style.role = roles[i];
            style.glyph_tinted = variant_ != 2;
            style.spinner = variant_ == 1 ? ui::SpinnerKind::dots : ui::SpinnerKind::arc;
            if (i == 4)
                style.size = ui::ButtonSize::small;
            else if (i == 5)
                style.size = ui::ButtonSize::large;
            push_[static_cast<std::size_t>(i)].style = style;
        }
        ui::PushButtonStyle plain;
        themed(plain);
        plain.role = ui::ButtonRole::secondary;
        // Their labels are long: they follow the variant down, not up.
        plain.size = variant_ == 1 ? ui::ButtonSize::small : ui::ButtonSize::medium;
        banner_toggle_.style = plain;
        banner_kind_.style = plain;

        // ---- icon buttons ----
        for (int i = 0; i < kIconCount; ++i)
        {
            ui::IconButtonStyle style;
            themed(style);
            style.size = variant_ == 1 ? 56.0f : (variant_ == 2 ? 72.0f : 64.0f);
            style.shape = variant_ == 1 ? ui::IconButtonShape::round : ui::IconButtonShape::square;
            style.toggle = i < 2;
            style.role = variant_ == 3 ? ui::ButtonRole::ghost : ui::ButtonRole::secondary;
            style.tip_inverted = variant_ != 2;
            style.tip_placement =
                variant_ == 2 ? ui::TooltipPlacement::below : ui::TooltipPlacement::above;
            icons_[static_cast<std::size_t>(i)].style = style;
        }

        // ---- split button ----
        {
            ui::SplitButtonStyle style;
            themed(style);
            style.size = size;
            style.exits = all;
            style.role = variant_ == 3 ? ui::ButtonRole::secondary : ui::ButtonRole::primary;
            style.swap_on_choose = variant_ == 3;
            split_.style = style;
        }

        // ---- groups ----
        {
            ui::ButtonGroupStyle style;
            themed(style);
            style.size = size;
            style.exits = all;
            style.joined = variant_ != 2;
            style.padding = 14.0f; // the labels are centred: all the room is theirs
            style.role = variant_ == 3 ? ui::ButtonRole::ghost : ui::ButtonRole::secondary;
            toolbar_.style = style;

            style.role = ui::ButtonRole::secondary;
            style.mode = ui::GroupMode::exclusive;
            style.joined = variant_ < 2;
            // Large and apart, the labels need the room the icons took.
            style.icon_width = variant_ == 1 ? 22.0f : (variant_ == 2 ? 0.0f : 26.0f);
            view_.style = style;

            style.mode = ui::GroupMode::multiple;
            style.joined = variant_ == 1 || variant_ == 3;
            style.icon_width = 0.0f;
            style.selected_role = variant_ == 3 ? ui::ButtonRole::danger : ui::ButtonRole::primary;
            filters_.style = style;
        }

        // ---- hold buttons ----
        const ui::HoldVariant kinds[kHoldCount] = {
            ui::HoldVariant::fill,
            ui::HoldVariant::ring,
            ui::HoldVariant::underline,
        };
        const ui::ButtonRole hold_roles[kHoldCount] = {
            ui::ButtonRole::danger,
            ui::ButtonRole::secondary,
            ui::ButtonRole::primary,
        };
        for (int i = 0; i < kHoldCount; ++i)
        {
            ui::HoldButtonStyle style;
            themed(style);
            style.size = size;
            style.role = hold_roles[i];
            style.variant = kinds[i];
            if (variant_ == 2)
                style.variant = ui::HoldVariant::ring;
            else if (variant_ == 3)
                style.variant = ui::HoldVariant::underline;
            style.hold_seconds = variant_ == 1 ? 0.9f : 1.2f;
            style.glyph_tinted = variant_ != 2;
            holds_[static_cast<std::size_t>(i)].style = style;
        }

        // ---- quick action ----
        {
            ui::QuickActionBarStyle style;
            themed(style);
            style.collapse_after = variant_ == 1 ? 2.5f : 0.0f;
            style.height = variant_ == 1 ? 48.0f : (variant_ == 2 ? 64.0f : 56.0f);
            style.glyph_size = variant_ == 1 ? 28.0f : (variant_ == 2 ? 36.0f : 32.0f);
            style.role = variant_ == 2 ? ui::ButtonRole::primary : ui::ButtonRole::secondary;
            style.glyph_tinted = variant_ != 2;
            quick_.style = style;
            if (variant_ == 1)
                quick_.at(0).expand();
        }

        // ---- banner ----
        {
            ui::BannerStyle style;
            themed(style);
            style.kind = kNotices[notice_].kind;
            style.exits = all;
            constexpr ui::BannerLook looks[kVariantCount] = {
                ui::BannerLook::tinted,
                ui::BannerLook::outlined,
                ui::BannerLook::filled,
                ui::BannerLook::accent,
            };
            style.look = looks[variant_];
            if (variant_ == 1)
            {
                style.padding = 16.0f;
                style.icon_size = 32.0f;
                style.min_height = 68.0f;
                style.title_size = 22.0f;
                style.body_size = 19.0f;
                style.button_height = 40.0f;
                style.button_text = 18.0f;
                style.close_size = 38.0f;
            }
            banner_.style = style;
        }

        // ---- coach mark ----
        {
            ui::CoachMarkStyle style;
            themed(style);
            // Tight enough to leave the captions over the rows in the dark.
            style.spot_padding = 8.0f;
            if (variant_ == 1)
            {
                style.width = 400.0f;
                style.padding = 20.0f;
                style.title_size = 24.0f;
                style.text_size = 20.0f;
                style.spot_padding = 6.0f;
            }
            else if (variant_ == 2)
            {
                style.scrim = 0.78f;
                style.ring_width = 5.0f;
                style.counter = false;
            }
            coach_.style = style;
        }
        layout();
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the board's own cues
    std::array<ui::PushButton, kPushCount> push_;
    std::array<ui::IconButton, kIconCount> icons_;
    ui::SplitButton split_;
    ui::ButtonGroup toolbar_;
    ui::ButtonGroup view_;
    ui::ButtonGroup filters_;
    ui::PushButton banner_toggle_;
    ui::PushButton banner_kind_;
    std::array<ui::HoldButton, kHoldCount> holds_;
    ui::QuickActionBar quick_;
    ui::Banner banner_;
    ui::CoachMark coach_;
    ui::Pulse edge_;
    int stop_ = kPush;
    int variant_ = 0;
    int notice_ = 2;
    int alerts_ = 3;
    float saving_ = 0.0f;
    float board_top_ = kTop;
    float clock_ = 0.0f;
    float age_ = 0.0f;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_actions_page(app::Context &context)
{
    return std::make_unique<ActionsPage>(context);
}

} // namespace hui::concepts::gallery
