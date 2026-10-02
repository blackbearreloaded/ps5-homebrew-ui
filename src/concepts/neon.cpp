// ps5-homebrew-ui - Design "Neon Arcade": the main menu of a synthwave racer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The title screen of an invented racing game, "Lumen Drift": a wordmark that
// is a neon sign, a vertical main menu on the left and a preview panel on the
// right that shows what the focused entry leads to. PLAY opens a second
// level, a row of track cards, and Cross there fires a one-second "GO"
// moment. What makes it feel finished:
//
//   - light is the material. Nothing is merely coloured: a tube is a stack of
//     glows of decreasing alpha around a thin hot core (neon_bloom and
//     neon_edge), text is drawn as a magenta and a cyan copy either side of a
//     white one over a soft glow (neon_text), and bars, arcs and lines get
//     the same layered treatment;
//   - the focus is one tube that glides between the entries on an underdamped
//     spring, so it overshoots a little and settles; its width follows the
//     label, the label slides right and lights up;
//   - the preview cross-fades with a slide, then assembles with a stagger:
//     bars fill, the ring sweeps, numbers count up, the car is traced line by
//     line;
//   - moving down the menu descends a major scale, so the position can be
//     heard; the ends refuse softly and stay silent while a direction is held;
//   - the sign flickers on when the screen opens and breathes afterwards, the
//     floor rushes during "GO", and all of that stops under "Reduce motion".

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kWhite = Color::rgb(0xffffff);
const Color kMagenta = Color::rgb(0xff2bd6);
const Color kCyan = Color::rgb(0x22e8ff);
const Color kSun = Color::rgb(0xffc247);   // the one warm accent: numbers that matter
const Color kDim = Color::rgb(0xb7a3ea);   // unlit lettering
const Color kNight = Color::rgb(0x0c0522); // the dark behind every sign
const Color kSkyTop = Color::rgb(0x090220);
const Color kHorizon = Color::rgb(0x471268);
const Color kSunLow = Color::rgb(0xff9a3d); // the backdrop's sun

constexpr float kPi = 3.14159265f;
constexpr float kMargin = 96.0f;

// The wordmark.
constexpr float kLogoBaseline = 172.0f;
constexpr float kLogoSize = 88.0f;

// The main menu.
constexpr int kItems = 6;
constexpr float kMenuY = 304.0f; // top of the first row
constexpr float kRowH = 72.0f;
constexpr float kRowPitch = 82.0f;
constexpr float kLabelSize = 44.0f;
constexpr float kLabelTrack = 5.0f;
constexpr float kLabelPad = 30.0f; // tube edge to lettering: the lit label steps in by this
constexpr float kTubeRadius = 18.0f;

// The preview panel.
constexpr Rect kPanel{1112.0f, 236.0f, 712.0f, 596.0f};
constexpr float kPanelPad = 44.0f;
constexpr float kPanelInner = 624.0f; // kPanel.w - 2 * kPanelPad
constexpr float kPanelRadius = 28.0f;

// The track selector.
constexpr int kTracks = 5;
constexpr float kCard = 264.0f;
constexpr float kCardGap = 40.0f;
constexpr float kCardPitch = kCard + kCardGap;
constexpr float kCardGrow = 0.16f; // extra scale of the focused card
constexpr float kCardLift = 10.0f;
constexpr float kTrackX = (gfx::kVirtualWidth - (kTracks * kCard + (kTracks - 1) * kCardGap)) / 2;
constexpr float kTrackY = 326.0f;
constexpr Rect kInfoPane{430.0f, 656.0f, 1060.0f, 240.0f}; // the focused track's facts
constexpr float kGoSeconds = 1.0f;

// Motion: snappy, with a little life in the highlight.
constexpr float kFocusOmega = 24.0f;
constexpr float kFocusDamping = 0.62f; // below 1: the tube overshoots, then settles

// Focus pitches for the six entries: F E D C B A, a major scale going down.
constexpr float kScale[kItems] = {1.335f, 1.260f, 1.122f, 1.0f, 0.944f, 0.841f};

constexpr const char *kTechniques[] = {
    "Neon from plain shapes: layered glows of falling alpha around a thin hot core",
    "Chromatic sign lettering: magenta and cyan copies either side of white, over a glow",
    "A focus tube on an underdamped spring: it overshoots, settles and follows the label width",
    "Previews that cross-fade, then assemble: bars fill, a ring sweeps, numbers count up",
    "Focus cue pitched down a major scale by menu position, panned to where it happened",
    "A second level that slides in, and a one-second launch moment with flash and zoom",
};

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::down},
    {1.4f, 0, Direction::down, "career"},
    {1.4f, 0, Direction::up, "garage"},
    {0.25f, 0, Direction::up},
    {0.6f, action_bit(Action::confirm)}, // PLAY: into the track selector
    {0.6f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.9f, action_bit(Action::confirm), Direction::none, "tracks"},
    {0.42f, 0, Direction::none, "go"},
    {1.0f, action_bit(Action::back)},
    {0.5f, 0, Direction::down},
    {0.12f, 0, Direction::down},
    {0.12f, 0, Direction::down},
    {0.12f, 0, Direction::down},
    {0.12f, 0, Direction::down},
    {0.6f, action_bit(Action::confirm)}, // QUIT: the dialog
    {0.9f, action_bit(Action::back), Direction::none, "quit"},
    {0.4f, 0, Direction::up},
    {0.12f, 0, Direction::up},
    {0.12f, 0, Direction::up},
    {0.12f, 0, Direction::up},
    {0.12f, 0, Direction::up},
};

struct MenuItem
{
    const char *label;
    const char *caption; // small tracked line at the top of its preview
    const char *title;   // nullptr: the preview names the selected track
};
constexpr MenuItem kMenu[kItems] = {
    {"PLAY", "NEXT RACE", nullptr},
    {"CAREER", "CAREER", "Season 3"},
    {"GARAGE", "GARAGE", "VX-9 Phantom"},
    {"LEADERBOARD", "LEADERBOARD  \xC2\xB7  THIS WEEK", nullptr},
    {"OPTIONS", "OPTIONS", "Quick settings"},
    {"QUIT", "QUIT", "See you on the grid"},
};
enum : int
{
    kPlay = 0,
    kCareer = 1,
    kGarage = 2,
    kLeaderboard = 3,
    kOptions = 4,
    kQuit = 5,
};

// The five circuits. Covers, titles and accents come from the shared
// catalogue; the racing facts are this design's own invention.
struct Track
{
    int item; // catalogue index
    const char *line;
    const char *length;
    const char *laps;
    const char *best;
    int stats[3]; // speed, technique, danger: 0..100
};
constexpr Track kTrackList[kTracks] = {
    {3,
     "Container canyons and a wet quay. Brake late: the water is close.",
     "4.8 km",
     "3",
     "1:43.560",
     {78, 64, 52}},
    {13,
     "Long sweepers under purple lamps. The road hums on the right line.",
     "6.1 km",
     "3",
     "2:08.214",
     {92, 48, 35}},
    {11,
     "Four sectors, four tunnels, and no straight longer than a breath.",
     "3.9 km",
     "5",
     "1:21.077",
     {56, 95, 70}},
    {6,
     "An orbital ring road. Mind the solar weather on the back straight.",
     "9.4 km",
     "2",
     "3:02.845",
     {98, 40, 61}},
    {12,
     "Hairpins up a sleeping volcano. The summit gate closes at dawn.",
     "5.5 km",
     "4",
     "2:31.392",
     {61, 88, 93}},
};
constexpr const char *kStatNames[3] = {"SPEED", "TECHNIQUE", "DANGER"};

struct Entry
{
    const char *name;
    const char *time;
    bool you;
};
constexpr Entry kBoard[] = {
    {"KESTREL", "1:42.318", false}, {"MIRA.V", "1:42.904", false},  {"0XFADE", "1:43.127", false},
    {"NOVA-7", "1:43.560", true},   {"LOWTIDE", "1:44.012", false}, {"QUILL", "1:44.871", false},
};
constexpr int kBoardRows = 6;

// The garage car: a side view traced with straight tubes in a 600 x 170 box.
struct Segment
{
    float x1, y1, x2, y2;
    bool trim; // drawn in magenta instead of cyan
};
constexpr float kWheelRear = 120.0f;
constexpr float kWheelFront = 478.0f;
constexpr float kWheelY = 130.0f;
constexpr Segment kCar[] = {
    {79, 130, 22, 130, false},   {22, 130, 16, 98, false},    {16, 98, 150, 80, false},
    {150, 80, 222, 44, false},   {222, 44, 328, 40, false},   {328, 40, 424, 80, false},
    {424, 80, 548, 98, false},   {548, 98, 588, 114, false},  {588, 114, 586, 130, false},
    {586, 130, 519, 130, false}, {437, 130, 161, 130, false}, {16, 98, 10, 66, false},
    {2, 66, 58, 66, false},      {236, 55, 320, 51, true},    {320, 51, 396, 81, true},
    {396, 81, 190, 81, true},    {190, 81, 236, 55, true},    {150, 106, 420, 106, true},
};
constexpr int kCarSegments = static_cast<int>(sizeof(kCar) / sizeof(kCar[0]));

// ---- neon from plain shapes ------------------------------------------------

Color clear_of(Color c)
{
    return {c.r, c.g, c.b, 0.0f};
}

// The light a tube throws around itself: three glows, each tighter and
// stronger than the one before. A single glow reads as a blurred box; the
// stack has the long faint tail and the bright rim of a real bloom. Draw it
// before whatever the tube frames, because a glow also fills its inside.
void neon_bloom(gfx::DrawList &list, const Rect &r, float radius, Color core, Color halo,
                float power)
{
    list.glow(r.inset(-4), radius + 4, 64, halo.with_alpha(0.26f * power));
    list.glow(r, radius, 28, halo.with_alpha(0.30f * power));
    list.glow(r, radius, 11, core.with_alpha(0.46f * power));
}

// The tube itself: two faint wide borders (the bloom on the inside), the
// coloured glass, and a one-pixel white line down its middle, the hot gas.
void neon_edge(gfx::DrawList &list, const Rect &r, float radius, Color core, float power)
{
    const Color none = clear_of(core);
    list.bordered_rect(r, radius, none, 10, core.with_alpha(0.07f * power));
    list.bordered_rect(r, radius, none, 6, core.with_alpha(0.15f * power));
    list.bordered_rect(r, radius, none, 3, core.with_alpha(std::min(1.0f, 0.45f + 0.55f * power)));
    list.bordered_rect(r.inset(1), radius - 1, clear_of(kWhite), 1,
                       kWhite.with_alpha(std::min(1.0f, 0.75f * power)));
}

// A straight tube: the same idea with capsules.
void neon_line(gfx::DrawList &list, float x1, float y1, float x2, float y2, Color color,
               float power)
{
    list.line(x1, y1, x2, y2, 13, color.with_alpha(0.06f * power));
    list.line(x1, y1, x2, y2, 7, color.with_alpha(0.15f * power));
    list.line(x1, y1, x2, y2, 2.8f, color.with_alpha(power));
}

// A bent tube.
void neon_arc(gfx::DrawList &list, float cx, float cy, float radius, float thickness, float start,
              float sweep, Color color, float power)
{
    list.arc(cx, cy, radius + 8, thickness + 16, start, sweep, color.with_alpha(0.05f * power));
    list.arc(cx, cy, radius + 4, thickness + 8, start, sweep, color.with_alpha(0.13f * power));
    list.arc(cx, cy, radius, thickness, start, sweep, color.with_alpha(power));
}

// Sign lettering: a soft glow, a magenta copy to the right, a cyan copy to
// the left, white on top. The coloured copies peek out either side of the
// white one, which is what makes it read as light instead of as print.
float neon_text(gfx::DrawList &list, const ui::FontRef &font, std::string_view value, float x,
                float baseline, float size, float tracking, gfx::Align align, float power,
                float split)
{
    const float width = font.measure(value, size, tracking);
    float left = x;
    if (align == gfx::Align::center)
        left = x - width * 0.5f;
    else if (align == gfx::Align::right)
        left = x - width;
    // The glow is a pill thinner than its own softness, so it never reaches
    // full strength anywhere: a haze, not a box.
    const Rect haze{left + size * 0.15f, baseline - size * 0.6f, width - size * 0.3f, size * 0.5f};
    list.glow(haze, size * 0.25f, size * 0.7f, kMagenta.with_alpha(0.46f * power));
    list.glow(haze, size * 0.25f, size * 0.32f, kCyan.with_alpha(0.22f * power));
    ui::text(list, font, value, x + split, baseline + split * 0.34f, size,
             kMagenta.with_alpha(0.9f * power), align, tracking);
    ui::text(list, font, value, x - split, baseline - split * 0.34f, size,
             kCyan.with_alpha(0.9f * power), align, tracking);
    ui::text(list, font, value, x, baseline, size, kWhite.with_alpha(0.3f + 0.7f * power), align,
             tracking);
    return width;
}

// A lit bar on a dark track, with a white-hot tip.
void neon_bar(gfx::DrawList &list, const Rect &r, float amount, Color color)
{
    const float radius = r.h * 0.5f;
    list.rounded_rect(r, radius, kWhite.with_alpha(0.12f));
    if (amount <= 0.0f)
        return;
    const Rect lit{r.x, r.y, std::max(r.h, r.w * std::min(amount, 1.0f)), r.h};
    list.glow(lit, radius, 18, color.with_alpha(0.22f));
    list.glow(lit, radius, 7, color.with_alpha(0.35f));
    list.rounded_rect(lit, radius, color);
    list.circle(lit.x + lit.w - radius, lit.cy(), radius * 0.62f, kWhite.with_alpha(0.9f));
}

// A dark pane with a thin magenta tube around it: quieter than the focus
// tube, so there is still only one brightest thing on the screen.
void neon_pane(gfx::DrawList &list, const Rect &r, float radius)
{
    list.glow(r, radius, 46, kMagenta.with_alpha(0.11f));
    list.shadow({r.x, r.y + 16, r.w, r.h}, radius, 40, Color::rgb(0x000000, 0.45f));
    list.gradient_rect(r, radius, Color::rgb(0x1a0a3c, 0.94f), kNight.with_alpha(0.95f));
    list.bordered_rect(r, radius, clear_of(kMagenta), 5, kMagenta.with_alpha(0.12f));
    list.bordered_rect(r, radius, clear_of(kMagenta), 2, kMagenta.with_alpha(0.75f));
}

// The hint row on a small dark plate: the floor's bright lines run under
// that corner, and a hint must never be the thing that is hard to read.
void hint_plate(gfx::DrawList &list, const ui::Fonts &fonts, const ui::Hint *hints, int count)
{
    const ui::HintLayout layout;
    float width = 0.0f;
    for (int i = 0; i < count; ++i)
        width += ui::button_width(hints[i].button, layout.size) + 12.0f +
                 fonts.regular.measure(hints[i].label, layout.text_size) +
                 (i + 1 < count ? layout.item_gap : 0.0f);
    const float right = gfx::kVirtualWidth - kMargin;
    const Rect plate{right - width - 22, layout.cy - 32, width + 44, 64};
    list.rounded_rect(plate, 32, kNight.with_alpha(0.78f));
    list.bordered_rect(plate, 32, clear_of(kMagenta), 1.5f, kMagenta.with_alpha(0.4f));
    ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, count, right, true, layout);
}

// How brightly the sign burns. It stutters while it strikes, then breathes;
// now and then it dips for a few frames, as tubes do.
float sign_power(float age, float clock, bool reduced)
{
    if (reduced)
        return 1.0f;
    if (age < 0.07f)
        return 0.0f;
    if (age < 0.13f)
        return 0.9f;
    if (age < 0.21f)
        return 0.12f;
    if (age < 0.29f)
        return 0.8f;
    if (age < 0.34f)
        return 0.25f;
    const float strike = tween::cubic_out((age - 0.34f) / 0.3f);
    float power = (0.86f + 0.14f * ui::breathe(clock, 3.4f)) * (0.6f + 0.4f * strike);
    const float phase = std::fmod(clock, 7.3f);
    if (age > 2.0f && ((phase > 5.0f && phase < 5.05f) || (phase > 5.12f && phase < 5.16f)))
        power *= 0.55f;
    return power;
}

class Neon final : public app::Concept
{
  public:
    explicit Neon(app::Context &context) : context_(context)
    {
        for (int i = 0; i < kItems; ++i)
            label_w_[i] = context.fonts.display.measure(kMenu[i].label, kLabelSize, kLabelTrack);
        tube_y_.snap(row_y(0));
        tube_w_.snap(tube_width(0));
        item_focus_[0].snap(1.0f);
        card_focus_[0].snap(1.0f);
        frame_x_.snap(card_cx(0));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "neon",
            "Neon Arcade",
            "A synthwave racer's main menu: neon sign, gliding tube, live previews",
            "src/concepts/neon.cpp",
            audio::SoundSet::glass,
            kMagenta,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        // The previews assemble again once the panel has arrived.
        preview_age_ = -0.3f;
        dialog_open_ = false;
        go_.running = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool reduced = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        preview_age_ += dt;

        // The launch moment owns the screen for its second: presses made
        // while the screen is white would act on things the player cannot see.
        if (go_.running)
            go_.update(dt);
        else if (dialog_open_)
            update_dialog(input, feedback);
        else if (in_tracks_)
            update_tracks(input, feedback);
        else
            update_menu(input, feedback);

        // ---- animation state ----
        // The floor only rushes during "GO"; under reduced motion it stands still.
        const float boost = go_.running ? tween::ping(go_.progress()) : 0.0f;
        if (!reduced)
            road_ += dt * (1.0f + 9.0f * boost);

        if (focus_ != shown_)
        {
            previous_ = shown_;
            shown_ = focus_;
            swap_.start(reduced ? 0.12f : 0.34f);
            preview_age_ = reduced ? 0.0f : -0.12f;
        }
        swap_.update(dt);
        if (track_ != track_shown_)
        {
            track_previous_ = track_shown_;
            track_shown_ = track_;
            track_swap_.start(reduced ? 0.12f : 0.3f);
        }
        track_swap_.update(dt);

        const float damping = reduced ? 1.0f : kFocusDamping;
        tube_y_.target = row_y(focus_);
        tube_y_.update(dt, kFocusOmega, damping);
        tube_w_.target = tube_width(focus_);
        tube_w_.update(dt, 20.0f);
        for (int i = 0; i < kItems; ++i)
        {
            item_focus_[i].target = i == focus_ ? 1.0f : 0.0f;
            item_focus_[i].update(dt, 22.0f);
        }
        frame_x_.target = card_cx(track_);
        frame_x_.update(dt, 22.0f, damping);
        for (int i = 0; i < kTracks; ++i)
        {
            card_focus_[i].target = i == track_ ? 1.0f : 0.0f;
            card_focus_[i].update(dt, 20.0f);
        }
        press_.update(dt, 7.0f);
        refuse_.update(dt, 9.0f);
        level_.target = in_tracks_ ? 1.0f : 0.0f;
        level_.update(dt, reduced ? 40.0f : 13.0f);
        dialog_.target = dialog_open_ ? 1.0f : 0.0f;
        dialog_.update(dt, reduced ? 40.0f : 14.0f);
        choice_position_.target = static_cast<float>(choice_);
        choice_position_.update(dt, 22.0f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::grid;
        frame.backdrop.colors[0] = kSkyTop;
        frame.backdrop.colors[1] = kHorizon;
        frame.backdrop.colors[2] = kMagenta;
        frame.backdrop.colors[3] = kSunLow;
        frame.backdrop.time = road_;
        // A light touch of tube television: enough to bind the glows together,
        // not enough to hurt the lettering.
        frame.post.mode = gfx::BackdropMode::scanlines;
        frame.post.params[0] = 0.10f;
        frame.post.params[1] = 0.34f;

        gfx::DrawList &list = frame.scene;
        const float back = dialog_.value;
        // The bright horizon sits behind the lower menu rows: a soft shade on
        // the left keeps the lettering on a dark ground.
        list.gradient_rect_h({0, 0, 900, gfx::kVirtualHeight}, 0,
                             kNight.with_alpha(0.55f * (1.0f - level_.value)),
                             kNight.with_alpha(0.0f));
        list.push_transform(1.0f - 0.03f * back, 960, 540, 0, 0);
        draw_logo(list);
        draw_driver(list);
        draw_menu(list);
        draw_panel(list);
        draw_tracks(list);
        list.pop_transform();
        if (back > 0.01f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              kNight.with_alpha(0.55f * back));
            frame.glass = true;
            draw_dialog(frame.overlay, frame.glass_texture);
        }
        if (go_.running)
            draw_go(frame.overlay);
        draw_hints(frame);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    const demo::Item &track_item(int track) const
    {
        return context_.catalog[static_cast<std::size_t>(kTrackList[track].item)];
    }

    static float row_y(int index)
    {
        return kMenuY + static_cast<float>(index) * kRowPitch;
    }
    float tube_width(int index) const
    {
        return label_w_[index] + 2.0f * kLabelPad;
    }
    static float card_cx(int index)
    {
        return kTrackX + static_cast<float>(index) * kCardPitch + kCard * 0.5f;
    }

    // ---- input ----

    void refuse(app::Feedback &feedback, float dx, float dy)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        refuse_.trigger();
        refuse_dx_ = dx;
        refuse_dy_ = dy;
    }

    void update_menu(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int step = input.nav == Direction::down ? 1 : -1;
            const int next = focus_ + step;
            if (next >= 0 && next < kItems)
            {
                focus_ = next;
                // Each entry has its own note; going down the menu goes down the scale.
                feedback.play(audio::Cue::focus, kScale[focus_],
                              ui::pan_for_x(kMargin + tube_width(focus_) * 0.5f));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, static_cast<float>(step));
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            if (focus_ == kPlay)
            {
                in_tracks_ = true;
                feedback.play(audio::Cue::open);
            }
            else if (focus_ == kQuit)
            {
                dialog_open_ = true;
                choice_ = 0; // the safe answer
                choice_position_.snap(0.0f);
                feedback.play(audio::Cue::modal_open);
            }
            else
            {
                feedback.play(audio::Cue::select, kScale[focus_]);
                press_.trigger();
            }
        }
    }

    void update_tracks(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int step = input.nav == Direction::right ? 1 : -1;
            const int next = track_ + step;
            if (next >= 0 && next < kTracks)
            {
                track_ = next;
                track_direction_ = static_cast<float>(step);
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(card_cx(track_)));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, static_cast<float>(step), 0.0f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            feedback.play(audio::Cue::launch);
            feedback.rumble(0.7f, 0.18f);
            go_.start(kGoSeconds);
            press_.trigger();
        }
        else if (input.is_pressed(Action::back))
        {
            in_tracks_ = false;
            feedback.play(audio::Cue::back);
            preview_age_ = -0.15f; // the panel slides back in and assembles again
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int step = input.nav == Direction::right ? 1 : -1;
            const int next = choice_ + step;
            if (next >= 0 && next <= 1)
            {
                choice_ = next;
                feedback.play(audio::Cue::focus, 1.0f, next == 0 ? -0.15f : 0.15f);
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, static_cast<float>(step), 0.0f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            dialog_open_ = false;
            if (choice_ == 0)
            {
                feedback.play(audio::Cue::modal_close);
            }
            else
            {
                // A reference app has nowhere to quit to: the sign goes dark
                // and strikes again, as the game would on its next start.
                feedback.play(audio::Cue::select);
                age_ = 0.0f;
                preview_age_ = -0.3f;
                focus_ = kPlay;
                tube_y_.snap(row_y(focus_));
                tube_w_.snap(tube_width(focus_));
            }
        }
        else if (input.is_pressed(Action::back))
        {
            dialog_open_ = false;
            feedback.play(audio::Cue::back);
        }
    }

    // ---- the screen ----

    void draw_logo(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float power = sign_power(age_, clock_, reduced);
        const float in = tween::stagger(age_, 0, 0.05f, 0.4f);
        list.push_opacity(in);
        const float width =
            neon_text(list, fonts.display, "LUMEN DRIFT", kMargin - 4, kLogoBaseline, kLogoSize,
                      4.0f, gfx::Align::left, power, 3.0f);
        // The tagline and a short rule finish the sign.
        const float tag =
            ui::text(list, fonts.semibold, "MIDNIGHT GRAND PRIX", kMargin, kLogoBaseline + 44, 18,
                     kCyan.with_alpha(0.5f + 0.5f * power), gfx::Align::left, 8.0f);
        neon_line(list, kMargin + tag + 20, kLogoBaseline + 38, kMargin + width - 12,
                  kLogoBaseline + 38, kMagenta, 0.3f + 0.7f * power);
        list.pop_opacity();
    }

    // Who is playing: a small plate in the top-right corner.
    void draw_driver(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 2, 0.06f, 0.5f);
        list.push_opacity(in);
        const float cx = 1824.0f - 30.0f;
        const float cy = 128.0f - (context_.settings.reduced_motion ? 0.0f : 10.0f * (1.0f - in));
        list.glow({cx - 30, cy - 30, 60, 60}, 30, 22, kCyan.with_alpha(0.25f));
        list.circle(cx, cy, 30, kNight.with_alpha(0.85f));
        list.ring(cx, cy, 30, 2.5f, kCyan);
        ui::text(list, fonts.semibold, "N", cx, cy + 9, 26, kWhite, gfx::Align::center);
        ui::text(list, fonts.semibold, "NOVA-7", cx - 50, cy - 2, 26, kWhite, gfx::Align::right,
                 2.0f);
        ui::text(list, fonts.semibold, "LEVEL 42  \xC2\xB7  12,480 CR", cx - 50, cy + 24, 16,
                 kDim.with_alpha(0.85f), gfx::Align::right, 3.0f);
        list.pop_opacity();
    }

    void draw_menu(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = level_.value;
        const float alpha = tween::clamp01(1.0f - 1.7f * t);
        if (alpha <= 0.01f)
            return;
        const float slide = reduced ? 0.0f : -520.0f * t;
        list.push_opacity(alpha);

        // The one tube. It grows a little when pressed and shivers when refused.
        const float lit = tween::stagger(age_, 4, 0.06f, 0.4f);
        const float power =
            lit * (0.85f + 0.15f * (reduced ? 1.0f : ui::breathe(clock_))) + 0.5f * press_.value;
        const float nudge = ui::shake(refuse_.value, clock_, 9.0f, 9.0f);
        const Rect tube = Rect{kMargin + slide + nudge * refuse_dx_,
                               tube_y_.value + std::fabs(nudge) * refuse_dy_, tube_w_.value, kRowH}
                              .inset(-5.0f * press_.value);
        neon_bloom(list, tube, kTubeRadius, kCyan, kMagenta, power);
        list.rounded_rect(tube, kTubeRadius, kNight.with_alpha(0.72f * lit));
        neon_edge(list, tube, kTubeRadius, kCyan, power);

        for (int i = 0; i < kItems; ++i)
        {
            const float in = tween::stagger(age_, 2 + i, 0.055f, 0.45f);
            const float f = item_focus_[i].value;
            const float x =
                kMargin + kLabelPad * f + slide - (reduced ? 0.0f : 48.0f * (1.0f - in));
            const float baseline = row_y(i) + kRowH * 0.5f + 0.35f * kLabelSize;
            list.push_opacity(in);
            if (f > 0.02f)
            {
                // The lit label throws its own light inside the tube.
                const Rect haze{x + 10, baseline - 26, label_w_[i] - 20, 16};
                list.glow(haze, 8, 24, kCyan.with_alpha(0.3f * f));
            }
            ui::text(list, fonts.display, kMenu[i].label, x, baseline, kLabelSize,
                     gfx::mix(kDim.with_alpha(0.62f), kWhite, f), gfx::Align::left, kLabelTrack);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    // Entrance progress of the nth part of a preview, and the progress of a
    // number counting up. `age` is the time since the preview appeared; the
    // preview that is fading out is drawn with a large age, fully assembled.
    float part(float age, int index) const
    {
        if (context_.settings.reduced_motion)
            return 1.0f;
        return tween::stagger(age, index, 0.07f, 0.45f);
    }
    float count(float age, float delay = 0.15f) const
    {
        if (context_.settings.reduced_motion)
            return 1.0f;
        return tween::cubic_out((age - delay) / 0.9f);
    }

    // One "LABEL ====---- 86" row; the bar fills and the number counts with it.
    void draw_stat(gfx::DrawList &list, float x, float y, const char *label, int value,
                   float appear, float progress, Color color) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[16];
        list.push_opacity(appear);
        ui::text(list, fonts.semibold, label, x, y + 6, 17, kWhite.with_alpha(0.72f),
                 gfx::Align::left, 3.0f);
        const float shown = static_cast<float>(value) * progress;
        neon_bar(list, {x + 188, y - 5, kPanelInner - 188 - 76, 10}, shown / 100.0f, color);
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(shown + 0.5f));
        ui::text(list, fonts.mono, text, x + kPanelInner, y + 8, 26, kWhite, gfx::Align::right);
        list.pop_opacity();
    }

    void draw_panel(gfx::DrawList &list) const
    {
        const bool reduced = context_.settings.reduced_motion;
        const float t = level_.value;
        const float alpha = tween::clamp01(1.0f - 1.7f * t);
        if (alpha <= 0.01f)
            return;
        const float in = tween::stagger(age_, 5, 0.06f, 0.5f);
        list.push_opacity(alpha * in);
        list.push_transform(1.0f, 0, 0, reduced ? 0.0f : 460.0f * t,
                            reduced ? 0.0f : 36.0f * (1.0f - in));

        neon_pane(list, kPanel, kPanelRadius);

        if (swap_.running)
        {
            // The old preview leaves quickly to the left; the new one arrives
            // a beat later from the right and then assembles.
            const float s = swap_.progress();
            const float away = reduced ? 0.0f : -30.0f * tween::cubic_in(s * 2.2f);
            draw_preview(list, previous_, 1.0f - tween::smoothstep(s * 2.2f), away, 10.0f);
            const float arrive = tween::clamp01((s - 0.3f) / 0.7f);
            draw_preview(list, shown_, tween::smoothstep(arrive),
                         reduced ? 0.0f : 40.0f * (1.0f - tween::quint_out(arrive)), preview_age_);
        }
        else
        {
            draw_preview(list, shown_, 1.0f, 0.0f, preview_age_);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_preview(gfx::DrawList &list, int index, float alpha, float slide, float age) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const float x = kPanel.x + kPanelPad + slide;
        const float y = kPanel.y;
        list.push_opacity(alpha);

        const char *title = kMenu[index].title;
        if (title == nullptr)
            title = track_item(track_).title;
        list.push_opacity(part(age, 0));
        ui::text(list, fonts.semibold, kMenu[index].caption, x, y + 62, 18, kCyan, gfx::Align::left,
                 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(title, 44, kPanelInner), x - 2,
                 y + 118, 44, kWhite);
        list.pop_opacity();

        switch (index)
        {
        case kPlay:
            draw_preview_play(list, x, y, age);
            break;
        case kCareer:
            draw_preview_career(list, x, y, age);
            break;
        case kGarage:
            draw_preview_garage(list, x, y, age);
            break;
        case kLeaderboard:
            draw_preview_board(list, x, y, age);
            break;
        case kOptions:
            draw_preview_options(list, x, y, age);
            break;
        default:
            draw_preview_quit(list, x, y, age);
            break;
        }
        list.pop_opacity();
    }

    // PLAY: the track that Cross would start, with its three ratings.
    void draw_preview_play(gfx::DrawList &list, float x, float y, float age) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Track &track = kTrackList[track_];
        const demo::Item &it = track_item(track_);

        const float art_in = part(age, 1);
        const Rect art{x, y + 152 + 16 * (1.0f - art_in), 216, 216};
        list.push_opacity(art_in);
        list.glow(art, 18, 26, it.accent.with_alpha(0.3f));
        list.image(it.cover, art, gfx::kCanvasUv, kWhite, 18);
        list.bordered_rect(art, 18, clear_of(kCyan), 2, kCyan.with_alpha(0.85f));
        list.pop_opacity();

        const float tx = x + 248;
        list.push_opacity(part(age, 2));
        ui::paragraph(list, fonts.regular, track.line, tx, y + 184, 24, kPanelInner - 248, 34,
                      kWhite.with_alpha(0.86f), 3);
        list.pop_opacity();
        const char *labels[3] = {"LENGTH", "LAPS", "BEST"};
        const char *values[3] = {track.length, track.laps, track.best};
        const float columns[3] = {0.0f, 132.0f, 232.0f};
        list.push_opacity(part(age, 3));
        for (int i = 0; i < 3; ++i)
        {
            ui::text(list, fonts.semibold, labels[i], tx + columns[i], y + 318, 15,
                     kWhite.with_alpha(0.55f), gfx::Align::left, 3.0f);
            ui::text(list, fonts.mono, values[i], tx + columns[i], y + 354, 26,
                     i == 2 ? kSun : kWhite);
        }
        list.pop_opacity();

        for (int i = 0; i < 3; ++i)
            draw_stat(list, x, y + 432 + static_cast<float>(i) * 50, kStatNames[i], track.stats[i],
                      part(age, 4 + i), count(age, 0.25f + 0.08f * static_cast<float>(i)),
                      i == 2 ? kMagenta : kCyan);
    }

    // CAREER: a ring that sweeps to the season's progress while the number
    // in its middle counts up to it.
    void draw_preview_career(gfx::DrawList &list, float x, float y, float age) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[24];
        constexpr float kDone = 0.64f;
        const float progress = count(age);
        const float cx = x + 150;
        const float cy = y + 356;
        list.push_opacity(part(age, 1));
        list.arc(cx, cy, 136, 14, 0.0f, 2.0f * kPi, kWhite.with_alpha(0.10f));
        neon_arc(list, cx, cy, 136, 14, 0.0f, 2.0f * kPi * kDone * progress, kCyan, 1.0f);
        list.arc(cx, cy, 131, 3, 0.0f, 2.0f * kPi * kDone * progress, kWhite.with_alpha(0.7f));
        std::snprintf(text, sizeof(text), "%d%%",
                      static_cast<int>(kDone * 100.0f * progress + 0.5f));
        ui::text(list, fonts.mono, text, cx, cy + 14, 60, kWhite, gfx::Align::center);
        ui::text(list, fonts.semibold, "COMPLETE", cx + 2, cy + 50, 15, kWhite.with_alpha(0.6f),
                 gfx::Align::center, 4.0f);
        list.pop_opacity();

        struct Row
        {
            const char *label;
            int value;
            const char *of; // " / 22", or nullptr
        };
        const Row rows[3] = {
            {"EVENTS WON", 14, " / 22"}, {"PODIUMS", 19, nullptr}, {"STARS", 31, " / 66"}};
        const float rx = x + 360;
        for (int i = 0; i < 3; ++i)
        {
            const float in = part(age, 2 + i);
            const float ry = y + 232 + static_cast<float>(i) * 108 + 14 * (1.0f - in);
            const float n = count(age, 0.2f + 0.1f * static_cast<float>(i));
            list.push_opacity(in);
            ui::text(list, fonts.semibold, rows[i].label, rx, ry, 16, kWhite.with_alpha(0.6f),
                     gfx::Align::left, 3.0f);
            std::snprintf(text, sizeof(text), "%d",
                          static_cast<int>(static_cast<float>(rows[i].value) * n + 0.5f));
            const float w =
                ui::text(list, fonts.mono, text, rx, ry + 50, 44, i == 0 ? kSun : kWhite);
            if (rows[i].of != nullptr)
                ui::text(list, fonts.mono, rows[i].of, rx + w, ry + 50, 26,
                         kWhite.with_alpha(0.55f));
            list.pop_opacity();
        }
    }

    // GARAGE: the car is traced tube by tube, then its three ratings fill.
    void draw_preview_garage(gfx::DrawList &list, float x, float y, float age) const
    {
        const bool reduced = context_.settings.reduced_motion;
        const float ox = x + 12;
        const float oy = y + 176;
        // The road under it.
        list.push_opacity(part(age, 1));
        neon_line(list, ox - 4, oy + 170, ox + 604, oy + 170, kMagenta, 0.8f);
        list.pop_opacity();
        for (int i = 0; i < kCarSegments; ++i)
        {
            const Segment &s = kCar[i];
            const float drawn = reduced ? 1.0f : tween::stagger(age, i, 0.035f, 0.3f);
            if (drawn <= 0.0f)
                continue;
            // Each tube grows from its first end to its second.
            neon_line(list, ox + s.x1, oy + s.y1, ox + tween::lerp(s.x1, s.x2, drawn),
                      oy + tween::lerp(s.y1, s.y2, drawn), s.trim ? kMagenta : kCyan, drawn);
        }
        const float wheels = reduced ? 1.0f : tween::stagger(age, 6, 0.035f, 0.5f);
        list.push_opacity(wheels);
        for (float wheel : {kWheelRear, kWheelFront})
        {
            // Arch over the wheel, from nine o'clock across the top.
            neon_arc(list, ox + wheel, oy + kWheelY, 42, 3, -0.5f * kPi, kPi * wheels, kCyan, 1.0f);
            list.ring(ox + wheel, oy + kWheelY, 31, 9, kCyan.with_alpha(0.12f));
            list.ring(ox + wheel, oy + kWheelY, 28, 3, kWhite.with_alpha(0.9f));
            list.ring(ox + wheel, oy + kWheelY, 12, 2.5f, kMagenta);
        }
        // Headlight.
        list.glow({ox + 574, oy + 106, 16, 8}, 4, 26, kSun.with_alpha(0.5f));
        list.rounded_rect({ox + 572, oy + 106, 16, 7}, 3.5f, kSun);
        list.pop_opacity();

        const char *names[3] = {"SPEED", "GRIP", "BOOST"};
        const int values[3] = {86, 72, 94};
        for (int i = 0; i < 3; ++i)
            draw_stat(list, x, y + 432 + static_cast<float>(i) * 50, names[i], values[i],
                      part(age, 4 + i), count(age, 0.3f + 0.08f * static_cast<float>(i)),
                      i == 2 ? kMagenta : kCyan);
    }

    // LEADERBOARD: six rows; the times are monospaced so the columns hold.
    void draw_preview_board(gfx::DrawList &list, float x, float y, float age) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        char text[8];
        for (int i = 0; i < kBoardRows; ++i)
        {
            const Entry &entry = kBoard[i];
            const float in = part(age, 1 + i);
            const Rect row{x + (reduced ? 0.0f : 28.0f * (1.0f - in)),
                           y + 150 + static_cast<float>(i) * 68, kPanelInner, 58};
            list.push_opacity(in);
            if (entry.you)
            {
                list.glow(row, 14, 16, kCyan.with_alpha(0.22f));
                list.rounded_rect(row, 14, Color::rgb(0x0f2a4a, 0.9f));
                list.bordered_rect(row, 14, clear_of(kCyan), 2, kCyan);
            }
            else
            {
                list.rounded_rect(row, 14, kWhite.with_alpha(0.055f));
            }
            const float baseline = row.cy() + 9;
            std::snprintf(text, sizeof(text), "%02d", i + 1);
            ui::text(list, fonts.mono, text, row.x + 22, baseline, 24,
                     i == 0 ? kSun : kWhite.with_alpha(0.6f));
            ui::text(list, fonts.semibold, entry.name, row.x + 84, baseline, 26,
                     kWhite.with_alpha(entry.you ? 1.0f : 0.88f), gfx::Align::left, 2.0f);
            if (entry.you)
                ui::text(list, fonts.semibold, "YOU", row.x + 226, baseline - 1, 15, kCyan,
                         gfx::Align::left, 3.0f);
            if (i == 0)
                list.star(row.x + 236, row.cy(), 10, kSun);
            ui::text(list, fonts.mono, entry.time, row.x + row.w - 22, baseline, 26,
                     entry.you ? kWhite : kWhite.with_alpha(0.88f), gfx::Align::right);
            list.pop_opacity();
        }
    }

    // OPTIONS: the app's real settings, shown as the switches they would be.
    void draw_preview_options(gfx::DrawList &list, float x, float y, float age) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Settings &settings = context_.settings;
        char text[8];
        const char *labels[4] = {"Reduce motion", "Controller rumble", "Light bar", "Music"};
        const bool states[3] = {settings.reduced_motion, settings.haptics, settings.light_bar};
        for (int i = 0; i < 4; ++i)
        {
            const float in = part(age, 1 + i);
            const float top = y + 156 + static_cast<float>(i) * 86 + 14 * (1.0f - in);
            const float cy = top + 36;
            list.push_opacity(in);
            if (i > 0)
                list.rounded_rect({x, top - 7, kPanelInner, 1.5f}, 0, kWhite.with_alpha(0.1f));
            ui::text(list, fonts.regular, labels[i], x, cy + 10, 28, kWhite.with_alpha(0.92f));
            if (i < 3)
            {
                // The thumb travels to its side with a small overshoot as the
                // row arrives, so "on" is seen happening.
                const bool on = states[i];
                const float travel =
                    on ? tween::back_out(count(age, 0.2f + 0.1f * static_cast<float>(i)) * 1.6f)
                       : 0.0f;
                const Rect pill{x + kPanelInner - 84, cy - 20, 84, 40};
                if (on)
                {
                    list.glow(pill, 20, 14, kCyan.with_alpha(0.3f * travel));
                    list.bordered_rect(pill, 20, kCyan.with_alpha(0.22f), 2.5f, kCyan);
                }
                else
                {
                    list.bordered_rect(pill, 20, kWhite.with_alpha(0.04f), 2.5f,
                                       kWhite.with_alpha(0.32f));
                }
                list.circle(pill.x + 20 + 44 * travel, cy, 13,
                            on ? kWhite : kWhite.with_alpha(0.5f));
                ui::text(list, fonts.semibold, on ? "ON" : "OFF", pill.x - 18, cy + 6, 16,
                         on ? kCyan : kWhite.with_alpha(0.5f), gfx::Align::right, 3.0f);
            }
            else
            {
                // Ten lamps for the volume, lit one after the other.
                const int volume = std::clamp(settings.music_volume, 0, 10);
                const float lit = static_cast<float>(volume) * count(age, 0.45f);
                const float right = x + kPanelInner - 52;
                for (int k = 0; k < 10; ++k)
                {
                    const Rect lamp{right - static_cast<float>(10 - k) * 24, cy - 12, 16, 24};
                    const float glow = tween::clamp01(lit - static_cast<float>(k));
                    list.rounded_rect(lamp, 4, kWhite.with_alpha(0.12f));
                    if (glow > 0.0f)
                    {
                        list.glow(lamp, 4, 10, kMagenta.with_alpha(0.3f * glow));
                        list.rounded_rect(lamp, 4, kMagenta.with_alpha(glow));
                    }
                }
                std::snprintf(text, sizeof(text), "%d", static_cast<int>(lit + 0.5f));
                ui::text(list, fonts.mono, text, x + kPanelInner, cy + 9, 26, kWhite,
                         gfx::Align::right);
            }
            list.pop_opacity();
        }
        list.push_opacity(part(age, 5));
        ui::text(list, fonts.regular, "Live values from this app's settings.", x, y + 548, 20,
                 kWhite.with_alpha(0.55f));
        list.pop_opacity();
    }

    // QUIT: a short farewell. The session clock is the app's real uptime.
    void draw_preview_quit(gfx::DrawList &list, float x, float y, float age) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        char text[24];
        const float icon_in = part(age, 1);
        const float cx = x + 92;
        const float cy = y + 270;
        const float power = icon_in * (reduced ? 1.0f : 0.8f + 0.2f * ui::breathe(clock_, 2.0f));
        // A power symbol: a ring open at the top and a bar through the gap.
        neon_arc(list, cx, cy, 72, 8, 0.6f, (2.0f * kPi - 1.2f) * icon_in, kMagenta, power);
        neon_line(list, cx, cy - 84, cx, cy - 84 + 60 * icon_in, kMagenta, power);

        list.push_opacity(part(age, 2));
        ui::paragraph(list, fonts.regular,
                      "Your career is saved at the last checkpoint. The lights stay on until "
                      "you come back.",
                      x + 216, y + 228, 26, kPanelInner - 216, 38, kWhite.with_alpha(0.88f), 4);
        list.pop_opacity();

        const int seconds = static_cast<int>(std::min(context_.telemetry.uptime, 359999.0));
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", seconds / 3600, seconds / 60 % 60,
                      seconds % 60);
        const char *labels[2] = {"THIS SESSION", "LAST CHECKPOINT"};
        const char *values[2] = {text, track_item(track_).title};
        for (int i = 0; i < 2; ++i)
        {
            const float in = part(age, 3 + i);
            const float top = y + 412 + static_cast<float>(i) * 76 + 12 * (1.0f - in);
            list.push_opacity(in);
            list.rounded_rect({x, top, kPanelInner, 1.5f}, 0, kWhite.with_alpha(0.1f));
            ui::text(list, fonts.semibold, labels[i], x, top + 46, 16, kWhite.with_alpha(0.6f),
                     gfx::Align::left, 3.0f);
            ui::text(list, i == 0 ? fonts.mono : fonts.semibold, values[i], x + kPanelInner,
                     top + 48, 26, i == 0 ? kSun : kWhite, gfx::Align::right);
            list.pop_opacity();
        }
    }

    // ---- the second level: five track cards ----

    void draw_track_info(gfx::DrawList &list, int index, float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Track &track = kTrackList[index];
        const float cx = 960.0f + slide;
        list.push_opacity(alpha);
        ui::text(list, fonts.display, track_item(index).title, cx, kInfoPane.y + 74, 48, kWhite,
                 gfx::Align::center);
        ui::text(list, fonts.regular, fonts.regular.font->fit(track.line, 26, kInfoPane.w - 96), cx,
                 kInfoPane.y + 118, 26, kWhite.with_alpha(0.82f), gfx::Align::center);
        const char *labels[3] = {"LENGTH", "LAPS", "BEST LAP"};
        const char *values[3] = {track.length, track.laps, track.best};
        for (int i = 0; i < 3; ++i)
        {
            const float column = cx + static_cast<float>(i - 1) * 230;
            ui::text(list, fonts.semibold, labels[i], column, kInfoPane.y + 170, 15,
                     kWhite.with_alpha(0.55f), gfx::Align::center, 3.0f);
            ui::text(list, fonts.mono, values[i], column, kInfoPane.y + 206, 28,
                     i == 2 ? kSun : kWhite, gfx::Align::center);
        }
        list.pop_opacity();
    }

    void draw_tracks(gfx::DrawList &list) const
    {
        const float t = level_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        char text[16];
        // The cards fan in from the right: the further right, the later.
        const float slide = reduced ? 0.0f : (1.0f - t) * 300.0f;
        const auto fan = [slide](float index) { return slide * (1.0f + 0.3f * index); };
        list.push_opacity(tween::smoothstep(t));

        ui::text(list, fonts.semibold, "SELECT TRACK", kTrackX + fan(0.0f), 282, 20, kCyan,
                 gfx::Align::left, 6.0f);
        std::snprintf(text, sizeof(text), "%d / %d", track_ + 1, kTracks);
        ui::text(list, fonts.mono, text, kTrackX + kTracks * kCardPitch - kCardGap + fan(4.0f), 282,
                 22, kWhite.with_alpha(0.7f), gfx::Align::right);

        // The frame is one tube that glides from card to card. Its bloom goes
        // under the covers, its glass over them.
        const float position = (frame_x_.value - card_cx(0)) / kCardPitch;
        const float size = kCard * (1.0f + kCardGrow) + 20.0f + 12.0f * press_.value;
        const float nudge = ui::shake(refuse_.value, clock_, 14.0f, 9.0f) * refuse_dx_;
        const Rect frame{frame_x_.value + fan(position) + nudge - size * 0.5f,
                         kTrackY + kCard * 0.5f - kCardLift - size * 0.5f, size, size};
        const float power =
            0.85f + 0.15f * (reduced ? 1.0f : ui::breathe(clock_)) + 0.6f * press_.value;
        neon_bloom(list, frame, 32, kCyan, kMagenta, power);

        // Resting cards first, then the focused one on top of its neighbours.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < kTracks; ++i)
            {
                if ((i == track_) != (pass == 1))
                    continue;
                const demo::Item &it = track_item(i);
                const float f = card_focus_[i].value;
                const float card = kCard * (1.0f + kCardGrow * f);
                const Rect r{card_cx(i) + fan(static_cast<float>(i)) +
                                 (i == track_ ? nudge : 0.0f) - card * 0.5f,
                             kTrackY + kCard * 0.5f - kCardLift * f - card * 0.5f, card, card};
                if (f > 0.02f)
                    list.shadow({r.x, r.y + 20, r.w, r.h}, 24, 36, Color::rgb(0x000000, 0.55f * f));
                // Unlit cards sit back: darker, with a faint magenta rim.
                const float light = 0.5f + 0.5f * f;
                list.image(it.cover, r, gfx::kCanvasUv, Color{light, light, light, 1.0f}, 24);
                list.bordered_rect(r, 24, clear_of(kMagenta), 2,
                                   kMagenta.with_alpha(0.45f * (1.0f - f)));
            }
        }
        neon_edge(list, frame, 32, kCyan, power);

        // The facts of the focused track cross-fade on a pane of their own,
        // sliding the way the frame moved.
        const float rise = reduced ? 0.0f : (1.0f - t) * 60.0f;
        list.push_transform(1.0f, 0, 0, 0, rise);
        neon_pane(list, kInfoPane, kPanelRadius);
        if (track_swap_.running)
        {
            const float s = track_swap_.progress();
            const float way = reduced ? 0.0f : track_direction_;
            draw_track_info(list, track_previous_, 1.0f - tween::smoothstep(s * 2.2f),
                            -36.0f * way * tween::cubic_in(s * 2.2f));
            const float arrive = tween::clamp01((s - 0.25f) / 0.75f);
            draw_track_info(list, track_shown_, tween::smoothstep(arrive),
                            44.0f * way * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_track_info(list, track_shown_, 1.0f, 0.0f);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // The launch: a white flash, the floor rushing, streaks from the centre
    // and "GO" punching in, then flying past the camera.
    void draw_go(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = go_.progress();
        const float envelope =
            tween::clamp01(t / 0.06f) * (1.0f - tween::smoothstep((t - 0.8f) / 0.2f));
        const Rect screen{0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight};
        list.rounded_rect(screen, 0, kNight.with_alpha(0.88f * envelope));

        if (!reduced)
        {
            // Streaks on golden-angle bearings, so they never line up.
            for (int i = 0; i < 30; ++i)
            {
                const float bearing = static_cast<float>(i) * 2.39996f;
                float travel = t * 1.7f + static_cast<float>(i) * 0.137f;
                travel -= std::floor(travel);
                const float inner = 240.0f + 900.0f * travel;
                const float outer = inner + 50.0f + 260.0f * travel;
                const float sx = std::sin(bearing);
                const float sy = -std::cos(bearing) * 0.6f;
                list.line(960 + sx * inner, 540 + sy * inner, 960 + sx * outer, 540 + sy * outer,
                          3.0f,
                          (i % 2 == 0 ? kCyan : kMagenta).with_alpha(0.7f * envelope * travel));
            }
        }

        const float zoom = reduced
                               ? 1.0f
                               : (0.5f + 0.5f * tween::back_out(t / 0.24f)) * (1.0f + 0.08f * t) +
                                     1.4f * tween::cubic_in((t - 0.78f) / 0.22f);
        list.push_opacity(envelope);
        list.push_transform(zoom, 960, 520, 0, 0);
        neon_text(list, fonts.display, "GO", 960, 520 + 0.36f * 400, 400, 12.0f, gfx::Align::center,
                  1.0f, 7.0f);
        list.pop_transform();
        const Track &track = kTrackList[track_];
        const std::string line =
            ui::upper(track_item(track_).title) + "  \xC2\xB7  " + track.laps + " LAPS";
        ui::text(list, fonts.semibold, line, 960, 800, 26, kWhite, gfx::Align::center, 8.0f);
        list.pop_opacity();

        const float flash = reduced ? 0.25f * (1.0f - tween::smoothstep(t * 4.0f))
                                    : 0.9f * (1.0f - tween::cubic_out(t * 3.0f));
        if (flash > 0.004f)
            list.rounded_rect(screen, 0, kWhite.with_alpha(flash));
    }

    // "Quit?" on frosted glass. The highlight is the same tube as the menu's;
    // it turns from cyan to magenta as it reaches the answer that ends things.
    void draw_dialog(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = dialog_.value;
        const Rect panel{580, 366 + (reduced ? 0.0f : 34.0f * (1.0f - t)), 760, 348};
        constexpr float kRadius = 36.0f;
        constexpr float kButtonW = 304.0f;
        constexpr float kButtonGap = 32.0f;
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.glow(panel, kRadius, 50, kMagenta.with_alpha(0.14f));
        list.shadow({panel.x, panel.y + 22, panel.w, panel.h}, kRadius, 56,
                    Color::rgb(0x000000, 0.55f));
        list.glass(glass, panel, kRadius, kWhite);
        list.rounded_rect(panel, kRadius, Color::rgb(0x150835, 0.66f));
        list.bordered_rect(panel, kRadius, clear_of(kWhite), 1.5f, kWhite.with_alpha(0.24f));

        const float x = panel.x + 60;
        ui::text(list, fonts.semibold, "QUIT", x, panel.y + 66, 18, kMagenta, gfx::Align::left,
                 4.0f);
        ui::text(list, fonts.display, "Leave the grid?", x - 2, panel.y + 124, 48, kWhite);
        ui::text(list, fonts.regular, "You can pick up again from the last checkpoint.", x,
                 panel.y + 174, 26, kWhite.with_alpha(0.82f));

        const float by = panel.y + 228;
        const float nudge = ui::shake(refuse_.value, clock_, 10.0f, 9.0f) * refuse_dx_;
        const Color core = gfx::mix(kCyan, kMagenta, tween::clamp01(choice_position_.value));
        const Rect tube{x + choice_position_.value * (kButtonW + kButtonGap) + nudge, by, kButtonW,
                        68};
        neon_bloom(list, tube, 20, core, kMagenta, 0.9f);
        list.rounded_rect(tube, 20, kNight.with_alpha(0.7f));
        neon_edge(list, tube, 20, core, 0.95f);
        const char *answers[2] = {"No, keep racing", "Yes, quit"};
        for (int i = 0; i < 2; ++i)
        {
            const Rect button{x + static_cast<float>(i) * (kButtonW + kButtonGap), by, kButtonW,
                              68};
            const bool focused = i == choice_;
            if (!focused)
                list.bordered_rect(button, 20, kWhite.with_alpha(0.05f), 1.5f,
                                   kWhite.with_alpha(0.2f));
            ui::text(list, fonts.semibold, answers[i], button.cx(), button.cy() + 9, 26,
                     kWhite.with_alpha(focused ? 1.0f : 0.7f), gfx::Align::center);
        }
        list.pop_opacity();
    }

    void draw_hints(app::Frame &frame) const
    {
        const ui::Fonts &fonts = context_.fonts;
        if (go_.running)
            return;
        if (dialog_open_ || dialog_.value > 0.5f)
        {
            frame.overlay.push_opacity(dialog_.value);
            const ui::Hint hints[] = {{ui::Button::cross, "Choose"},
                                      {ui::Button::circle, "Cancel"}};
            hint_plate(frame.overlay, fonts, hints, 2);
            frame.overlay.pop_opacity();
            return;
        }
        // The two levels cross-fade their hints with the level itself.
        gfx::DrawList &list = frame.scene;
        const float t = level_.value;
        const float in = tween::stagger(age_, 9, 0.06f, 0.5f) * (1.0f - dialog_.value);
        const float menu = tween::clamp01(1.0f - 2.0f * t);
        const float tracks = tween::clamp01(2.0f * t - 1.0f);
        if (menu > 0.01f)
        {
            list.push_opacity(in * menu);
            const ui::Hint hints[] = {{ui::Button::dpad, "Navigate"},
                                      {ui::Button::cross, "Select"}};
            hint_plate(list, fonts, hints, 2);
            list.pop_opacity();
        }
        if (tracks > 0.01f)
        {
            list.push_opacity(in * tracks);
            const ui::Hint hints[] = {{ui::Button::dpad, "Track"},
                                      {ui::Button::cross, "Race"},
                                      {ui::Button::circle, "Back"}};
            hint_plate(list, fonts, hints, 3);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    float label_w_[kItems] = {}; // menu label widths, measured once
    float age_ = 0.0f;           // seconds since enter(): the entrance and the sign striking
    float clock_ = 0.0f;         // free-running, for idle light
    float road_ = 0.0f;          // the backdrop's time: runs faster during "GO"

    // Main menu.
    int focus_ = 0;
    tween::Bounce tube_y_; // the focus tube: overshoots a little on the way
    tween::Spring tube_w_;
    tween::Spring item_focus_[kItems]; // 0..1 per entry: label shift and brightness
    ui::Pulse press_;
    ui::Pulse refuse_;
    float refuse_dx_ = 0.0f;
    float refuse_dy_ = 0.0f;

    // Preview panel.
    int shown_ = 0;    // the preview on show
    int previous_ = 0; // ... and the one fading out
    tween::Timer swap_;
    float preview_age_ = 0.0f; // seconds since the preview on show appeared

    // Track selector.
    bool in_tracks_ = false;
    tween::Spring level_; // 0 main menu, 1 track selector
    int track_ = 0;
    tween::Bounce frame_x_;
    tween::Spring card_focus_[kTracks];
    int track_shown_ = 0;
    int track_previous_ = 0;
    tween::Timer track_swap_;
    float track_direction_ = 1.0f;
    tween::Timer go_; // the launch moment

    // Quit dialog.
    bool dialog_open_ = false;
    tween::Spring dialog_;
    int choice_ = 0; // 0 no, 1 yes
    tween::Spring choice_position_;
};

} // namespace

std::unique_ptr<app::Concept> make_neon(app::Context &context)
{
    return std::make_unique<Neon>(context);
}

} // namespace hui::concepts
