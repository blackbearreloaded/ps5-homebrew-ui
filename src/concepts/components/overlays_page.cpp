// ps5-homebrew-ui - Component Library page: Dialog, Sheet, ToastStack and Tooltip.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A board of triggers, one column per component. Cross fires the focused
// trigger: a dialog of each kind, a sheet from each edge (one holds a
// ui::ListView through the content slot), a toast of each kind. A tooltip
// follows the focus and says what the trigger does; the last column sets the
// side it prefers. While a dialog or a sheet is open it takes every input.
// Square changes the knobs of all four components together.

#include "concepts/components/page.hpp"

#include "ui/components/dialog.hpp"
#include "ui/components/list.hpp"
#include "ui/components/sheet.hpp"
#include "ui/components/toast.hpp"
#include "ui/components/tooltip.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr int kColumns = 4;
constexpr int kRows = 4;
constexpr float kColumnWidth = 280.0f;
constexpr float kColumnStep = 308.0f;
constexpr float kCaptionY = 268.0f;
constexpr float kBoardTop = 344.0f;
constexpr float kButtonHeight = 60.0f;
// Rows stand far enough apart for a tooltip to fit between them.
constexpr float kRowStep = 122.0f;
constexpr float kStatusY = 930.0f;
// Toasts get a lane of their own beside the board, so a notification never
// lands on the trigger that sent it.
constexpr Rect kLane{1344.0f, 344.0f, 480.0f, 608.0f};
// The tooltip is kept inside the board: on the last column "right" must flip.
constexpr Rect kTipBounds{72.0f, 252.0f, 1252.0f, 716.0f};
// The gallery draws its hint row over the page: keep content above it.
constexpr float kHintTop = 968.0f;

enum Column : int
{
    dialogs,
    sheets,
    toasts,
    tooltips,
};

struct Trigger
{
    const char *label; // nullptr: the column has no such row
    const char *tip;
};

constexpr const char *kCaptions[kColumns] = {
    "Dialog",
    "Sheet",
    "ToastStack",
    "Tooltip",
};

constexpr Trigger kTriggers[kColumns][kRows] = {
    {
        {"Information", "One answer under an info icon"},
        {"Confirm", "Two answers: the focus opens on the main one"},
        {"Destructive", "The focus opens on the safe answer"},
        {"Three choices", "Left and right move, the ends refuse softly"},
    },
    {
        {"Left edge", "The page draws the content through the slot"},
        {"Right edge", "A ui::ListView lives in the content slot"},
        {"Bottom edge", "A shelf of covers; back closes it"},
        {nullptr, nullptr},
    },
    {
        {"Info", "Toasts never take the focus"},
        {"Success", "Each toast chimes once, when it appears"},
        {"Warning", "More than the limit wait in a queue"},
        {"Danger", "The bar shows the time left"},
    },
    {
        {"Above", "Prefers the room above its anchor"},
        {"Below", "Prefers the room below its anchor"},
        {"Left", "Sits to the left of its anchor"},
        {"Right", "No room on the right of the board: it flips"},
    },
};

constexpr ui::TooltipPlacement kPlacements[kRows] = {
    ui::TooltipPlacement::above,
    ui::TooltipPlacement::below,
    ui::TooltipPlacement::left,
    ui::TooltipPlacement::right,
};

constexpr ui::StatusKind kToastKinds[kRows] = {
    ui::StatusKind::info,
    ui::StatusKind::success,
    ui::StatusKind::warning,
    ui::StatusKind::danger,
};
constexpr const char *kToastTitles[kRows] = {
    "Download ready",
    "Progress saved",
    "Controller battery low",
    "Connection lost",
};
constexpr const char *kToastBodies[kRows] = {
    "Lantern Pass, 2.4 GB",
    "Slot 2, chapter 8",
    "About 20 minutes left",
    "Trying again in 10 seconds",
};

constexpr ui::SheetEdge kEdges[] = {
    ui::SheetEdge::left,
    ui::SheetEdge::right,
    ui::SheetEdge::bottom,
};
constexpr const char *kSheetTitles[] = {
    "Details",
    "Up next",
    "Recently played",
};
constexpr int kSheets = 3;
constexpr int kListSheet = 1;

constexpr const char *kVariants[] = {
    "Frosted, centred",
    "Compact toasts, top-centre",
    "Solid, stacked buttons, bottom",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Open"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kModalHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kWest = action_bit(Action::west);

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.5f, kConfirm}, // the destructive dialog
    {1.0f, 0, Direction::right, "overlays"},
    {0.5f, kBack},
    {0.5f, 0, Direction::right},
    {0.3f, 0, Direction::up},
    {0.4f, kConfirm}, // the sheet with the list inside
    {1.0f, kBack, Direction::none, "overlays-sheet"},
    {0.5f, 0, Direction::right},
    {0.3f, kConfirm}, // three toasts
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    {1.0f, kWest, Direction::none, "overlays-toasts"},
    {0.4f, kConfirm}, // compact: a fourth and a fifth, one must wait
    {0.3f, 0, Direction::up},
    {0.3f, kConfirm},
    {0.8f, kWest, Direction::none, "overlays-compact"},
    {0.4f, 0, Direction::left},
    {0.3f, 0, Direction::left},
    {0.4f, kConfirm}, // solid, stacked, at the bottom
    {1.0f, kBack, Direction::none, "overlays-bottom"},
    {0.5f, kWest},
};

int rows_in(int column)
{
    int rows = 0;
    while (rows < kRows && kTriggers[column][rows].label != nullptr)
        ++rows;
    return rows;
}

Rect trigger_rect(int column, int row)
{
    return {kPageArea.x + static_cast<float>(column) * kColumnStep,
            kBoardTop + static_cast<float>(row) * kRowStep, kColumnWidth, kButtonHeight};
}

class OverlaysPage final : public Page
{
  public:
    explicit OverlaysPage(app::Context &context) : context_(context)
    {
        for (int i = 0; i < kSheets; ++i)
            sheets_[static_cast<std::size_t>(i)].set_title(kSheetTitles[i]);

        std::vector<ui::ListItem> queue;
        for (std::size_t i = 0; i < 9 && i < context_.catalog.size(); ++i)
        {
            const demo::Item &title = context_.catalog[i];
            ui::ListItem item;
            item.title = title.title;
            item.subtitle = std::string(title.genre) + " \xC2\xB7 " + title.studio;
            queue.push_back(item);
        }
        list_.set_items(std::move(queue));
        list_.leading = [this](ui::Canvas &canvas, const Rect &box, const ui::ListItem &, int index,
                               float focus)
        {
            const float size = 48.0f + 6.0f * focus;
            draw_cover(canvas, static_cast<std::size_t>(index),
                       {box.cx() - size * 0.5f, box.cy() - size * 0.5f, size, size}, 10.0f);
        };

        sheets_[0].content = [this](ui::Canvas &canvas, const Rect &area, float)
        { draw_details(canvas, area); };
        sheets_[kListSheet].content = [this](ui::Canvas &canvas, const Rect &, float)
        { list_.draw(canvas); };
        sheets_[2].content = [this](ui::Canvas &canvas, const Rect &area, float)
        { draw_shelf(canvas, area); };

        restyle(ui::default_theme(), false);
        highlight_.snap(trigger_rect(column_, row_));
    }

    const char *title() const override
    {
        return "Overlays";
    }
    const char *summary() const override
    {
        return "ui::Dialog, ui::Sheet, ui::ToastStack, ui::Tooltip";
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
        dialog_.dismiss();
        for (ui::Sheet &sheet : sheets_)
            sheet.dismiss();
        toasts_.clear(true);
        tooltip_.hide();
        status_.clear();
        list_.enter();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        age_ += dt;

        ui::Sheet *sheet = open_sheet();
        if (dialog_.is_open())
            update_dialog(input, feedback);
        else if (sheet != nullptr)
            update_sheet(*sheet, input, feedback);
        else
            update_board(input, feedback);

        // The label follows the focus, and steps aside for anything modal.
        const Trigger &trigger = kTriggers[column_][row_];
        if (modal())
        {
            tooltip_.hide();
        }
        else
        {
            tooltip_.style.placement = column_ == tooltips ? kPlacements[row_] : placement_;
            tooltip_.show(trigger_rect(column_, row_), trigger.tip);
        }

        highlight_.target(trigger_rect(column_, row_));
        highlight_.update(dt, style_);
        press_.update(dt, 8.0f);
        dialog_.update(dt);
        for (ui::Sheet &each : sheets_)
            each.update(dt);
        list_.update(dt);
        toasts_.update(dt, feedback);
        tooltip_.update(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);

        for (int column = 0; column < kColumns; ++column)
        {
            const float arrive = reduced_ ? tween::cubic_out(age_ / 0.2f)
                                          : tween::stagger(age_, column, 0.05f, 0.34f);
            const float x = trigger_rect(column, 0).x;
            list.push_opacity(arrive);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, reduced_ ? 0.0f : 20.0f * (1.0f - arrive));
            const bool active = column == column_;
            paint.label(kCaptions[column], x, kCaptionY, 20.0f,
                        active ? paint.page_text() : paint.page_text_muted());
            for (int row = 0; row < rows_in(column); ++row)
            {
                ui::Look look;
                if (active && row == row_)
                    look.press = tween::clamp01(press_.value);
                paint.button(trigger_rect(column, row), kTriggers[column][row].label,
                             ui::ButtonKind::secondary, look);
            }
            list.pop_transform();
            list.pop_opacity();
        }
        paint.label("Toasts land here", kLane.x, kCaptionY, 20.0f, paint.page_text_muted());
        // One ring for the whole board; the buttons draw none of their own.
        ui::HighlightStyle ring;
        ring.kind = ui::HighlightKind::ring;
        highlight_.draw(canvas, style_, ring, tween::cubic_out(age_ / 0.3f));

        if (!status_.empty())
            paint.body(status_, kPageArea.x, kStatusY, 22.0f, paint.page_text_muted());

        tooltip_.draw(canvas);
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        for (const ui::Sheet &sheet : sheets_)
            sheet.draw(canvas);
        dialog_.draw(canvas);
        // Toasts are above everything: they report on what just closed.
        toasts_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (modal())
            return kModalHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    bool modal() const
    {
        if (dialog_.is_open())
            return true;
        for (const ui::Sheet &sheet : sheets_)
        {
            if (sheet.is_open())
                return true;
        }
        return false;
    }

    ui::Sheet *open_sheet()
    {
        for (ui::Sheet &sheet : sheets_)
        {
            if (sheet.is_open())
                return &sheet;
        }
        return nullptr;
    }

    // Every variant starts from the components' defaults, so what it does not
    // name is the default. State (what is open, the focus) is left alone.
    void apply_variant()
    {
        style_.theme = theme_;
        style_.reduced_motion = reduced_;

        ui::DialogStyle dialog;
        ui::SheetStyle sheet;
        ui::ToastStyle toast;
        ui::TooltipStyle tip;
        ui::ListStyle rows;
        Rect toast_bounds = kLane;
        float side = 560.0f;
        float bottom = 400.0f;
        tip.delay = 0.35f;
        toast.margin = 0.0f;
        toast.width = kLane.w;
        toast.duration = 5.0f;
        toast.anchor = ui::ToastAnchor::top_right;
        placement_ = ui::TooltipPlacement::above;

        if (variant_ == 2)
        {
            dialog.frosted = false;
            dialog.buttons = ui::DialogButtons::stacked;
            dialog.align = ui::DialogAlign::bottom;
            dialog.centered = false;
            dialog.width = 680.0f;
            dialog.bottom_margin = 1080.0f - kHintTop + 24.0f;
            sheet.frosted = false;
            sheet.margin = 28.0f;
            sheet.handle = false;
            side = 520.0f;
            toast.anchor = ui::ToastAnchor::bottom_right;
            toast.progress = false;
            tip.inverted = false;
            placement_ = ui::TooltipPlacement::below;
        }
        else if (variant_ == 1)
        {
            dialog.width = 600.0f;
            dialog.padding = 32.0f;
            dialog.icon_size = 48.0f;
            dialog.title_size = 30.0f;
            dialog.body_size = 22.0f;
            dialog.button_height = 56.0f;
            dialog.scrim = 0.35f;
            sheet.padding = 28.0f;
            sheet.title_size = 28.0f;
            sheet.scrim = 0.35f;
            side = 460.0f;
            bottom = 340.0f;
            toast_bounds = {0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight};
            toast.anchor = ui::ToastAnchor::top_center;
            toast.margin = 36.0f;
            toast.width = 320.0f;
            toast.padding = 14.0f;
            toast.gap = 8.0f;
            toast.icon_size = 28.0f;
            toast.title_size = 21.0f;
            toast.body_lines = 0;
            toast.frosted = true;
            tip.text_size = 18.0f;
            tip.padding_x = 14.0f;
            tip.padding_y = 8.0f;
            tip.pointer = 0.0f;
            placement_ = ui::TooltipPlacement::below;
        }

        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        // The gallery draws its hints over the page: sheets stop above them.
        sheet.footer = gfx::kVirtualHeight - kHintTop;
        tip.max_width = 520.0f;
        // The gallery captures the blurred screen only for glass themes; in
        // the others its glass texture holds nothing worth showing.
        const bool blurred = theme_.style == ui::SurfaceStyle::glass;
        dialog.frosted = dialog.frosted && blurred;
        sheet.frosted = sheet.frosted && blurred;
        toast.frosted = toast.frosted && blurred;
        themed(dialog);
        themed(sheet);
        themed(toast);
        themed(tip);
        themed(rows);

        dialog_.style = dialog;
        for (int i = 0; i < kSheets; ++i)
        {
            ui::Sheet &target = sheets_[static_cast<std::size_t>(i)];
            target.style = sheet;
            target.style.edge = kEdges[i];
            target.style.size = kEdges[i] == ui::SheetEdge::bottom ? bottom : side;
        }
        toasts_.style = toast;
        toasts_.set_bounds(toast_bounds);
        tooltip_.style = tip;
        tooltip_.set_bounds(kTipBounds);

        rows.leading_width = 54.0f;
        rows.row_height = 78.0f;
        rows.title_size = 25.0f;
        rows.subtitle_size = 19.0f;
        rows.padding = 16.0f;
        list_.style = rows;
        list_.set_items(list_.items());
        Rect area = sheets_[kListSheet].content_rect();
        area.h = std::min(area.h, kHintTop - area.y);
        list_.set_bounds(area);
    }

    void say(const char *who, const char *what, const std::string &detail = {})
    {
        status_ = std::string(who) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    void update_board(const InputFrame &input, ui::Feedback &feedback)
    {
        const float x = trigger_rect(column_, row_).cx();
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, style_, style_.sounds.change);
            return;
        }
        if (input.nav != Direction::none)
        {
            int column = column_;
            int row = row_;
            if (input.nav == Direction::left)
                --column;
            else if (input.nav == Direction::right)
                ++column;
            else if (input.nav == Direction::up)
                --row;
            else
                ++row;
            if (column < 0 || column >= kColumns || row < 0 || row >= rows_in(column_))
            {
                ui::refuse(feedback, style_, input, highlight_.refusal(), x);
                return;
            }
            column_ = column;
            // A shorter column takes the focus on its last row.
            row_ = std::min(row, rows_in(column_) - 1);
            ui::play_cue(feedback, style_, style_.sounds.move, trigger_rect(column_, row_).cx());
            return;
        }
        if (input.is_pressed(Action::confirm))
        {
            press_.trigger();
            fire(feedback);
        }
    }

    void fire(ui::Feedback &feedback)
    {
        switch (column_)
        {
        case dialogs:
            dialog_.open(dialog_content(row_), feedback);
            say("Dialog", "open()", kTriggers[column_][row_].label);
            break;
        case sheets:
            if (row_ == kListSheet)
            {
                list_.set_focus(0);
                list_.enter();
            }
            sheets_[static_cast<std::size_t>(row_)].open(feedback);
            say("Sheet", "open()", kTriggers[column_][row_].label);
            break;
        case toasts:
        {
            toasts_.push(kToastKinds[row_], kToastTitles[row_], kToastBodies[row_]);
            char text[64];
            std::snprintf(text, sizeof(text), "%d shown, %d waiting", toasts_.visible_count(),
                          toasts_.queued_count());
            say("ToastStack", "push()", text);
            break;
        }
        default:
            // Pin this side for every trigger of the board.
            placement_ = kPlacements[row_];
            ui::play_cue(feedback, style_, style_.sounds.change, trigger_rect(column_, row_).cx());
            say("Tooltip", "style.placement", kTriggers[column_][row_].label);
            break;
        }
    }

    void update_dialog(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = dialog_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            const int choice = dialog_.choice();
            const ui::DialogButton &button =
                dialog_.content().buttons[static_cast<std::size_t>(choice)];
            char text[96];
            std::snprintf(text, sizeof(text), "choice() = %d, \"%s\"", choice,
                          button.label.c_str());
            say("Dialog", "Event::activated", text);
            if (button.destructive)
                toasts_.push(ui::StatusKind::success, "Save deleted", "Slot 2 is free again");
        }
        else if (event == ui::Event::cancelled)
        {
            say("Dialog", "Event::cancelled");
        }
        else if (event == ui::Event::refused)
        {
            say("Dialog", "Event::refused");
        }
    }

    void update_sheet(ui::Sheet &sheet, const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = sheet.handle(input, feedback);
        if (event == ui::Event::cancelled)
        {
            say("Sheet", "Event::cancelled");
            return;
        }
        // The sheet only wants back: the rest belongs to what is inside it.
        if (event != ui::Event::none || &sheet != &sheets_[kListSheet])
            return;
        if (list_.handle(input, feedback) == ui::Event::activated)
        {
            const std::string &name = list_.items()[static_cast<std::size_t>(list_.focus())].title;
            toasts_.push(ui::StatusKind::success, "Added to the queue", name);
            say("Sheet", "the list inside returned Event::activated", name);
            sheet.close(feedback);
        }
    }

    ui::DialogContent dialog_content(int row) const
    {
        ui::DialogContent content;
        switch (row)
        {
        case 0:
            content.icon = ui::StatusKind::info;
            content.title = "Update installed";
            content.body = "Version 2.4 adds a photo mode and a chapter select. The notes are "
                           "in the Extras menu.";
            content.buttons = {{"Continue", ui::ButtonKind::primary, false}};
            break;
        case 1:
            content.icon = ui::StatusKind::question;
            content.title = "Start a new journey?";
            content.body = "Your current chapter keeps its own slot. You can return to it "
                           "from Continue.";
            content.buttons = {{"Not now", ui::ButtonKind::secondary, false},
                               {"Start", ui::ButtonKind::primary, false}};
            content.default_button = 1;
            break;
        case 2:
            content.icon = ui::StatusKind::danger;
            content.title = "Delete this save?";
            content.body = "Slot 2, Lantern Pass, 63 hours. This cannot be undone.";
            content.buttons = {{"Cancel", ui::ButtonKind::secondary, false},
                               {"Delete", ui::ButtonKind::primary, true}};
            // Asked for the destructive button: the dialog picks the safe one.
            content.default_button = 1;
            break;
        default:
            content.icon = ui::StatusKind::warning;
            content.title = "Leave without saving?";
            content.body = "The controller layout was changed. Save it before going back "
                           "to the menu?";
            content.buttons = {{"Stay", ui::ButtonKind::secondary, false},
                               {"Discard", ui::ButtonKind::secondary, false},
                               {"Save", ui::ButtonKind::primary, false}};
            content.default_button = 2;
            break;
        }
        return content;
    }

    void draw_cover(ui::Canvas &canvas, std::size_t index, const Rect &art, float radius) const
    {
        if (index >= context_.catalog.size())
            return;
        const demo::Item &title = context_.catalog[index];
        if (title.cover != 0)
            canvas.list.image(title.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
        else
            canvas.list.gradient_rect(art, radius, title.mid, title.dark);
    }

    // The left sheet: plain drawing by the page, inside the slot's rectangle.
    void draw_details(ui::Canvas &canvas, const Rect &area) const
    {
        if (context_.catalog.size() == 0)
            return;
        const demo::Item &title = context_.catalog[0];
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float art = std::min(168.0f, area.w * 0.4f);
        draw_cover(canvas, 0, {area.x, area.y + 8.0f, art, art}, 14.0f);
        const float x = area.x + art + 24.0f;
        const float room = std::max(area.x + area.w - x, 40.0f);
        paint.label(ui::fit_label(paint, title.title, 28.0f, room), x, area.y + 50.0f, 28.0f,
                    theme_.text);
        paint.body(ui::fit_body(paint, title.studio, 21.0f, room), x, area.y + 84.0f, 21.0f,
                   theme_.text_muted);
        paint.body(ui::fit_body(paint, title.genre, 21.0f, room), x, area.y + 114.0f, 21.0f,
                   theme_.text_muted);

        char value[3][32];
        std::snprintf(value[0], sizeof(value[0]), "%d", title.year);
        std::snprintf(value[1], sizeof(value[1]), "%d h", title.hours);
        std::snprintf(value[2], sizeof(value[2]), "%d", title.players);
        const char *names[3] = {"Released", "Played", "Players"};
        float y = area.y + art + 60.0f;
        for (int i = 0; i < 3; ++i)
        {
            paint.body(names[i], area.x, y, 22.0f, theme_.text_muted);
            paint.label(value[i], area.x + area.w, y, 22.0f, theme_.text, gfx::Align::right);
            canvas.list.rounded_rect({area.x, y + 14.0f, area.w, 1.5f}, 0.0f,
                                     theme_.text_muted.with_alpha(0.2f));
            y += 46.0f;
        }
        y += 12.0f;
        for (const std::string &line : ui::wrap_body(paint, title.blurb, 22.0f, area.w, 7))
        {
            if (y > area.y + area.h)
                break;
            paint.body(line, area.x, y, 22.0f, theme_.text);
            y += 32.0f;
        }
    }

    // The bottom sheet: a shelf of covers.
    void draw_shelf(ui::Canvas &canvas, const Rect &area) const
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float gap = 30.0f;
        const float label = 34.0f;
        // Stay inside the safe area and above the gallery's hint row.
        const float left = std::max(area.x, kPageArea.x);
        const float right = std::min(area.x + area.w, kPageArea.x + kPageArea.w);
        const float bottom = std::min(area.y + area.h, kHintTop);
        const float size = std::clamp(bottom - area.y - label - 8.0f, 60.0f, 220.0f);
        float x = left;
        for (std::size_t i = 0; i < context_.catalog.size() && x + size <= right + 0.5f; ++i)
        {
            draw_cover(canvas, i, {x, area.y + 4.0f, size, size}, 14.0f);
            paint.label(ui::fit_label(paint, context_.catalog[i].genre, 20.0f, size), x,
                        area.y + size + label, 20.0f, theme_.text_muted);
            x += size + gap;
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the board's own cues and highlight
    ui::Dialog dialog_;
    std::array<ui::Sheet, kSheets> sheets_;
    ui::ListView list_;
    ui::ToastStack toasts_;
    ui::Tooltip tooltip_;
    ui::Highlight highlight_;
    ui::Pulse press_;
    ui::TooltipPlacement placement_ = ui::TooltipPlacement::above;
    int column_ = 0;
    int row_ = 0;
    int variant_ = 0;
    float clock_ = 0.0f;
    float age_ = 0.0f;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_overlays_page(app::Context &context)
{
    return std::make_unique<OverlaysPage>(context);
}

} // namespace hui::concepts::gallery
