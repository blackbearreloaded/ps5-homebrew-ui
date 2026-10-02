// ps5-homebrew-ui - Design "Aurora Shelf": a console home screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The classic living-room launcher: a hero panel for the focused title above
// horizontal shelves of covers. What makes it feel finished:
//
//   - the backdrop takes the colours of the focused cover, eased, so the whole
//     screen breathes with the selection instead of sitting on a fixed theme;
//   - the hero text and artwork cross-fade with a small slide when the focus
//     changes, staggered so the title lands first;
//   - shelves scroll with springs, the focused card grows, and the focus ring
//     is a separate spring that glides between cards;
//   - Cross opens a frosted sheet over the screen with the details and
//     actions; the screen behind it dims, blurs and scales back slightly;
//   - every move has a sound placed in stereo where it happened, and the ends
//     of a shelf answer with a soft refusal and a nudge instead of silence.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

const Color kWhite = Color::rgb(0xffffff);

constexpr float kMargin = 96.0f;
constexpr float kCard = 196.0f; // card size at rest
constexpr float kCardGap = 26.0f;
constexpr float kCardGrow = 1.2f;     // focused card scale
constexpr float kShelfY = 730.0f;     // top of the focused shelf's cards
constexpr float kShelfPitch = 304.0f; // distance between shelves
constexpr int kActions = 3;

constexpr const char *kTechniques[] = {
    "Backdrop colours eased toward the focused cover's palette (ui::SpringColor)",
    "Hero text and artwork cross-fade with a staggered slide on every focus change",
    "Spring-driven shelf scrolling, card growth and a gliding focus ring",
    "Frosted details sheet: Frame::glass blurs the screen behind the overlay",
    "Stereo-panned focus sounds, pitched row changes, refusal nudge at the ends",
};

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.9f, 0, Direction::down, "shelf"},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.9f, action_bit(Action::confirm), Direction::none, "library"},
    {0.5f, 0, Direction::down},
    {0.7f, action_bit(Action::confirm), Direction::none, "details"},
    {0.6f, action_bit(Action::back)},
};

struct Shelf
{
    const char *title;
    std::vector<int> items; // catalogue indices
    int column = 0;
    ui::Scroller scroll;
};

class Aurora final : public app::Concept
{
  public:
    explicit Aurora(app::Context &context) : context_(context)
    {
        const auto &items = context.catalog.items();
        shelves_.resize(3);
        shelves_[0].title = "Continue playing";
        shelves_[1].title = "Your library";
        shelves_[2].title = "New this year";
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const demo::Item &item = items[static_cast<std::size_t>(i)];
            if (item.progress > 0.0f && item.progress < 1.0f && shelves_[0].items.size() < 9)
                shelves_[0].items.push_back(i);
            shelves_[1].items.push_back(i);
            if (item.year == 2026)
                shelves_[2].items.push_back(i);
        }
        favorite_.assign(items.size(), false);
        shown_ = previous_ = focused_item();
        apply_palette(true);
        ring_.snap(card_rect(0, 0, true));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "aurora",
            "Aurora Shelf",
            "A console home screen: hero panel, cover shelves, frosted details",
            "src/concepts/aurora.cpp",
            audio::SoundSet::glass,
            Color::rgb(0x7ff0d8),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        sheet_open_ = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (sheet_open_)
            update_sheet(input, feedback);
        else
            update_shelves(input, feedback);

        // ---- animation state ----
        if (focused_item() != shown_)
        {
            previous_ = shown_;
            shown_ = focused_item();
            hero_.start(context_.settings.reduced_motion ? 0.12f : 0.42f);
            apply_palette(false);
        }
        hero_.update(dt);
        for (ui::SpringColor &colour : palette_)
            colour.update(dt, 4.0f);
        row_position_.target = static_cast<float>(row_);
        row_position_.update(dt, 11.0f);
        for (int r = 0; r < static_cast<int>(shelves_.size()); ++r)
        {
            Shelf &shelf = shelves_[static_cast<std::size_t>(r)];
            const float start = static_cast<float>(shelf.column) * (kCard + kCardGap);
            shelf.scroll.reveal(start, start + kCard * kCardGrow, gfx::kVirtualWidth - kMargin,
                                kMargin * 2.2f);
            shelf.scroll.update(dt, 12.0f);
        }
        ring_.target(card_rect(row_, shelves_[static_cast<std::size_t>(row_)].column, true));
        ring_.update(dt, 20.0f);
        nudge_.update(dt, 9.0f);
        sheet_.target = sheet_open_ ? 1.0f : 0.0f;
        sheet_.update(dt, context_.settings.reduced_motion ? 40.0f : 13.0f);
        action_position_.target = static_cast<float>(action_);
        action_position_.update(dt, 22.0f);
        star_.update(dt, 5.0f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::aurora;
        frame.backdrop.colors[0] = palette_[0].value();
        frame.backdrop.colors[1] = palette_[1].value();
        frame.backdrop.colors[2] = palette_[2].value();
        frame.backdrop.colors[3] = palette_[3].value();
        frame.backdrop.time = clock_;

        gfx::DrawList &list = frame.scene;
        // The sheet pushes the screen back a little: the scene shrinks toward
        // its centre and dims while the overlay is up.
        const float back = sheet_.value;
        list.push_transform(1.0f - 0.035f * back, 960, 540, 0, 0);
        draw_top_bar(list);
        draw_hero(list);
        draw_shelves(list);
        list.pop_transform();
        if (back > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x05070f, 0.45f * back));

        if (back > 0.01f)
        {
            frame.glass = true;
            draw_sheet(frame.overlay, frame.glass_texture);
        }
        draw_hints(back > 0.5f ? frame.overlay : frame.scene);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    int focused_item() const
    {
        const Shelf &shelf = shelves_[static_cast<std::size_t>(row_)];
        return shelf.items[static_cast<std::size_t>(shelf.column)];
    }

    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(index)];
    }

    // The backdrop's four colours come from the focused cover.
    void apply_palette(bool snap)
    {
        const demo::Item &focused = item(focused_item());
        const Color targets[4] = {
            gfx::mix(focused.dark, Color::rgb(0x05060c), 0.35f),
            gfx::mix(focused.dark, focused.mid, 0.35f),
            focused.mid,
            gfx::mix(focused.mid, focused.accent, 0.55f),
        };
        for (int i = 0; i < 4; ++i)
        {
            if (snap)
                palette_[i].snap(targets[i]);
            else
                palette_[i].target(targets[i]);
        }
    }

    // Where a card sits. `focused` gives the grown rectangle the ring uses.
    Rect card_rect(int row, int column, bool focused) const
    {
        const Shelf &shelf = shelves_[static_cast<std::size_t>(row)];
        const float x =
            kMargin + static_cast<float>(column) * (kCard + kCardGap) - shelf.scroll.offset();
        const float y = kShelfY + (static_cast<float>(row) - row_position_.value) * kShelfPitch;
        if (!focused)
            return {x, y, kCard, kCard};
        const float grown = kCard * kCardGrow;
        return {x, y - (grown - kCard), grown, grown};
    }

    void update_shelves(const InputFrame &input, app::Feedback &feedback)
    {
        Shelf &shelf = shelves_[static_cast<std::size_t>(row_)];
        const int count = static_cast<int>(shelf.items.size());
        const int rows = static_cast<int>(shelves_.size());
        bool refused = false;
        switch (input.nav)
        {
        case Direction::left:
        case Direction::right:
        {
            const int next = shelf.column + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next < count)
            {
                shelf.column = next;
                feedback.play(audio::Cue::focus, 1.0f,
                              ui::pan_for_x(card_rect(row_, next, false).cx()));
            }
            else
            {
                refused = !input.nav_repeat;
                nudge_direction_ = input.nav == Direction::right ? 1.0f : -1.0f;
            }
            break;
        }
        case Direction::up:
        case Direction::down:
        {
            const int next = row_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < rows)
            {
                row_ = next;
                // Rows sound like steps of a scale: lower shelves, lower pitch.
                feedback.play(audio::Cue::tab, 1.12f - 0.08f * static_cast<float>(row_));
            }
            else
            {
                refused = !input.nav_repeat;
                nudge_direction_ = 0.0f;
            }
            break;
        }
        case Direction::none:
            break;
        }
        if (refused)
        {
            feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            feedback.rumble(0.25f, 0.05f);
            nudge_.trigger();
        }
        if (input.is_pressed(Action::confirm))
        {
            sheet_open_ = true;
            action_ = 0;
            action_position_.snap(0.0f);
            feedback.play(audio::Cue::open);
        }
        if (input.is_pressed(Action::north))
            toggle_favorite(feedback);
    }

    void update_sheet(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next =
                std::clamp(action_ + (input.nav == Direction::down ? 1 : -1), 0, kActions - 1);
            if (next != action_)
            {
                action_ = next;
                feedback.play(audio::Cue::focus, 1.0f, 0.35f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            if (action_ == 0)
            {
                feedback.play(audio::Cue::launch);
                feedback.rumble(0.7f, 0.18f);
                sheet_open_ = false;
            }
            else if (action_ == 1)
            {
                toggle_favorite(feedback);
            }
            else
            {
                feedback.play(audio::Cue::modal_close);
                sheet_open_ = false;
            }
        }
        if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            sheet_open_ = false;
        }
    }

    void toggle_favorite(app::Feedback &feedback)
    {
        const std::size_t index = static_cast<std::size_t>(focused_item());
        favorite_[index] = !favorite_[index];
        feedback.play(favorite_[index] ? audio::Cue::favorite_on : audio::Cue::favorite_off);
        if (favorite_[index])
            star_.trigger();
    }

    void draw_top_bar(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.05f, 0.5f);
        list.push_opacity(in);
        const char *tabs[] = {"Home", "Library", "Store", "Captures"};
        float x = kMargin;
        for (int i = 0; i < 4; ++i)
        {
            const bool active = i == 0;
            const float w =
                ui::text(list, active ? fonts.semibold : fonts.regular, tabs[i], x,
                         92 - 10 * (1.0f - in), 26, kWhite.with_alpha(active ? 1.0f : 0.55f));
            if (active)
                list.rounded_rect({x, 104, w, 4}, 2, palette_[3].value());
            x += w + 44;
        }
        ui::text(list, fonts.regular, "21:47", 1824 - 64, 92, 26, kWhite.with_alpha(0.8f),
                 gfx::Align::right);
        list.circle(1800, 82, 22, palette_[3].value());
        ui::text(list, fonts.semibold, "B", 1800, 91, 22, Color::rgb(0x0b0d16), gfx::Align::center);
        list.pop_opacity();
    }

    // One title's hero block, drawn at an opacity and a horizontal offset so
    // two of them can cross-fade.
    void draw_hero_item(gfx::DrawList &list, int index, float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(index);
        char text[96];
        list.push_opacity(alpha);

        // Artwork, floating gently, with a glow in its own accent colour.
        const float bob = context_.settings.reduced_motion ? 0.0f : std::sin(clock_ * 0.8f) * 6.0f;
        const Rect art{1316 + slide * 1.6f, 132 + bob, 440, 440};
        list.glow(art.inset(30), 60, 90, it.accent.with_alpha(0.3f));
        list.shadow({art.x, art.y + 26, art.w, art.h}, 36, 46, Color::rgb(0x000000, 0.55f));
        list.image(it.cover, art, gfx::kCanvasUv, kWhite, 36);
        list.bordered_rect(art, 36, Color::rgb(0x000000, 0.0f), 2, kWhite.with_alpha(0.16f));

        const float x = kMargin + slide;
        ui::text(list, fonts.semibold, ui::upper(shelves_[static_cast<std::size_t>(row_)].title), x,
                 212, 20, it.accent, gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, it.title, x - 4, 304, 88, kWhite);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %d  \xC2\xB7  %s", it.genre, it.year,
                      it.studio);
        float cursor =
            x + ui::text(list, fonts.regular, text, x, 358, 26, kWhite.with_alpha(0.78f));
        list.star(cursor + 42, 349, 12, Color::rgb(0xffd166));
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
        ui::text(list, fonts.semibold, text, cursor + 62, 358, 26, kWhite);
        ui::paragraph(list, fonts.regular, it.blurb, x, 414, 28, 820, 40, kWhite.with_alpha(0.86f),
                      2);

        // Progress, then the two actions.
        const Rect bar{x, 496, 420, 8};
        list.rounded_rect(bar, 4, kWhite.with_alpha(0.18f));
        if (it.progress > 0.0f)
            list.rounded_rect({bar.x, bar.y, std::max(8.0f, bar.w * it.progress), bar.h}, 4,
                              it.accent);
        if (it.hours > 0)
            std::snprintf(text, sizeof(text), "%d%%  \xC2\xB7  %d h played",
                          static_cast<int>(it.progress * 100.0f + 0.5f), it.hours);
        else
            std::snprintf(text, sizeof(text), "Not started");
        ui::text(list, fonts.regular, text, bar.x + bar.w + 24, 508, 22, kWhite.with_alpha(0.7f));

        const Rect play{x, 540, 220, 64};
        list.glow(play, 32, 18, it.accent.with_alpha(0.35f));
        list.rounded_rect(play, 32, kWhite);
        ui::draw_button(list, fonts, ui::GlyphStyle::light(), ui::Button::cross, play.x + 22,
                        play.cy(), 34);
        ui::text(list, fonts.semibold, it.progress > 0.0f ? "Resume" : "Play", play.x + 72,
                 play.cy() + 10, 28, Color::rgb(0x0b0d16));
        const Rect more{x + 240, 540, 64, 64};
        list.bordered_rect(more, 32, kWhite.with_alpha(0.1f), 2, kWhite.with_alpha(0.3f));
        const bool starred = favorite_[static_cast<std::size_t>(index)];
        list.star(more.cx(), more.cy(), 16 + 10 * star_.value,
                  starred ? Color::rgb(0xffd166) : kWhite.with_alpha(0.85f), starred ? 0.0f : 2.5f);
        list.pop_opacity();
    }

    void draw_hero(gfx::DrawList &list) const
    {
        const float in = tween::stagger(age_, 1, 0.08f, 0.6f);
        list.push_opacity(in);
        if (hero_.running)
        {
            // The old title leaves quickly; the new one arrives a beat later.
            const float t = hero_.progress();
            draw_hero_item(list, previous_, 1.0f - tween::smoothstep(t * 2.2f),
                           -36.0f * tween::cubic_in(tween::clamp01(t * 2.2f)));
            const float arrive = tween::clamp01((t - 0.25f) / 0.75f);
            draw_hero_item(list, shown_, tween::smoothstep(arrive),
                           44.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_hero_item(list, shown_, 1.0f, 30.0f * (1.0f - in));
        }
        list.pop_opacity();
    }

    void draw_shelves(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float nudge = ui::shake(nudge_.value, clock_, 16.0f, 8.0f) * nudge_direction_;
        for (int r = 0; r < static_cast<int>(shelves_.size()); ++r)
        {
            const Shelf &shelf = shelves_[static_cast<std::size_t>(r)];
            // Shelves above the focused one are gone; the one below peeks in.
            const float distance = static_cast<float>(r) - row_position_.value;
            const float visible =
                tween::clamp01(1.0f + distance * 2.5f) *
                (distance > 0.0f ? 1.0f - 0.45f * tween::clamp01(distance) : 1.0f);
            if (visible <= 0.01f)
                continue;
            const float in = tween::stagger(age_, 3 + r, 0.09f, 0.6f);
            list.push_opacity(visible * in);
            const float y = kShelfY + distance * kShelfPitch + 40.0f * (1.0f - in);
            ui::text(list, fonts.semibold, shelf.title, kMargin, y - 62, 24,
                     kWhite.with_alpha(r == row_ ? 0.95f : 0.6f));
            for (int c = 0; c < static_cast<int>(shelf.items.size()); ++c)
            {
                Rect rect = card_rect(r, c, false);
                rect.y = y;
                if (rect.x > gfx::kVirtualWidth || rect.x + rect.w < -80.0f)
                    continue;
                const bool focused = r == row_ && c == shelf.column;
                if (focused)
                    continue; // drawn last, on top
                const demo::Item &it = item(shelf.items[static_cast<std::size_t>(c)]);
                // Cards to the right of a grown card make room for it.
                if (r == row_ && c > shelf.column)
                    rect.x += kCard * (kCardGrow - 1.0f);
                list.image(it.cover, rect, gfx::kCanvasUv, kWhite.with_alpha(0.82f), 22);
            }
            list.pop_opacity();
        }

        // The focused card: grown, lifted, ringed.
        const Shelf &shelf = shelves_[static_cast<std::size_t>(row_)];
        const demo::Item &it = item(shelf.items[static_cast<std::size_t>(shelf.column)]);
        Rect ring = ring_.value();
        ring.x += nudge;
        const float in = tween::stagger(age_, 3 + row_, 0.09f, 0.6f);
        list.push_opacity(in);
        list.shadow({ring.x, ring.y + 18, ring.w, ring.h}, 26, 34, Color::rgb(0x000000, 0.6f));
        list.glow(ring, 26, 26, it.accent.with_alpha(0.4f + 0.15f * ui::breathe(clock_)));
        list.image(it.cover, ring, gfx::kCanvasUv, kWhite, 26);
        list.bordered_rect(ring.inset(-5), 30, Color::rgb(0x000000, 0.0f), 4, kWhite);
        if (favorite_[static_cast<std::size_t>(focused_item())])
            list.star(ring.x + ring.w - 26, ring.y + 26, 13, Color::rgb(0xffd166));
        list.pop_opacity();
    }

    void draw_sheet(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(focused_item());
        const float t = sheet_.value;
        char text[96];
        constexpr float kHeight = 500.0f;
        const Rect sheet{120, gfx::kVirtualHeight - kHeight * t - 40.0f * t + 60.0f * (1.0f - t),
                         1680, kHeight};
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.shadow({sheet.x, sheet.y + 20, sheet.w, sheet.h}, 44, 60, Color::rgb(0x000000, 0.5f));
        // Frosted panel: the blurred screen, a tint, then a hairline of light.
        list.glass(glass, sheet, 44, kWhite);
        list.rounded_rect(sheet, 44,
                          gfx::mix(it.dark, Color::rgb(0x0b0d16), 0.5f).with_alpha(0.62f));
        list.bordered_rect(sheet, 44, Color::rgb(0x000000, 0.0f), 1.5f, kWhite.with_alpha(0.22f));

        const Rect art{sheet.x + 56, sheet.y + 56, 300, 300};
        list.image(it.cover, art, gfx::kCanvasUv, kWhite, 28);
        const float x = art.x + art.w + 56;
        ui::text(list, fonts.semibold, ui::upper(it.genre), x, sheet.y + 92, 20, it.accent,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, it.title, x - 2, sheet.y + 156, 60, kWhite);
        ui::paragraph(list, fonts.regular, it.blurb, x, sheet.y + 208, 25, 700, 36,
                      kWhite.with_alpha(0.82f), 2);

        // Three stat tiles; the first is a gauge drawn with arcs.
        const float tiles_y = sheet.y + 300;
        const char *labels[] = {"RATING", "PLAYED", "PLAYERS"};
        for (int i = 0; i < 3; ++i)
        {
            const float appear = tween::stagger(t, i, 0.12f, 0.6f);
            const Rect tile{x + static_cast<float>(i) * 236, tiles_y + 24 * (1.0f - appear), 216,
                            120};
            list.push_opacity(appear);
            list.rounded_rect(tile, 22, kWhite.with_alpha(0.08f));
            ui::text(list, fonts.semibold, labels[i], tile.x + 22, tile.y + 36, 15,
                     kWhite.with_alpha(0.55f), gfx::Align::left, 3.0f);
            if (i == 0)
            {
                std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
                list.arc(tile.x + 166, tile.y + 62, 32, 7, 0.0f, 6.2831853f,
                         kWhite.with_alpha(0.14f));
                list.arc(tile.x + 166, tile.y + 62, 32, 7, 0.0f,
                         6.2831853f * it.rating / 5.0f * appear, it.accent);
            }
            else if (i == 1)
            {
                std::snprintf(text, sizeof(text), "%d h", it.hours);
            }
            else
            {
                std::snprintf(text, sizeof(text), "1\xE2\x80\x93%d", it.players);
                if (it.players == 1)
                    std::snprintf(text, sizeof(text), "1");
            }
            ui::text(list, fonts.semibold, text, tile.x + 22, tile.y + 92, 40, kWhite);
            list.pop_opacity();
        }

        // Actions: one highlight that springs between the rows.
        const bool starred = favorite_[static_cast<std::size_t>(focused_item())];
        const char *actions[kActions] = {it.progress > 0.0f ? "Resume" : "Play",
                                         starred ? "Remove from favorites" : "Add to favorites",
                                         "Close"};
        const float ax = sheet.x + sheet.w - 56 - 420;
        const float ay = sheet.y + 76;
        list.rounded_rect({ax, ay + action_position_.value * 84, 420, 72}, 36, kWhite);
        for (int i = 0; i < kActions; ++i)
        {
            const bool focused = i == action_;
            const float y = ay + static_cast<float>(i) * 84;
            if (!focused)
                list.bordered_rect({ax, y, 420, 72}, 36, kWhite.with_alpha(0.06f), 1.5f,
                                   kWhite.with_alpha(0.18f));
            ui::text(list, fonts.semibold, actions[i], ax + 36, y + 46, 26,
                     focused ? Color::rgb(0x0b0d16) : kWhite.with_alpha(0.9f));
        }
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        if (sheet_open_)
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Choose"}, {ui::Button::circle, "Back"}};
            ui::draw_hints(list, fonts, style, hints, 2, 1824, true);
        }
        else
        {
            list.push_opacity(tween::stagger(age_, 8, 0.08f, 0.5f) * (1.0f - sheet_.value));
            const ui::Hint hints[] = {{ui::Button::cross, "Details"},
                                      {ui::Button::triangle, "Favorite"}};
            ui::draw_hints(list, fonts, style, hints, 2, 1824, true);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    std::vector<Shelf> shelves_;
    std::vector<bool> favorite_;
    int row_ = 0;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion
    int shown_ = 0;      // the title the hero shows
    int previous_ = 0;   // ... and the one it is fading out
    tween::Timer hero_;
    ui::SpringColor palette_[4];
    tween::Spring row_position_;
    ui::SpringRect ring_;
    ui::Pulse nudge_;
    float nudge_direction_ = 0.0f;
    ui::Pulse star_;
    bool sheet_open_ = false;
    tween::Spring sheet_;
    int action_ = 0;
    tween::Spring action_position_;
};

} // namespace

std::unique_ptr<app::Concept> make_aurora(app::Context &context)
{
    return std::make_unique<Aurora>(context);
}

} // namespace hui::concepts
