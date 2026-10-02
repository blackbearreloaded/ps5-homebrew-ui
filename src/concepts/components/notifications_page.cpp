// ps5-homebrew-ui - Component Library page: NotificationStack, NotificationCenter,
// NotificationBell. Copyright (C) 2026 BlackBearReloaded SPDX-License-Identifier: GPL-3.0-or-later
//
// A home screen that is not listening (covers, a shelf of recent titles) and,
// above it, the things that interrupt it. A row of triggers sends
// notifications: one that stays and asks a question, one that passes, one
// that turns into a download and then into "Restart". The stack is drawn
// over the page; the bell counts what was missed and opens the history in a
// sheet. Triangle is the way in: it gives the stack the focus, or, in the
// shortcut variant, fires the newest notification's main action directly.

#include "concepts/components/page.hpp"

#include "ui/components/badge.hpp"
#include "ui/components/button.hpp"
#include "ui/components/notification.hpp"
#include "ui/components/notification_bell.hpp"
#include "ui/components/notification_center.hpp"
#include "ui/components/sheet.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kCaptionY = 258.0f;
constexpr float kTriggerY = 272.0f;
constexpr float kTriggerHeight = 58.0f;
constexpr float kTriggerGap = 14.0f;
constexpr float kBellSize = 64.0f;
constexpr Rect kBell{kPageArea.x + kPageArea.w - kBellSize, kTriggerY - 3.0f, kBellSize, kBellSize};
// The gallery draws its hint row over the page: keep content above it.
constexpr float kHintTop = 968.0f;
// Where notifications live: under the triggers, over the home screen.
constexpr Rect kStackBounds{kPageArea.x, 350.0f, kPageArea.w, 610.0f};
constexpr float kCoversCaptionY = 378.0f;
constexpr float kCoversTop = 394.0f;
constexpr float kCoverSize = 228.0f;
constexpr float kShelfCaptionY = 730.0f;
constexpr float kShelfTop = 746.0f;
constexpr float kShelfHeight = 150.0f;
constexpr float kDownloadSeconds = 4.2f;
constexpr int kDownloadSize = 120; // megabytes, invented

enum Trigger : int
{
    update_available,
    friend_online,
    low_battery,
    save_failed,
    burst,
    clear_all,
    trigger_count,
};
constexpr int kBellFocus = trigger_count; // the bell is the row's last stop

struct TriggerInfo
{
    const char *label;
    ui::StatusKind kind; // the small symbol before the label
};
constexpr TriggerInfo kTriggers[trigger_count] = {
    {"Update available", ui::StatusKind::info}, {"Friend online", ui::StatusKind::success},
    {"Low battery", ui::StatusKind::warning},   {"Save failed", ui::StatusKind::danger},
    {"Burst of five", ui::StatusKind::none},    {"Clear all", ui::StatusKind::none},
};

// What a notification is, so the page knows what its actions mean.
enum Tag : int
{
    tag_plain,
    tag_update,
    tag_download,
    tag_installed,
    tag_save,
    tag_request,
};

constexpr const char *kVariants[] = {
    "Cards, top right, focus",
    "Compact, bottom centre, shortcut",
    "Accent, bottom right, two at a time",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));
constexpr int kShortcutVariant = 1;

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Send"},
    {ui::Button::triangle, "Focus notifications"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kShortcutHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Send"},
    {ui::Button::triangle, "Newest action"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kStackHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Back to the page"},
};
constexpr ui::Hint kCenterHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kNorth = action_bit(Action::north);
constexpr std::uint32_t kWest = action_bit(Action::west);

constexpr app::TourStep kTour[] = {
    {0.6f, kConfirm},                                      // "Update available" arrives
    {1.0f, kNorth},                                        // the stack takes the focus
    {0.7f, kConfirm, Direction::none, "notifications"},    // "Update now"
    {1.7f, 0, Direction::right, "notifications-progress"}, // it became a download
    {0.3f, kConfirm},                                      // a friend comes online
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.3f, kConfirm},                                   // five at once: some must wait
    {1.7f, 0, Direction::right, "notifications-queue"}, // installed, and "+N more"
    {0.4f, kConfirm},                                   // clear all: they go to the history
    {0.7f, 0, Direction::right},                        // the bell
    {0.4f, kConfirm},                                   // the centre opens in a sheet
    {1.2f, 0, Direction::down, "notifications-center"},
    {0.4f, kBack},
    {0.4f, kWest}, // compact, bottom centre, Triangle shortcut
    {0.3f, 0, Direction::left},
    {0.3f, 0, Direction::left},
    {0.3f, kConfirm},
    {1.1f, kWest, Direction::none, "notifications-compact"}, // accent, bottom right
    {1.1f, kWest, Direction::none, "notifications-accent"},  // back to the cards
    {0.3f, 0, Direction::right},
    {0.3f, kConfirm}, // clear all
};

Rect trigger_rect(int index)
{
    const float room = kBell.x - 28.0f - kPageArea.x;
    const float count = static_cast<float>(trigger_count);
    const float width = (room - kTriggerGap * (count - 1.0f)) / count;
    return {kPageArea.x + static_cast<float>(index) * (width + kTriggerGap), kTriggerY, width,
            kTriggerHeight};
}

const char *reason_name(ui::CloseReason reason)
{
    switch (reason)
    {
    case ui::CloseReason::timed_out:
        return "timed_out";
    case ui::CloseReason::closed:
        return "closed";
    case ui::CloseReason::action:
        return "action";
    default:
        return "dismissed";
    }
}

class NotificationsPage final : public Page
{
  public:
    explicit NotificationsPage(app::Context &context) : context_(context)
    {
        sheet_.set_title("Notifications");
        sheet_.content = [this](ui::Canvas &canvas, const Rect &, float) { center_.draw(canvas); };
        center_.attach(stack_);
        stack_.on_close = [this](int id, ui::CloseReason reason)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "id %d, CloseReason::%s", id, reason_name(reason));
            say("NotificationStack", "on_close", text);
        };
        restyle(ui::default_theme(), false);
        highlight_.snap(trigger_rect(focus_));
    }

    const char *title() const override
    {
        return "Notifications";
    }
    const char *summary() const override
    {
        return "ui::NotificationStack, ui::NotificationCenter, ui::NotificationBell";
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
        sheet_.dismiss();
        stack_.set_active(false);
        stack_.clear(true);
        download_id_ = 0;
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        age_ += dt;

        if (sheet_.is_open())
        {
            update_center(input, feedback);
        }
        else
        {
            if (stack_.active())
                update_stack(input, feedback);
            else
                update_board(input, feedback);
            // The shortcut works whoever has the focus.
            answer(stack_.handle_shortcut(input, feedback), feedback);
        }
        advance_download(dt);

        const bool on_board = !sheet_.is_open() && !stack_.active();
        bell_.set_active(on_board && focus_ == kBellFocus);
        bell_.set_count(center_.unread());
        center_.set_active(sheet_.is_open());

        highlight_.target(trigger_rect(std::min(focus_, trigger_count - 1)));
        highlight_.update(dt, style_);
        ring_.target = on_board && focus_ != kBellFocus ? 1.0f : 0.0f;
        ring_.update(dt, 18.0f);
        press_.update(dt, 8.0f);
        stack_.update(dt, feedback);
        center_.update(dt);
        bell_.update(dt);
        sheet_.update(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const auto arrive = [&](int part)
        {
            const float shown =
                reduced_ ? tween::cubic_out(age_ / 0.2f) : tween::stagger(age_, part, 0.05f, 0.34f);
            list.push_opacity(shown);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, reduced_ ? 0.0f : 20.0f * (1.0f - shown));
        };
        const auto done = [&]()
        {
            list.pop_transform();
            list.pop_opacity();
        };

        // ---- the triggers and the bell ----
        arrive(0);
        paint.label("Send a notification", kPageArea.x, kCaptionY, 20.0f, paint.page_text_muted());
        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 20.0f, 1180.0f), kBell.x - 28.0f, kCaptionY,
                       20.0f, paint.page_text_muted(), gfx::Align::right);
        for (int i = 0; i < trigger_count; ++i)
        {
            const Rect r = trigger_rect(i);
            ui::Look look;
            if (i == focus_)
                look.press = tween::clamp01(press_.value);
            const ui::ButtonFace face =
                ui::draw_button_face(canvas, theme_, r, ui::ButtonRole::secondary, look);
            const float mark = kTriggers[i].kind != ui::StatusKind::none ? 22.0f : 0.0f;
            const float gap = mark > 0.0f ? 10.0f : 0.0f;
            const std::string label =
                ui::fit_label(paint, kTriggers[i].label, 20.0f, r.w - 28.0f - mark - gap);
            const float width = paint.label_width(label, 20.0f) + mark + gap;
            const float left = face.content.cx() - width * 0.5f;
            if (mark > 0.0f)
                ui::draw_status_icon(canvas, theme_, kTriggers[i].kind, left + mark * 0.5f,
                                     face.content.cy(), mark);
            paint.label(label, left + mark + gap, face.content.cy() + 7.0f, 20.0f, face.ink);
        }
        // One ring for the row; the buttons draw none of their own.
        ui::HighlightStyle ring;
        ring.kind = ui::HighlightKind::ring;
        highlight_.draw(canvas, style_, ring, tween::clamp01(ring_.value));
        bell_.draw(canvas);
        done();

        // ---- the home screen underneath ----
        arrive(1);
        paint.label("Continue playing", kPageArea.x, kCoversCaptionY, 20.0f,
                    paint.page_text_muted());
        const float cover_gap = (kPageArea.w - 7.0f * kCoverSize) / 6.0f;
        for (std::size_t i = 0; i < 7 && i < context_.catalog.size(); ++i)
        {
            const demo::Item &item = context_.catalog[i];
            const float x = kPageArea.x + static_cast<float>(i) * (kCoverSize + cover_gap);
            draw_cover(canvas, i, {x, kCoversTop, kCoverSize, kCoverSize},
                       std::min(theme_.radius_card, 16.0f));
            paint.label(ui::fit_label(paint, item.title, 21.0f, kCoverSize), x,
                        kCoversTop + kCoverSize + 30.0f, 21.0f, paint.page_text());
            paint.body(ui::fit_body(paint, item.genre, 18.0f, kCoverSize), x,
                       kCoversTop + kCoverSize + 56.0f, 18.0f, paint.page_text_muted());
        }
        done();

        arrive(2);
        paint.label("Recently added", kPageArea.x, kShelfCaptionY, 20.0f, paint.page_text_muted());
        const float tile_gap = 20.0f;
        const float tile_w = (kPageArea.w - 3.0f * tile_gap) / 4.0f;
        for (std::size_t i = 0; i < 4 && context_.catalog.size() > 0; ++i)
        {
            const demo::Item &item = context_.catalog[i + 7];
            const Rect tile{kPageArea.x + static_cast<float>(i) * (tile_w + tile_gap), kShelfTop,
                            tile_w, kShelfHeight};
            paint.panel(tile);
            const float art = kShelfHeight - 36.0f;
            draw_cover(canvas, i + 7, {tile.x + 18.0f, tile.y + 18.0f, art, art},
                       std::min(theme_.radius, 10.0f));
            const float x = tile.x + art + 36.0f;
            const float room = std::max(tile.x + tile.w - 18.0f - x, 40.0f);
            paint.label(ui::fit_label(paint, item.title, 22.0f, room), x, tile.y + 48.0f, 22.0f,
                        theme_.text);
            paint.body(ui::fit_body(paint, item.studio, 18.0f, room), x, tile.y + 76.0f, 18.0f,
                       theme_.text_muted);
            paint.progress({x, tile.y + kShelfHeight - 44.0f, room, 12.0f}, item.progress);
        }
        done();
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        // The stack floats over the page; the history covers both.
        stack_.draw(canvas);
        sheet_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (sheet_.is_open())
            return kCenterHints;
        if (stack_.active())
            return kStackHints;
        if (variant_ == kShortcutVariant)
            return kShortcutHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // Every variant starts from the components' defaults, so what it does not
    // name is the default. What is on screen and the focus are left alone.
    void apply_variant()
    {
        style_.theme = theme_;
        style_.reduced_motion = reduced_;
        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };

        ui::NotificationStyle stack;
        stack.margin = 0.0f;
        if (variant_ == kShortcutVariant)
        {
            stack.look = ui::NotificationLook::compact;
            stack.anchor = ui::ToastAnchor::bottom_center;
            stack.width = 720.0f;
            stack.shortcut = Action::north;
            stack.gap = 10.0f;
        }
        else if (variant_ == 2)
        {
            stack.look = ui::NotificationLook::accent;
            stack.anchor = ui::ToastAnchor::bottom_right;
            stack.width = 620.0f;
            stack.max_visible = 2;
            stack.time_bar = false;
            stack.stretch_actions = false;
            stack.body_lines = 2;
        }
        themed(stack);
        stack_.style = stack;
        stack_.set_bounds(kStackBounds);
        // The focus path belongs to the other two variants.
        if (variant_ == kShortcutVariant)
            stack_.set_active(false);

        ui::NotificationCenterStyle center;
        themed(center);
        center_.style = center;

        ui::NotificationBellStyle bell;
        themed(bell);
        bell_.style = bell;
        bell_.set_bounds(kBell);

        ui::SheetStyle sheet;
        sheet.edge = ui::SheetEdge::right;
        sheet.size = 620.0f;
        // The gallery draws its hints over the page: the sheet stops above them.
        sheet.footer = gfx::kVirtualHeight - kHintTop;
        themed(sheet);
        sheet_.style = sheet;
        center_.set_bounds(sheet_.content_rect());
    }

    void say(const char *who, const char *what, const std::string &detail = {})
    {
        status_ = std::string(who) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    void update_board(const InputFrame &input, ui::Feedback &feedback)
    {
        const bool on_bell = focus_ == kBellFocus;
        const float x = on_bell ? kBell.cx() : trigger_rect(focus_).cx();
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, style_, style_.sounds.change);
            return;
        }
        if (input.is_pressed(Action::north))
        {
            // With a shortcut set the stack answers Triangle by itself.
            if (variant_ == kShortcutVariant)
            {
                if (stack_.newest() == 0)
                    ui::refuse(feedback, style_, input, highlight_.refusal(), x);
                return;
            }
            if (!stack_.focusable())
            {
                ui::refuse(feedback, style_, input, highlight_.refusal(), x);
                return;
            }
            stack_.set_active(true);
            ui::play_cue(feedback, style_, style_.sounds.move, kStackBounds.x + kStackBounds.w);
            say("NotificationStack", "set_active(true)", "the timers stand still");
            return;
        }
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = focus_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0 || next > kBellFocus)
            {
                ui::refuse(feedback, style_, input, highlight_.refusal(), x);
                return;
            }
            focus_ = next;
            ui::play_cue(feedback, style_, style_.sounds.move,
                         focus_ == kBellFocus ? kBell.cx() : trigger_rect(focus_).cx());
            return;
        }
        if (on_bell)
        {
            if (bell_.handle(input, feedback) == ui::Event::activated)
            {
                center_.enter();
                sheet_.open(feedback);
                say("NotificationBell", "Event::activated", "the centre opens in a ui::Sheet");
            }
            return;
        }
        if (input.is_pressed(Action::confirm))
        {
            press_.trigger();
            ui::play_cue(feedback, style_, style_.sounds.activate, x);
            fire(focus_);
        }
    }

    void update_stack(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = stack_.handle(input, feedback);
        if (event == ui::Event::cancelled)
            say("NotificationStack", "Event::cancelled", "the page has the focus again");
        else
            answer(event, feedback);
    }

    void update_center(const InputFrame &input, ui::Feedback &feedback)
    {
        if (sheet_.handle(input, feedback) == ui::Event::cancelled)
        {
            // Seen: next time they are listed under "Earlier".
            center_.mark_all_read();
            say("NotificationCenter", "mark_all_read()");
            return;
        }
        // The sheet only wants back: the rest belongs to the list inside.
        const ui::Event event = center_.handle(input, feedback);
        const ui::NotificationRecord *record = center_.event_record();
        if (event == ui::Event::activated && record != nullptr)
        {
            say("NotificationCenter", "Event::activated",
                "\"" + record->notification.actions[0] + "\" again");
        }
        else if (event == ui::Event::changed)
        {
            say("NotificationCenter", "Event::changed", "the history is empty");
        }
    }

    // What the page does about an event of the stack, from either path.
    void answer(ui::Event event, ui::Feedback &)
    {
        const int id = stack_.event_id();
        if (event == ui::Event::changed)
        {
            char text[48];
            std::snprintf(text, sizeof(text), "id %d closed by the player", id);
            say("NotificationStack", "Event::changed", text);
            return;
        }
        if (event != ui::Event::activated)
            return;
        const int action = stack_.event_action();
        const ui::Notification *note = stack_.find(id);
        if (note == nullptr)
            return;
        char text[96];
        std::snprintf(text, sizeof(text), "id %d, action %d \"%s\"", id, action,
                      note->actions[static_cast<std::size_t>(action)].c_str());
        say("NotificationStack", "Event::activated", text);

        if (note->tag == tag_update && action == 0)
        {
            // The same notification becomes the download: updating it here
            // keeps it on screen although its action was chosen.
            ui::Notification download;
            download.kind = ui::StatusKind::info;
            download.source = "System update";
            download.time = "now";
            download.title = "Downloading version 2.4";
            download.body = megabytes(0.0f);
            download.progress = 0.0f;
            download.seconds = -1.0f;
            download.closable = false;
            download.tag = tag_download;
            stack_.update_notification(id, download);
            download_id_ = id;
            download_ = 0.0f;
        }
        else if (note->tag == tag_save && action == 0)
        {
            ui::Notification synced;
            synced.kind = ui::StatusKind::success;
            synced.source = "Cloud saves";
            synced.time = "now";
            synced.title = "Save synced";
            synced.body = "Slot 2 is up to date.";
            synced.seconds = 4.0f;
            stack_.update_notification(id, synced);
        }
    }

    static std::string megabytes(float progress)
    {
        char text[48];
        std::snprintf(text, sizeof(text), "%d of %d MB",
                      static_cast<int>(progress * static_cast<float>(kDownloadSize)),
                      kDownloadSize);
        return text;
    }

    // Progress is the application's to report: the page plays the download.
    void advance_download(float dt)
    {
        if (download_id_ == 0)
            return;
        download_ = std::min(download_ + dt / kDownloadSeconds, 1.0f);
        if (!stack_.set_progress(download_id_, download_))
        {
            download_id_ = 0; // it was cleared away
            return;
        }
        stack_.set_body(download_id_, megabytes(download_));
        if (download_ < 1.0f)
            return;
        ui::Notification installed;
        installed.kind = ui::StatusKind::success;
        installed.source = "System update";
        installed.time = "now";
        installed.title = "Update installed";
        installed.body = "Restart to finish. Your place in the game is kept.";
        installed.actions = {"Restart"};
        installed.seconds = -1.0f;
        installed.tag = tag_installed;
        stack_.update_notification(download_id_, installed);
        download_id_ = 0;
    }

    void fire(int trigger)
    {
        ui::Notification note;
        switch (trigger)
        {
        case update_available:
            note.kind = ui::StatusKind::info;
            note.source = "System update";
            note.time = "now";
            note.title = "Version 2.4 is ready";
            note.body = "Photo mode, a chapter select and faster loading.";
            note.actions = {"Update now", "Later"};
            note.seconds = -1.0f; // stays until answered
            note.tag = tag_update;
            break;
        case friend_online:
            note.kind = ui::StatusKind::success;
            note.source = "Friends";
            note.time = "now";
            note.title = "Mara Voss is online";
            note.body = "Playing Lantern Pass";
            note.seconds = 5.0f;
            // The icon slot: a person instead of a status symbol.
            note.icon = [this](ui::Canvas &canvas, const Rect &box)
            {
                ui::Avatar avatar;
                avatar.style.theme = theme_;
                avatar.set_name("Mara Voss");
                avatar.set_bounds(box);
                avatar.draw(canvas);
            };
            break;
        case low_battery:
            note.kind = ui::StatusKind::warning;
            note.source = "Controller";
            note.time = "now";
            note.title = "Controller battery low";
            note.body = "About 20 minutes left.";
            note.actions = {"Dismiss"};
            note.seconds = 8.0f;
            break;
        case save_failed:
            note.kind = ui::StatusKind::danger;
            note.source = "Cloud saves";
            note.time = "now";
            note.title = "Save failed";
            note.body = "The server did not answer. Slot 2 is safe on this console.";
            note.actions = {"Retry", "Details"};
            note.seconds = -1.0f;
            note.tag = tag_save;
            break;
        case burst:
        {
            struct Short
            {
                ui::StatusKind kind;
                const char *source;
                const char *title;
                const char *body;
                const char *action;
            };
            static constexpr Short kBurst[] = {
                {ui::StatusKind::success, "Trophies", "Trophy earned", "First light, bronze", ""},
                {ui::StatusKind::info, "Downloads", "Download complete", "Harbor Lights, 2.4 GB",
                 ""},
                {ui::StatusKind::question, "Friends", "Friend request", "Ilya Brandt", "Accept"},
                {ui::StatusKind::warning, "Storage", "Storage almost full", "2.1 GB left", ""},
                {ui::StatusKind::success, "Captures", "Capture saved", "A clip of 30 seconds", ""},
            };
            for (const Short &each : kBurst)
            {
                ui::Notification one;
                one.kind = each.kind;
                one.source = each.source;
                one.time = "now";
                one.title = each.title;
                one.body = each.body;
                if (each.action[0] != '\0')
                {
                    one.actions = {each.action};
                    one.tag = tag_request;
                }
                stack_.push(one);
            }
            say("NotificationStack", "push()", "five at once: what does not fit waits its turn");
            return;
        }
        default:
            stack_.clear();
            say("NotificationStack", "clear()", "everything goes to the history");
            return;
        }
        const int id = stack_.push(note);
        char text[96];
        std::snprintf(text, sizeof(text), "id %d, \"%s\"", id, note.title.c_str());
        say("NotificationStack", "push()", text);
    }

    void draw_cover(ui::Canvas &canvas, std::size_t index, const Rect &art, float radius) const
    {
        if (context_.catalog.size() == 0)
            return;
        const demo::Item &item = context_.catalog[index];
        if (item.cover != 0)
            canvas.list.image(item.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
        else
            canvas.list.gradient_rect(art, radius, item.mid, item.dark);
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the board's own cues and highlight
    ui::NotificationStack stack_;
    ui::NotificationCenter center_;
    ui::NotificationBell bell_;
    ui::Sheet sheet_;
    ui::Highlight highlight_;
    tween::Spring ring_{1.0f, 0.0f, 1.0f};
    ui::Pulse press_;
    int focus_ = 0;
    int variant_ = 0;
    int download_id_ = 0;
    float download_ = 0.0f;
    float age_ = 0.0f;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_notifications_page(app::Context &context)
{
    return std::make_unique<NotificationsPage>(context);
}

} // namespace hui::concepts::gallery
