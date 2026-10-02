// ps5-homebrew-ui - Design "Paper Library": a game shelf made of paper cards.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A warm, tactile library in the spirit of a boxed board-game shelf: cream
// paper cards on a night-blue table, each holding a cover like a photo print.
// What makes it feel finished:
//
//   - every card owns a pair of springs keyed by its title, not by its place,
//     so sorting (Square) and filtering (L2/R2) make the cards visibly travel
//     to their new places, a few milliseconds apart, instead of redrawing;
//   - a card in the air looks like it: the focused card grows, its shadow
//     drops away and softens, and a second sheet turns out from under it;
//     cards in flight lift and lean by their speed;
//   - one gold ring glides between cards and follows a card while it travels;
//     the grid scrolls with a spring and dissolves at the edges it scrolls past;
//   - the details dialog is itself a sheet of paper laid over a blurred
//     screen, with an ink highlight that glides between its actions;
//   - small things pop: the favourite sticker and the "finished" stamp
//     overshoot, and finishing a title throws confetti;
//   - an empty filter is a friendly note, never a blank screen;
//   - the paper sound set answers every move, panned to where it happened,
//     and the edges of the grid refuse softly.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/confetti.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kNightTop = Color::rgb(0x0c1330);
const Color kNightBottom = Color::rgb(0x1d1240);
const Color kPaper = Color::rgb(0xf4f1ea);
const Color kPaperShade = Color::rgb(0xe6e1d6);
const Color kPrint = Color::rgb(0xfcfbf7); // the white border of a photo print
const Color kInk = Color::rgb(0x1b1d2b);
const Color kInkMuted = Color::rgb(0x6b6f82);
const Color kOnDark = Color::rgb(0xf5f3ff);
const Color kGold = Color::rgb(0xffd166);
const Color kStar = Color::rgb(0xffb400);
const Color kBlack = Color::rgb(0x000000);
const Color kWhite = Color::rgb(0xffffff);
const Color kClear = Color::rgb(0x000000, 0.0f);

constexpr float kMargin = 96.0f;
constexpr float kRight = gfx::kVirtualWidth - kMargin;

// The grid: six cards across the safe area, two rows on screen.
constexpr int kColumns = 6;
constexpr float kCardW = 268.0f;
constexpr float kCardH = 344.0f;
constexpr float kCardGap = 24.0f;
constexpr float kRowPitch = kCardH + kCardGap;
constexpr float kCardRadius = 14.0f;
constexpr float kCoverInset = 12.0f; // paper margin around the cover
constexpr float kLift = 0.08f;       // how much the focused card grows
constexpr float kTilt = 0.042f;      // radians the sheet under a lifted card turns
constexpr float kRingGap = 12.0f;    // from the lifted card to the focus ring's outer edge
constexpr float kGridTop = 210.0f;   // the clipped grid area
constexpr float kGridBottom = 990.0f;
constexpr float kGridView = kGridBottom - kGridTop;
constexpr float kRowInset = 30.0f; // room above the first row for a lifted card and its ring
constexpr float kReveal = 38.0f;   // clearance scrolling keeps around the focused row
constexpr float kFeather = 14.0f;  // height of the dissolve at a scrolled edge
constexpr float kEdgeCover = 8.0f; // solid part of the top edge, over the row scrolled past it

// Header.
constexpr float kTitleBaseline = 126.0f;
constexpr float kChipY = 150.0f;
constexpr float kChipH = 46.0f;
constexpr float kChipGap = 10.0f;
constexpr float kShoulder = 34.0f; // L2 / R2 glyph height beside the chips

// Re-flow timing: cards start their journey a little after one another.
constexpr float kTravelStep = 0.014f;
constexpr float kAppearStep = 0.028f;
constexpr float kAppearWait = 0.09f; // lets the leaving cards fade first

// The details dialog.
constexpr float kDialogW = 1360.0f;
constexpr float kDialogH = 664.0f;
constexpr float kDialogX = (gfx::kVirtualWidth - kDialogW) * 0.5f;
constexpr float kDialogY = 190.0f;
constexpr float kDialogPad = 56.0f;
constexpr float kDialogCover = 404.0f; // the print, border included
constexpr float kActionH = 64.0f;
constexpr float kActionGap = 16.0f;

enum class Filter : int
{
    all,
    favorites,
    in_progress,
    finished,
    count,
};
constexpr int kFilters = static_cast<int>(Filter::count);

struct Chip
{
    const char *label;
    float width;
};
constexpr Chip kChips[kFilters] = {
    {"All", 112.0f}, {"Favorites", 176.0f}, {"In progress", 196.0f}, {"Finished", 164.0f}};

enum class Sort : int
{
    title,
    hours,
    rating,
    count,
};
constexpr int kSorts = static_cast<int>(Sort::count);
constexpr const char *kSortNames[kSorts] = {"A \xE2\x80\x93 Z", "Most played", "Top rated"};

enum class DialogAction : int
{
    play,
    favorite,
    finish,
    close,
    count,
};
constexpr int kActions = static_cast<int>(DialogAction::count);

constexpr const char *kTechniques[] = {
    "Cards keyed by item with their own position springs: sorting and filtering make them travel",
    "Paper physics: lifted cards grow, drop a softer shadow and turn a sheet out from under them",
    "A gold focus ring (ui::SpringRect) that glides between cards and follows them in flight",
    "Spring scrolling in a clipped grid whose scrolled edges dissolve into the backdrop",
    "A paper dialog over a glass-blurred screen, with a gliding ink highlight and confetti",
    "Overshooting favourite sticker and finished stamp; a friendly note for an empty filter",
    "The paper sound set, panned and pitched by position, with soft refusals at the edges",
};

constexpr app::TourStep kTour[] = {
    {0.6f, action_bit(Action::jump_next)}, // Favorites: nothing yet
    {0.9f, action_bit(Action::jump_prev), Direction::none, "empty"},
    {0.7f, action_bit(Action::north)}, // star three titles
    {0.25f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.25f, action_bit(Action::north)},
    {0.25f, 0, Direction::down},
    {0.25f, action_bit(Action::north)},
    {0.6f, action_bit(Action::west)}, // sort: cards take off
    {0.2f, 0, Direction::none, "travel"},
    {1.0f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.9f, action_bit(Action::confirm), Direction::none, "sorted"}, // open the details
    {0.4f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.7f, action_bit(Action::confirm), Direction::none, "details"}, // mark as finished
    {0.85f, action_bit(Action::back), Direction::none, "finished"},
    {0.6f, 0, Direction::none},
};

// One card's animation state. It belongs to the item, so it survives every
// re-ordering: only its targets change.
struct Card
{
    tween::Spring x, y;  // top-left corner in grid (content) coordinates
    tween::Spring shown; // 0 gone, 1 on the table
    tween::Spring lift;  // 0 resting, 1 focused
    tween::Bounce star;  // favourite sticker, overshoots
    tween::Bounce done;  // finished stamp, overshoots
    float target_x = 0.0f;
    float target_y = 0.0f;
    float delay = 0.0f;  // seconds until the targets above take effect
    bool wanted = false; // part of the current filter
    int slot = 0;        // its place in the visible order
};

Rect slot_rect(int slot)
{
    const int column = slot % kColumns;
    const int row = slot / kColumns;
    return {kMargin + static_cast<float>(column) * (kCardW + kCardGap),
            kRowInset + static_cast<float>(row) * kRowPitch, kCardW, kCardH};
}

// A rectangle scaled about its own centre.
Rect scaled(const Rect &r, float scale)
{
    return {r.cx() - r.w * scale * 0.5f, r.cy() - r.h * scale * 0.5f, r.w * scale, r.h * scale};
}

Rect chip_rect(int index)
{
    float x = kMargin + ui::button_width(ui::Button::l2, kShoulder) + 18.0f;
    for (int i = 0; i < index; ++i)
        x += kChips[i].width + kChipGap;
    return {x, kChipY, kChips[index].width, kChipH};
}

Rect action_rect(float index)
{
    const float width = (kDialogW - 2.0f * kDialogPad - 3.0f * kActionGap) / 4.0f;
    return {kDialogX + kDialogPad + index * (width + kActionGap),
            kDialogY + kDialogH - kDialogPad - kActionH, width, kActionH};
}

// A tick drawn from two strokes: sharper at small sizes than a font glyph.
void check_mark(gfx::DrawList &list, float cx, float cy, float size, Color color)
{
    const float t = size * 0.2f;
    list.line(cx - size * 0.42f, cy + size * 0.02f, cx - size * 0.1f, cy + size * 0.34f, t, color);
    list.line(cx - size * 0.1f, cy + size * 0.34f, cx + size * 0.46f, cy - size * 0.32f, t, color);
}

class Paper final : public app::Concept
{
  public:
    explicit Paper(app::Context &context) : context_(context)
    {
        const auto &items = context.catalog.items();
        const std::size_t count = items.size();
        cards_.resize(count);
        favorite_.assign(count, false);
        finished_.assign(count, false);
        titles_.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            finished_[i] = items[i].progress >= 1.0f;
            cards_[i].done.snap(finished_[i] ? 1.0f : 0.0f);
            // Fitted once: a title can never run over the edge of its card.
            const gfx::Font *font = context.fonts.semibold.font;
            const float room = kCardW - 2.0f * kCoverInset - 4.0f;
            titles_.push_back(font != nullptr ? font->fit(items[i].title, 24.0f, room)
                                              : std::string(items[i].title));
        }
        rebuild(true);
        ring_.snap(ring_target());
        chip_.snap(chip_rect(0));
        ring_alpha_.snap(1.0f);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "paper",
            "Paper Library",
            "A game shelf of paper cards that travel when sorted, filtered or picked up",
            "src/concepts/paper.cpp",
            audio::SoundSet::paper,
            kGold,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        if (dialog_open_)
            close_dialog();
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool reduced = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        if (!reduced)
            drift_ += dt; // the backdrop stands still under "Reduce motion"
        if (dialog_open_)
            update_dialog(input, feedback);
        else
            update_grid(input, feedback);

        // ---- animation state ----
        const int focused = focused_item();
        for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
        {
            Card &card = cards_[static_cast<std::size_t>(i)];
            const std::size_t index = static_cast<std::size_t>(i);
            // A card holds its old place until its turn in the stagger comes.
            card.delay -= dt;
            if (card.delay <= 0.0f)
            {
                card.delay = 0.0f;
                card.x.target = card.target_x;
                card.y.target = card.target_y;
                card.shown.target = card.wanted ? 1.0f : 0.0f;
            }
            card.x.update(dt, reduced ? 60.0f : 9.5f);
            card.y.update(dt, reduced ? 60.0f : 9.5f);
            card.shown.update(dt, 16.0f);
            card.lift.target = i == focused ? 1.0f : 0.0f;
            card.lift.update(dt, reduced ? 40.0f : 18.0f);
            card.star.target = favorite_[index] ? 1.0f : 0.0f;
            card.star.update(dt, 20.0f, reduced ? 1.0f : 0.42f);
            card.done.target = finished_[index] ? 1.0f : 0.0f;
            card.done.update(dt, 17.0f, reduced ? 1.0f : 0.4f);
        }

        if (focused >= 0)
        {
            // A little more clearance above than below: the top edge hides
            // what scrolled past it, the bottom edge lets the next row peek.
            const Rect row = slot_rect(focus_);
            scroll_.reveal(row.y - kEdgeCover, row.y + row.h, kGridView, kReveal, content_height());
        }
        else
        {
            scroll_.position.target = 0.0f;
        }
        scroll_.update(dt, reduced ? 40.0f : 12.0f);

        // The ring chases the card itself, not its slot, so it stays with a
        // card that is still on its way after a sort.
        if (focused >= 0)
            ring_.target(ring_target());
        ring_.update(dt, reduced ? 60.0f : 20.0f);
        ring_alpha_.target = focused >= 0 ? 1.0f : 0.0f;
        ring_alpha_.update(dt, 14.0f);
        nudge_.update(dt, 9.0f);

        chip_.target(chip_rect(static_cast<int>(filter_)));
        chip_.update(dt, reduced ? 60.0f : 18.0f);
        empty_.target = visible_.empty() ? 1.0f : 0.0f;
        empty_.update(dt, 11.0f);
        sort_label_.update(dt);
        count_swap_.update(dt);

        dialog_.target = dialog_open_ ? 1.0f : 0.0f;
        dialog_.update(dt, reduced ? 40.0f : 13.0f);
        action_position_.target = static_cast<float>(action_);
        action_position_.update(dt, reduced ? 60.0f : 21.0f);
        action_nudge_.update(dt, 9.0f);
        describe_.update(dt);
        confetti_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::bokeh;
        frame.backdrop.colors[0] = kNightTop;
        frame.backdrop.colors[1] = kNightBottom;
        frame.backdrop.colors[2] = Color::rgb(0xffc978); // lamp light
        frame.backdrop.colors[3] = Color::rgb(0x7d6bff);
        frame.backdrop.time = drift_;

        gfx::DrawList &list = frame.scene;
        // The dialog pushes the table back: the scene shrinks a little toward
        // its centre and dims while the sheet is up.
        const float back = dialog_.value;
        const float push = context_.settings.reduced_motion ? 0.0f : 0.035f * back;
        list.push_transform(1.0f - push, 960, 540, 0, 0);
        draw_grid(list);
        draw_edges(list);
        draw_empty(list);
        draw_header(list);
        list.pop_transform();
        if (back > 0.01f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x070a1c, 0.5f * back));
            frame.glass = true;
            draw_dialog(frame.overlay, frame.glass_texture);
        }
        // Confetti outlives the dialog: it keeps falling over the library.
        if (confetti_.active())
            confetti_.draw(frame.overlay);
        draw_hints(back > 0.5f ? frame.overlay : frame.scene);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(index)];
    }

    // The catalogue index under the ring, or -1 when the filter is empty.
    int focused_item() const
    {
        return visible_.empty() ? -1 : visible_[static_cast<std::size_t>(focus_)];
    }

    float progress(int index) const
    {
        return finished_[static_cast<std::size_t>(index)] ? 1.0f : item(index).progress;
    }

    bool matches(int index, Filter filter) const
    {
        const std::size_t i = static_cast<std::size_t>(index);
        switch (filter)
        {
        case Filter::favorites:
            return favorite_[i];
        case Filter::in_progress:
            return !finished_[i] && item(index).progress > 0.0f;
        case Filter::finished:
            return finished_[i];
        case Filter::all:
        case Filter::count:
            break;
        }
        return true;
    }

    int count_of(Filter filter) const
    {
        int count = 0;
        for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
            count += matches(i, filter) ? 1 : 0;
        return count;
    }

    // The sort order; titles break every tie so an order is always the same.
    bool before(int a, int b) const
    {
        const demo::Item &left = item(a);
        const demo::Item &right = item(b);
        if (sort_ == Sort::hours && left.hours != right.hours)
            return left.hours > right.hours;
        if (sort_ == Sort::rating && left.rating != right.rating)
            return left.rating > right.rating;
        return std::strcmp(left.title, right.title) < 0;
    }

    float content_height() const
    {
        const int rows = (static_cast<int>(visible_.size()) + kColumns - 1) / kColumns;
        return kRowInset + static_cast<float>(rows) * kRowPitch - kCardGap + kReveal;
    }

    // Where the ring wants to be: around the focused card, grown.
    Rect ring_target() const
    {
        const int focused = focused_item();
        if (focused < 0)
            return ring_.value();
        const Card &card = cards_[static_cast<std::size_t>(focused)];
        return scaled({card.x.value, card.y.value, kCardW, kCardH}, 1.0f + kLift);
    }

    // Recomputes which cards are on the table and where each belongs, after a
    // filter, sort or membership change. Nothing moves here: every card gets
    // a new target and a delay, and update() lets the springs do the travel.
    void rebuild(bool snap)
    {
        const bool reduced = context_.settings.reduced_motion;
        const int keep = focused_item();
        visible_.clear();
        for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
        {
            cards_[static_cast<std::size_t>(i)].wanted = false;
            if (matches(i, filter_))
                visible_.push_back(i);
        }
        std::sort(visible_.begin(), visible_.end(), [this](int a, int b) { return before(a, b); });

        // The focus stays on the same title when it is still here; otherwise
        // it stays at the same place in the grid.
        const auto found = std::find(visible_.begin(), visible_.end(), keep);
        if (found != visible_.end())
            focus_ = static_cast<int>(found - visible_.begin());
        else
            focus_ = std::clamp(focus_, 0, std::max(0, static_cast<int>(visible_.size()) - 1));
        if (visible_.empty())
            empty_filter_ = filter_;

        for (int slot = 0; slot < static_cast<int>(visible_.size()); ++slot)
        {
            Card &card = cards_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(slot)])];
            const Rect place = slot_rect(slot);
            card.wanted = true;
            card.slot = slot;
            card.target_x = place.x;
            card.target_y = place.y;
            if (snap)
            {
                card.x.snap(place.x);
                card.y.snap(place.y);
                card.shown.snap(1.0f);
                card.delay = 0.0f;
            }
            else if (reduced)
            {
                // No travel: the card is simply in its new place, faded in.
                if (card.x.value != place.x || card.y.value != place.y)
                    card.shown.value = 0.0f;
                card.x.snap(place.x);
                card.y.snap(place.y);
                card.delay = 0.0f;
            }
            else if (card.shown.value < 0.02f)
            {
                // A card that was off the table appears where it belongs.
                card.x.snap(place.x);
                card.y.snap(place.y);
                card.delay = kAppearWait + kAppearStep * static_cast<float>(std::min(slot, 17));
            }
            else
            {
                card.delay = kTravelStep * static_cast<float>(slot);
            }
        }
        for (Card &card : cards_)
        {
            if (!card.wanted)
                card.delay = 0.0f; // leaving cards go at once
        }

        // The count in the header cross-fades when what it says changes.
        const int shown = static_cast<int>(visible_.size());
        if (snap)
        {
            counted_filter_ = filter_;
            counted_ = shown;
        }
        else if (filter_ != counted_filter_ || shown != counted_)
        {
            previous_counted_filter_ = counted_filter_;
            previous_counted_ = counted_;
            counted_filter_ = filter_;
            counted_ = shown;
            count_swap_.start(reduced ? 0.12f : 0.32f);
        }
    }

    // "24 titles" for the whole library, "3 of 24 titles" for a filtered shelf.
    void count_text(Filter filter, int shown, char *out, std::size_t size) const
    {
        const int total = static_cast<int>(cards_.size());
        if (filter == Filter::all)
            std::snprintf(out, size, "%d titles", total);
        else
            std::snprintf(out, size, "%d of %d titles", shown, total);
    }

    void refuse(app::Feedback &feedback, float dx, float dy)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_x_ = dx;
        nudge_y_ = dy;
    }

    void update_grid(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(visible_.size());

        // L2 / R2: the filter chips, wrapping around like a rotary selector.
        const int turn = (input.is_pressed(Action::jump_next) ? 1 : 0) -
                         (input.is_pressed(Action::jump_prev) ? 1 : 0);
        if (turn != 0)
        {
            const int next = (static_cast<int>(filter_) + turn + kFilters) % kFilters;
            filter_ = static_cast<Filter>(next);
            // The chips sound like steps going up to the right.
            feedback.play(audio::Cue::tab, 0.94f + 0.04f * static_cast<float>(next),
                          ui::pan_for_x(chip_rect(next).cx()));
            rebuild(false);
            return;
        }

        if (input.is_pressed(Action::west))
        {
            previous_sort_ = sort_;
            sort_ = static_cast<Sort>((static_cast<int>(sort_) + 1) % kSorts);
            sort_label_.start(context_.settings.reduced_motion ? 0.12f : 0.36f);
            feedback.play(audio::Cue::flip, 0.96f + 0.05f * static_cast<float>(sort_));
            rebuild(false);
            return;
        }

        if (input.nav != Direction::none)
        {
            if (count == 0)
            {
                if (!input.nav_repeat)
                    refuse(feedback, 0.0f, 0.0f);
            }
            else
            {
                navigate(input, feedback, count);
            }
        }

        if (input.is_pressed(Action::confirm))
        {
            if (count == 0)
            {
                feedback.play(audio::Cue::invalid, 1.0f, 0.0f, 0.7f);
                nudge_.trigger();
            }
            else
            {
                dialog_open_ = true;
                dialog_item_ = focused_item();
                action_ = previous_action_ = 0;
                action_position_.snap(0.0f);
                feedback.play(audio::Cue::modal_open);
            }
        }
        if (input.is_pressed(Action::north))
        {
            if (count == 0)
            {
                feedback.play(audio::Cue::invalid, 1.0f, 0.0f, 0.7f);
                nudge_.trigger();
            }
            else
            {
                const int focused = focused_item();
                toggle_favorite(focused, feedback,
                                ui::pan_for_x(cards_[static_cast<std::size_t>(focused)].x.value +
                                              kCardW * 0.5f));
                // On the Favorites shelf an unstarred card leaves at once and
                // its neighbours close the gap.
                if (!matches(focused, filter_))
                    rebuild(false);
            }
        }
    }

    void navigate(const InputFrame &input, app::Feedback &feedback, int count)
    {
        const int column = focus_ % kColumns;
        const int row = focus_ / kColumns;
        const int last_row = (count - 1) / kColumns;
        int next = focus_;
        float dx = 0.0f;
        float dy = 0.0f;
        switch (input.nav)
        {
        case Direction::left:
            dx = -1.0f;
            if (column > 0)
                next = focus_ - 1;
            break;
        case Direction::right:
            dx = 1.0f;
            if (column < kColumns - 1 && focus_ + 1 < count)
                next = focus_ + 1;
            break;
        case Direction::up:
            dy = -1.0f;
            if (row > 0)
                next = focus_ - kColumns;
            break;
        case Direction::down:
            dy = 1.0f;
            // A short last row still takes the focus: down lands on its end.
            if (row < last_row)
                next = std::min(focus_ + kColumns, count - 1);
            break;
        case Direction::none:
            break;
        }
        if (next != focus_)
        {
            focus_ = next;
            // Lower rows sound a little lower; the tick sits where the card is.
            feedback.play(audio::Cue::focus, 1.05f - 0.035f * static_cast<float>(next / kColumns),
                          ui::pan_for_x(slot_rect(next).cx()));
        }
        else if (!input.nav_repeat)
        {
            refuse(feedback, dx, dy);
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        const std::size_t index = static_cast<std::size_t>(dialog_item_);
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = action_ + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next < kActions)
            {
                previous_action_ = action_;
                action_ = next;
                describe_.start(context_.settings.reduced_motion ? 0.1f : 0.28f);
                feedback.play(audio::Cue::focus, 1.0f,
                              ui::pan_for_x(action_rect(static_cast<float>(next)).cx()));
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                action_nudge_.trigger();
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            switch (static_cast<DialogAction>(action_))
            {
            case DialogAction::play:
                feedback.play(audio::Cue::launch);
                feedback.rumble(0.7f, 0.18f);
                close_dialog();
                break;
            case DialogAction::favorite:
                toggle_favorite(dialog_item_, feedback, ui::pan_for_x(action_rect(1.0f).cx()));
                break;
            case DialogAction::finish:
                if (!finished_[index])
                {
                    finished_[index] = true;
                    confetti_.burst(item(dialog_item_).accent, ++bursts_ * 7919u,
                                    context_.settings.reduced_motion);
                    feedback.play(audio::Cue::complete);
                    feedback.rumble(0.5f, 0.14f);
                }
                else
                {
                    // Already done: the action is dimmed and says so; pressing
                    // it anyway gets a soft "no".
                    feedback.play(audio::Cue::invalid, 1.0f, 0.0f, 0.7f);
                    feedback.rumble(0.25f, 0.05f);
                    action_nudge_.trigger();
                }
                break;
            case DialogAction::close:
            case DialogAction::count:
                feedback.play(audio::Cue::modal_close);
                close_dialog();
                break;
            }
        }
        else if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            close_dialog();
        }
    }

    // Back at the table. If the title no longer belongs to the shelf shown
    // (finished while "In progress" is up), it leaves now, in view.
    void close_dialog()
    {
        dialog_open_ = false;
        if (!matches(dialog_item_, filter_))
            rebuild(false);
    }

    void toggle_favorite(int index, app::Feedback &feedback, float pan)
    {
        const std::size_t i = static_cast<std::size_t>(index);
        favorite_[i] = !favorite_[i];
        feedback.play(favorite_[i] ? audio::Cue::favorite_on : audio::Cue::favorite_off, 1.0f, pan);
    }

    // ---- drawing -----------------------------------------------------------

    void draw_header(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        char text[64];

        // Title and count.
        float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        float rise = reduced ? 0.0f : 14.0f * (1.0f - in);
        list.push_opacity(in);
        const float title_w = ui::text(list, fonts.display, "Library", kMargin - 3,
                                       kTitleBaseline - rise, 60, kOnDark);
        const float count_x = kMargin + title_w + 24;
        const float swap = count_swap_.running ? count_swap_.progress() : 1.0f;
        if (swap < 1.0f)
        {
            count_text(previous_counted_filter_, previous_counted_, text, sizeof(text));
            ui::text(list, fonts.regular, text, count_x, kTitleBaseline - rise, 26,
                     kOnDark.with_alpha(0.62f * (1.0f - tween::smoothstep(swap * 2.2f))));
        }
        count_text(counted_filter_, counted_, text, sizeof(text));
        ui::text(list, fonts.regular, text, count_x, kTitleBaseline - rise, 26,
                 kOnDark.with_alpha(0.62f * tween::smoothstep((swap - 0.3f) / 0.7f)));
        list.pop_opacity();

        // Filter chips between the two shoulder glyphs. The paper tab is one
        // object that slides under the labels; the label over it turns to ink.
        in = tween::stagger(age_, 1, 0.07f, 0.5f);
        rise = reduced ? 0.0f : 14.0f * (1.0f - in);
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, -rise);
        const ui::GlyphStyle glyphs = ui::GlyphStyle::dark();
        const float chip_cy = kChipY + kChipH * 0.5f;
        ui::draw_button(list, fonts, glyphs, ui::Button::l2, kMargin, chip_cy, kShoulder);
        const Rect last = chip_rect(kFilters - 1);
        ui::draw_button(list, fonts, glyphs, ui::Button::r2, last.x + last.w + 18, chip_cy,
                        kShoulder);
        const Rect tab = chip_.value();
        list.shadow({tab.x, tab.y + 5, tab.w, tab.h}, kChipH * 0.5f, 12, kBlack.with_alpha(0.4f));
        list.gradient_rect(tab, kChipH * 0.5f, kPaper, gfx::mix(kPaper, kPaperShade, 0.7f));
        for (int i = 0; i < kFilters; ++i)
        {
            const Rect chip = chip_rect(i);
            // How much of this chip the tab covers decides its ink.
            const float away = std::fabs(tab.cx() - chip.cx()) / (chip.w * 0.6f);
            const float cover = tween::clamp01(1.0f - away);
            if (cover < 0.99f)
                list.bordered_rect(chip, kChipH * 0.5f, kOnDark.with_alpha(0.05f * (1.0f - cover)),
                                   1.5f, kOnDark.with_alpha(0.2f * (1.0f - cover)));
            std::snprintf(text, sizeof(text), "%d", count_of(static_cast<Filter>(i)));
            const float label_w = fonts.semibold.measure(kChips[i].label, 22);
            const float count_w = fonts.mono.measure(text, 18);
            const float x = chip.cx() - (label_w + 10 + count_w) * 0.5f;
            ui::text(list, fonts.semibold, kChips[i].label, x, chip_cy + 8, 22,
                     gfx::mix(kOnDark.with_alpha(0.78f), kInk, cover));
            ui::text(list, fonts.mono, text, x + label_w + 10, chip_cy + 7, 18,
                     gfx::mix(kOnDark.with_alpha(0.45f), kInkMuted, cover));
        }

        // The sort order, top right. On a change the old name leaves upward
        // and the new one arrives from below, like a card being turned.
        ui::text(list, fonts.semibold, "SORTED BY", kRight, kChipY - 12, 16,
                 kOnDark.with_alpha(0.5f), gfx::Align::right, 3.0f);
        const float turn = sort_label_.running ? sort_label_.progress() : 1.0f;
        const float slide = reduced ? 0.0f : 16.0f;
        if (turn < 1.0f)
        {
            const float leave = tween::clamp01(turn * 2.2f);
            ui::text(list, fonts.semibold, kSortNames[static_cast<int>(previous_sort_)], kRight,
                     chip_cy + 9 - slide * tween::cubic_in(leave), 24,
                     kGold.with_alpha(1.0f - tween::smoothstep(leave)), gfx::Align::right);
        }
        const float arrive = tween::clamp01((turn - 0.3f) / 0.7f);
        ui::text(list, fonts.semibold, kSortNames[static_cast<int>(sort_)], kRight,
                 chip_cy + 9 + slide * (1.0f - tween::quint_out(arrive)), 24,
                 kGold.with_alpha(tween::smoothstep(arrive)), gfx::Align::right);
        list.pop_transform();
        list.pop_opacity();
    }

    // One paper card at its animated place.
    void draw_card(gfx::DrawList &list, int index, bool focused) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const Card &card = cards_[static_cast<std::size_t>(index)];
        const demo::Item &it = item(index);

        // Entrance: cards are dealt onto the table in reading order, counted
        // from the first row on screen.
        const int first = kColumns * static_cast<int>(scroll_.position.target / kRowPitch);
        const int order = std::clamp(card.slot - first, 0, 17);
        const float in = tween::stagger(age_, 3 + order, 0.035f, 0.45f);
        const float alpha = tween::clamp01(card.shown.value) * in;
        if (alpha <= 0.01f)
            return;

        const float lift = card.lift.value;
        // A card in flight leaves the table too: speed stands in for height.
        const float speed = std::hypot(card.x.velocity, card.y.velocity);
        const float flight = reduced ? 0.0f : tween::clamp01(speed / 1600.0f);
        const float air = std::max(lift, flight);

        Rect r{card.x.value, card.y.value + kGridTop - scroll_.offset(), kCardW, kCardH};
        if (r.y > kGridBottom + 60.0f || r.y + r.h < kGridTop - 60.0f)
            return;
        if (!reduced)
            r.y += 34.0f * (1.0f - in);
        if (focused)
        {
            r.x += ui::shake(nudge_.value, clock_, 12.0f, 8.0f) * nudge_x_;
            r.y += ui::shake(nudge_.value, clock_, 12.0f, 8.0f) * nudge_y_;
        }
        const float scale = (0.9f + 0.1f * tween::clamp01(card.shown.value)) *
                            (1.0f + kLift * lift + 0.03f * flight);

        list.push_opacity(alpha);
        list.push_transform(scale, r.cx(), r.cy(), 0, 0);

        // Shadow: tight and dark on the table, wide and soft in the air.
        list.shadow({r.x, r.y + 5 + 17 * air, r.w, r.h}, kCardRadius, 14 + 28 * air,
                    kBlack.with_alpha(0.42f + 0.2f * air));
        // The tilt: images and text cannot turn, so the card stays square and
        // a second sheet turns out from under it. In flight the sheet leans
        // against the direction of travel.
        const float side = index % 2 == 0 ? 1.0f : -1.0f;
        const float lean = std::clamp(card.x.velocity * 0.00006f, -0.07f, 0.07f);
        const float tilt = reduced ? 0.0f : kTilt * lift * side - lean;
        if (std::fabs(tilt) > 0.002f)
            list.rotated_rect(r, kCardRadius, tilt, kPaperShade);
        list.gradient_rect(r, kCardRadius, kPaper, gfx::mix(kPaper, kPaperShade, 0.6f));

        // The cover, set into the paper like a photo print.
        const Rect art{r.x + kCoverInset, r.y + kCoverInset, r.w - 2 * kCoverInset,
                       r.w - 2 * kCoverInset};
        list.image(it.cover, art, gfx::kCanvasUv, kWhite, 6);
        list.bordered_rect(art, 6, kClear, 1.0f, kInk.with_alpha(0.16f));

        const float text_x = r.x + kCoverInset + 2;
        const float text_right = r.x + r.w - kCoverInset - 2;
        const float base = art.y + art.h;
        ui::text(list, fonts.semibold, titles_[static_cast<std::size_t>(index)], text_x, base + 32,
                 24, kInk);
        ui::text(list, fonts.regular, it.genre, text_x, base + 58, 20, kInkMuted);

        // Right of the genre: the state in a word, or the finished tick.
        const float amount = progress(index);
        const float done = std::max(0.0f, card.done.value);
        if (done > 0.02f)
        {
            list.circle(text_right - 11, base + 51, 11 * done, it.mid);
            check_mark(list, text_right - 11, base + 51, 13 * done, kPaper);
        }
        else
        {
            char text[16];
            if (amount > 0.0f)
                std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(amount * 100.0f + 0.5f));
            else
                std::snprintf(text, sizeof(text), "New");
            ui::text(list, fonts.regular, text, text_right, base + 58, 20, kInkMuted,
                     gfx::Align::right);
        }
        const Rect bar{text_x, base + 70, text_right - text_x, 6};
        list.rounded_rect(bar, 3, kInk.with_alpha(0.12f));
        if (amount > 0.0f)
            list.rounded_rect({bar.x, bar.y, std::max(6.0f, bar.w * amount), bar.h}, 3, it.mid);

        // The favourite sticker sits over the corner and pops with overshoot.
        const float star = std::max(0.0f, card.star.value);
        if (star > 0.02f)
        {
            const float cx = r.x + r.w - 20;
            const float cy = r.y + 20;
            list.shadow({cx - 21 * star, cy - 21 * star + 4, 42 * star, 42 * star}, 21 * star, 9,
                        kBlack.with_alpha(0.5f));
            list.circle(cx, cy, 22 * star, kPaper);
            list.star(cx, cy + 1, 14 * star, kStar);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_grid(gfx::DrawList &list) const
    {
        const int focused = focused_item();
        list.push_clip({0, kGridTop, gfx::kVirtualWidth, kGridView});
        // Leaving cards lie lowest, then the shelf in order, then the focus.
        for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
        {
            if (!cards_[static_cast<std::size_t>(i)].wanted)
                draw_card(list, i, false);
        }
        for (int index : visible_)
        {
            if (index != focused)
                draw_card(list, index, false);
        }

        // The gold ring: its own spring, so it glides from card to card. Its
        // light is drawn under the focused card and its line over it.
        const float ring_alpha = ring_alpha_.value * tween::stagger(age_, 6, 0.07f, 0.4f);
        Rect ring = ring_.value();
        // In transit the ring is only a line; its light comes on as it lands,
        // so no patch of glow sweeps over the cards it passes.
        const Rect home = ring_target();
        const float away = std::fabs(ring.x - home.x) + std::fabs(ring.y - home.y);
        const float landed = tween::clamp01(1.0f - away / 90.0f);
        ring.y += kGridTop - scroll_.offset();
        ring.x += ui::shake(nudge_.value, clock_, 12.0f, 8.0f) * nudge_x_;
        ring.y += ui::shake(nudge_.value, clock_, 12.0f, 8.0f) * nudge_y_;
        const float radius = kCardRadius * (1.0f + kLift);
        if (ring_alpha > 0.01f)
        {
            const float breath = context_.settings.reduced_motion ? 0.5f : ui::breathe(clock_);
            list.glow(ring.inset(-kRingGap), radius + kRingGap, 26,
                      kGold.with_alpha((0.3f + 0.2f * breath) * ring_alpha * landed));
        }
        if (focused >= 0)
            draw_card(list, focused, true);
        // The line stands off the card, leaving room for the turned sheet.
        if (ring_alpha > 0.01f)
            list.bordered_rect(ring.inset(-kRingGap), radius + kRingGap, kClear, 4.0f,
                               kGold.with_alpha(ring_alpha));
        list.pop_clip();
    }

    // The dissolve at an edge the grid has scrolled past. The band is the
    // backdrop's own colour at that height, opaque on the clip line and clear
    // on both sides of it, so cards fade out instead of being cut.
    void draw_edges(gfx::DrawList &list) const
    {
        const float offset = scroll_.offset();
        const float above = tween::clamp01(offset / 40.0f);
        const float below = tween::clamp01((content_height() - kGridView - offset) / 40.0f);
        const float width = gfx::kVirtualWidth;
        if (above > 0.01f)
        {
            const Color night =
                gfx::mix(kNightTop, kNightBottom, kGridTop / gfx::kVirtualHeight).with_alpha(above);
            list.gradient_rect({0, kGridTop - kFeather, width, kFeather}, 0, night.with_alpha(0.0f),
                               night);
            list.rounded_rect({0, kGridTop, width, kEdgeCover}, 0, night);
            list.gradient_rect({0, kGridTop + kEdgeCover, width, kFeather}, 0, night,
                               night.with_alpha(0.0f));
        }
        if (below > 0.01f)
        {
            const Color night = gfx::mix(kNightTop, kNightBottom, kGridBottom / gfx::kVirtualHeight)
                                    .with_alpha(below);
            list.gradient_rect({0, kGridBottom - kFeather, width, kFeather}, 0,
                               night.with_alpha(0.0f), night);
            list.gradient_rect({0, kGridBottom, width, kFeather}, 0, night, night.with_alpha(0.0f));
        }
    }

    // A filter with nothing in it shows a note left on the table.
    void draw_empty(gfx::DrawList &list) const
    {
        const float t = empty_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float bob = reduced ? 0.0f : std::sin(clock_ * 0.9f) * 4.0f;
        const float shake = ui::shake(nudge_.value, clock_, 10.0f, 8.0f);
        const Rect note{960 - 330 + shake, 400 + bob, 660, 330};

        list.push_opacity(tween::clamp01(t * 1.3f));
        list.push_transform(reduced ? 1.0f : 0.92f + 0.08f * t, note.cx(), note.cy(), 0, 0);
        list.shadow({note.x, note.y + 20, note.w, note.h}, 22, 44, kBlack.with_alpha(0.5f));
        if (!reduced)
            list.rotated_rect(note, 22, -0.045f, kPaperShade);
        list.gradient_rect(note, 22, kPaper, gfx::mix(kPaper, kPaperShade, 0.6f));

        const char *headline = "Nothing here yet";
        const char *before = "Titles you add will wait here.";
        const char *after = "";
        bool glyph = false;
        const float icon_y = note.y + 92;
        switch (empty_filter_)
        {
        case Filter::favorites:
            headline = "No favorites yet";
            before = "Press";
            after = "on a title to keep it here.";
            glyph = true;
            list.circle(note.cx(), icon_y, 44, kPaperShade);
            list.star(note.cx(), icon_y + 1, 26, kStar, 3.5f);
            break;
        case Filter::in_progress:
            headline = "Nothing on the go";
            before = "Titles you have started will wait here.";
            list.circle(note.cx(), icon_y, 44, kPaperShade);
            list.arc(note.cx(), icon_y, 24, 6, 0.0f, 4.4f, kInkMuted);
            break;
        case Filter::finished:
            headline = "Nothing finished yet";
            before = "Mark a title as finished and it moves here.";
            list.circle(note.cx(), icon_y, 44, kPaperShade);
            check_mark(list, note.cx(), icon_y, 34, kInkMuted);
            break;
        case Filter::all:
        case Filter::count:
            break;
        }
        ui::text(list, fonts.display, headline, note.cx(), note.y + 204, 40, kInk,
                 gfx::Align::center);
        const float line = note.y + 262;
        if (glyph)
        {
            // A sentence with the button in it, centred as one line.
            const float gap = 12.0f;
            const float before_w = fonts.regular.measure(before, 26);
            const float after_w = fonts.regular.measure(after, 26);
            const float glyph_w = ui::button_width(ui::Button::triangle, 36);
            float x = note.cx() - (before_w + after_w + glyph_w + 2 * gap) * 0.5f;
            ui::text(list, fonts.regular, before, x, line, 26, kInkMuted);
            x += before_w + gap;
            ui::draw_button(list, fonts, ui::GlyphStyle::light(), ui::Button::triangle, x, line - 9,
                            36);
            x += glyph_w + gap;
            ui::text(list, fonts.regular, after, x, line, 26, kInkMuted);
        }
        else
        {
            ui::text(list, fonts.regular, before, note.cx(), line, 26, kInkMuted,
                     gfx::Align::center);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // What the focused action will do, in one line under the stats.
    void describe(int action, char *out, std::size_t size) const
    {
        const demo::Item &it = item(dialog_item_);
        const std::size_t index = static_cast<std::size_t>(dialog_item_);
        switch (static_cast<DialogAction>(action))
        {
        case DialogAction::play:
            if (finished_[index])
                std::snprintf(out, size, "Start again from the beginning.");
            else if (it.progress > 0.0f)
                std::snprintf(out, size, "Pick up where you left off, at %d%%.",
                              static_cast<int>(it.progress * 100.0f + 0.5f));
            else
                std::snprintf(out, size, "Start a new game.");
            break;
        case DialogAction::favorite:
            std::snprintf(out, size, "%s",
                          favorite_[index] ? "Take it off the Favorites shelf."
                                           : "Keep it on the Favorites shelf.");
            break;
        case DialogAction::finish:
            std::snprintf(out, size, "%s",
                          finished_[index] ? "Already on the Finished shelf. Well played."
                                           : "Move it to the Finished shelf.");
            break;
        case DialogAction::close:
        case DialogAction::count:
            std::snprintf(out, size, "Back to the library.");
            break;
        }
    }

    void draw_dialog(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const demo::Item &it = item(dialog_item_);
        const std::size_t index = static_cast<std::size_t>(dialog_item_);
        const Card &card = cards_[index];
        const float t = dialog_.value;
        char text[96];

        // Everything behind is out of focus: the glass copy covers the screen.
        list.push_opacity(tween::clamp01(t * 1.2f));
        list.glass(glass, {0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0, kWhite);
        list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                          Color::rgb(0x0a0e26, 0.34f));
        list.pop_opacity();

        // The sheet is laid on top: it settles downward and grows to size.
        const Rect sheet{kDialogX, kDialogY, kDialogW, kDialogH};
        list.push_opacity(tween::clamp01(t * 1.5f));
        if (!reduced)
            list.push_transform(0.94f + 0.06f * t, 960, 540, 0, -36.0f * (1.0f - t));
        else
            list.push_transform(1.0f, 0, 0, 0, 0);
        list.shadow({sheet.x, sheet.y + 28, sheet.w, sheet.h}, 28, 70, kBlack.with_alpha(0.6f));
        if (!reduced)
            list.rotated_rect(sheet, 28, 0.012f, kPaperShade);
        list.gradient_rect(sheet, 28, kPaper, gfx::mix(kPaper, kPaperShade, 0.5f));
        list.bordered_rect(sheet, 28, kClear, 1.5f, kWhite.with_alpha(0.7f));

        // The cover as a print lying on the sheet, slightly askew underneath.
        const Rect print{sheet.x + kDialogPad, sheet.y + kDialogPad, kDialogCover, kDialogCover};
        const Rect art = print.inset(12);
        list.shadow({print.x, print.y + 12, print.w, print.h}, 8, 26, kBlack.with_alpha(0.38f));
        if (!reduced)
            list.rotated_rect(print, 8, -0.04f, gfx::mix(kPaperShade, kInkMuted, 0.18f));
        list.rounded_rect(print, 8, kPrint);
        list.image(it.cover, art, gfx::kCanvasUv, kWhite, 4);
        list.bordered_rect(art, 4, kClear, 1.0f, kInk.with_alpha(0.16f));

        // The finished stamp pops onto the top corner of the print, where the
        // covers keep clear of text.
        const float done = std::max(0.0f, card.done.value);
        if (done > 0.02f)
        {
            const Rect stamp{print.x + print.w - 168, print.y - 20, 186, 46};
            list.push_transform(done, stamp.cx(), stamp.cy(), 0, 0);
            list.shadow({stamp.x, stamp.y + 6, stamp.w, stamp.h}, 23, 14, kBlack.with_alpha(0.4f));
            list.rounded_rect(stamp, 23, kGold);
            check_mark(list, stamp.x + 30, stamp.cy(), 20, kInk);
            ui::text(list, fonts.semibold, "FINISHED", stamp.x + 54, stamp.cy() + 6, 17, kInk,
                     gfx::Align::left, 3.0f);
            list.pop_transform();
        }

        // Text column.
        const float x = sheet.x + kDialogPad + kDialogCover + 44;
        const float width = sheet.x + sheet.w - kDialogPad - x;
        ui::text(list, fonts.semibold, ui::upper(it.genre), x, sheet.y + 90, 18, it.mid,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(it.title, 58, width), x - 3,
                 sheet.y + 154, 58, kInk);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %d", it.studio, it.year);
        ui::text(list, fonts.regular, text, x, sheet.y + 196, 24, kInkMuted);
        ui::paragraph(list, fonts.regular, it.blurb, x, sheet.y + 250, 26, width, 38,
                      kInk.with_alpha(0.88f), 2);

        // Four stat tiles, arriving one after another.
        const char *labels[] = {"RATING", "PLAYED", "PLAYERS", "PROGRESS"};
        const float tile_w = (width - 3 * 16.0f) / 4.0f;
        const float amount = progress(dialog_item_);
        for (int i = 0; i < 4; ++i)
        {
            const float appear = tween::stagger(t, i, 0.1f, 0.6f);
            const float drop = reduced ? 0.0f : 18.0f * (1.0f - appear);
            const Rect tile{x + static_cast<float>(i) * (tile_w + 16), sheet.y + 346 + drop, tile_w,
                            114};
            list.push_opacity(appear);
            list.rounded_rect(tile, 14, kPaperShade);
            ui::text(list, fonts.semibold, labels[i], tile.x + 20, tile.y + 34, 15, kInkMuted,
                     gfx::Align::left, 3.0f);
            switch (i)
            {
            case 0:
                std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
                // Five small stars; the last one fills by the fraction.
                for (int s = 0; s < 5; ++s)
                {
                    const float fill = tween::clamp01(it.rating - static_cast<float>(s));
                    const float sx = tile.x + 94 + static_cast<float>(s) * 17;
                    list.star(sx, tile.y + 68, 7.5f,
                              gfx::mix(kInk.with_alpha(0.16f), kStar, fill > 0.5f ? 1.0f : 0.0f));
                }
                break;
            case 1:
                std::snprintf(text, sizeof(text), "%d h", it.hours);
                break;
            case 2:
                if (it.players > 1)
                    std::snprintf(text, sizeof(text), "1\xE2\x80\x93%d", it.players);
                else
                    std::snprintf(text, sizeof(text), "1");
                break;
            default:
            {
                std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(amount * 100.0f + 0.5f));
                const Rect bar{tile.x + 20, tile.y + 94, tile.w - 40, 6};
                list.rounded_rect(bar, 3, kInk.with_alpha(0.12f));
                const float filled = std::max(6.0f, bar.w * amount * appear);
                if (amount > 0.0f)
                    list.rounded_rect({bar.x, bar.y, filled, bar.h}, 3, it.mid);
                break;
            }
            }
            ui::text(list, fonts.semibold, text, tile.x + 20, tile.y + 80, 36, kInk);
            list.pop_opacity();
        }

        // The focused action in words; it cross-fades when the focus moves.
        const float line_x = sheet.x + kDialogPad + 4;
        const float line_y = sheet.y + 514;
        const float swap = describe_.running ? describe_.progress() : 1.0f;
        if (swap < 1.0f)
        {
            describe(previous_action_, text, sizeof(text));
            ui::text(list, fonts.regular, text, line_x, line_y, 24,
                     kInkMuted.with_alpha(1.0f - tween::smoothstep(swap * 2.2f)));
        }
        describe(action_, text, sizeof(text));
        ui::text(list, fonts.regular, text, line_x, line_y, 24,
                 kInkMuted.with_alpha(tween::smoothstep((swap - 0.3f) / 0.7f)));

        // Actions: one ink highlight glides under the labels.
        const bool starred = favorite_[index];
        const bool finished = finished_[index];
        const char *names[kActions] = {
            finished ? "Play again" : (it.progress > 0.0f ? "Resume" : "Play"),
            starred ? "Unfavorite" : "Favorite",
            finished ? "Finished" : "Mark as finished",
            "Close",
        };
        for (int i = 0; i < kActions; ++i)
            list.rounded_rect(action_rect(static_cast<float>(i)), kActionH * 0.5f, kPaperShade);
        Rect highlight = action_rect(action_position_.value);
        highlight.x += ui::shake(action_nudge_.value, clock_, 10.0f, 9.0f);
        list.shadow({highlight.x, highlight.y + 8, highlight.w, highlight.h}, kActionH * 0.5f, 16,
                    kBlack.with_alpha(0.35f));
        list.rounded_rect(highlight, kActionH * 0.5f, kInk);
        for (int i = 0; i < kActions; ++i)
        {
            const Rect r = action_rect(static_cast<float>(i));
            const float cover =
                tween::clamp01(1.0f - std::fabs(highlight.cx() - r.cx()) / (r.w * 0.6f));
            // Disabled is visible: a finished title dims "Finished".
            const bool disabled = i == static_cast<int>(DialogAction::finish) && finished;
            const Color ink = gfx::mix(kInk, kPaper, cover).with_alpha(disabled ? 0.5f : 1.0f);
            const float label_w = fonts.semibold.measure(names[i], 25);
            const bool icon = i == static_cast<int>(DialogAction::favorite) || disabled;
            float lx = r.cx() - (label_w + (icon ? 34.0f : 0.0f)) * 0.5f;
            if (i == static_cast<int>(DialogAction::favorite))
            {
                const float pop = std::max(0.0f, card.star.value);
                list.star(lx + 11, r.cy(), 12 + 3 * pop, starred ? kStar : ink,
                          starred ? 0.0f : 2.5f);
                lx += 34;
            }
            else if (disabled)
            {
                check_mark(list, lx + 11, r.cy(), 18, ink);
                lx += 34;
            }
            ui::text(list, fonts.semibold, names[i], lx, r.cy() + 9, 25, ink);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        if (dialog_open_)
        {
            const ui::Hint hints[] = {{ui::Button::dpad, "Actions"},
                                      {ui::Button::cross, "Choose"},
                                      {ui::Button::circle, "Back"}};
            list.push_opacity(dialog_.value);
            ui::draw_hints(list, fonts, style, hints, 3, kRight, true);
            list.pop_opacity();
        }
        else
        {
            // The row is honest: with nothing on the shelf, only the buttons
            // that still do something are offered.
            const float in = tween::stagger(age_, 9, 0.07f, 0.5f) * (1.0f - dialog_.value);
            const ui::Hint hints[] = {{ui::Button::l2, "Filter", ui::Button::r2},
                                      {ui::Button::square, "Sort"},
                                      {ui::Button::triangle, "Favorite"},
                                      {ui::Button::cross, "Details"}};
            list.push_opacity(in * (1.0f - empty_.value));
            ui::draw_hints(list, fonts, style, hints, 4, kRight, true);
            list.pop_opacity();
            list.push_opacity(in * empty_.value);
            ui::draw_hints(list, fonts, style, hints, 1, kRight, true);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    std::vector<Card> cards_;         // by catalogue index
    std::vector<std::string> titles_; // fitted to the card width
    std::vector<bool> favorite_;
    std::vector<bool> finished_;
    std::vector<int> visible_; // catalogue indices on the table, in order
    int focus_ = 0;            // slot in visible_
    Filter filter_ = Filter::all;
    Filter empty_filter_ = Filter::favorites; // the last filter found empty
    Sort sort_ = Sort::title;
    Sort previous_sort_ = Sort::title;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion
    float drift_ = 0.0f; // backdrop time; stops under "Reduce motion"
    ui::Scroller scroll_;
    ui::SpringRect ring_;
    tween::Spring ring_alpha_;
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    ui::SpringRect chip_;
    tween::Spring empty_;
    tween::Timer sort_label_;
    Filter counted_filter_ = Filter::all; // what the header count says now ...
    int counted_ = 0;
    Filter previous_counted_filter_ = Filter::all; // ... and what it said before
    int previous_counted_ = 0;
    tween::Timer count_swap_;
    bool dialog_open_ = false;
    int dialog_item_ = 0;
    tween::Spring dialog_;
    int action_ = 0;
    int previous_action_ = 0;
    tween::Spring action_position_;
    ui::Pulse action_nudge_;
    tween::Timer describe_;
    ui::Confetti confetti_;
    std::uint32_t bursts_ = 0;
};

} // namespace

std::unique_ptr<app::Concept> make_paper(app::Context &context)
{
    return std::make_unique<Paper>(context);
}

} // namespace hui::concepts
