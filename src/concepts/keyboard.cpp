// ps5-homebrew-ui - Design "First Run": a setup wizard with an on-screen keyboard.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Three steps a new player walks through once: pick an avatar, type a name,
// confirm. Typing with a controller is the hardest thing to make pleasant, so
// the keyboard is the centre of the design. What makes it feel finished:
//
//   - one highlight glides between the keys and morphs between their sizes;
//     the key caps take their colour from how much of it covers them, so no
//     letter is ever dark on dark while the highlight is still on its way;
//   - rows wrap around: the highlight leaves through one edge of the board
//     and comes in through the other, and a wide key remembers the column the
//     player came from, so up and down are always each other's undo;
//   - the frequent actions never need the focus: Square deletes (and repeats
//     while held), Triangle is space, L2 is shift, R2 is done; each special
//     key carries its glyph and dips when its shortcut is used;
//   - shift is armed by itself for the first letter, releases after one
//     letter and locks on a second press, like the keyboards players know;
//   - every character slides into the field, every deleted one sinks out, the
//     caret glides, and refusals (too long, empty) shake the field softly and
//     say why instead of doing nothing;
//   - steps change with a slide and a cross-fade in the direction of travel,
//     under a step indicator whose line springs forward and whose finished
//     dots draw their check mark;
//   - the last step ends in confetti behind a frosted welcome card, then the
//     wizard loops with everything the player chose still in place.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/confetti.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

// Two accents with one job each: cool is "where you are" (every focus), warm
// is "where you are going" (progress, the caret, the buttons that continue).
const Color kInk = Color::rgb(0xf3f5ff);
const Color kNight = Color::rgb(0x10143a); // text on lit tiles
const Color kPanel = Color::rgb(0x141a4c);
const Color kWarm = Color::rgb(0xffb86b);
const Color kCool = Color::rgb(0x7cc8ff);
const Color kWarn = Color::rgb(0xff7f7f);
const Color kWhite = Color::rgb(0xffffff);
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kShade = Color::rgb(0x03051a);

constexpr float kPi = 3.14159265f;
constexpr float kCentre = 960.0f;
constexpr float kPanelRadius = 28.0f;

// Step indicator.
constexpr int kSteps = 3;
constexpr float kStepY = 98.0f;
constexpr float kStepPitch = 230.0f;
constexpr float kStepDot = 21.0f;
constexpr const char *kStepNames[kSteps] = {"AVATAR", "NAME", "READY"};
constexpr float kStepSlide = 170.0f;  // how far a step travels while it changes
constexpr float kStepSeconds = 0.45f; // a screen change

// Step 1: the avatar row.
constexpr int kAvatars = 8;
constexpr float kAvatarY = 550.0f;
constexpr float kAvatarPitch = 190.0f;
constexpr float kAvatarRadius = 70.0f;
constexpr float kAvatarRest = 0.9f;  // scale of the ones not chosen
constexpr float kAvatarGrow = 1.32f; // scale of the focused one

// Step 2: the field and the keyboard.
constexpr int kMaxName = 14;
constexpr int kColumns = 10;
constexpr int kCharRows = 4;
constexpr int kRows = 5; // four rows of characters and the row of wide keys
constexpr int kKeys = kColumns * kCharRows + 4;
constexpr int kShiftKey = kColumns * kCharRows;
constexpr int kSpaceKey = kShiftKey + 1;
constexpr int kEraseKey = kShiftKey + 2;
constexpr int kDoneKey = kShiftKey + 3;
constexpr int kFirstKey = 24; // "g": the middle of the board, the shortest way anywhere
constexpr float kKeyW = 108.0f;
constexpr float kKeyH = 82.0f;
constexpr float kKeyGap = 10.0f;
constexpr float kKeyRadius = 18.0f;
constexpr float kBoardW = kColumns * kKeyW + (kColumns - 1) * kKeyGap;
constexpr float kBoardH = kRows * kKeyH + (kRows - 1) * kKeyGap;
constexpr float kBoardX = kCentre - kBoardW * 0.5f;
constexpr float kBoardY = 446.0f;
constexpr float kBoardPad = 20.0f;
constexpr Rect kBoard{kBoardX - kBoardPad, kBoardY - kBoardPad, kBoardW + 2.0f * kBoardPad,
                      kBoardH + 2.0f * kBoardPad};
constexpr Rect kField{kBoard.x, 258.0f, kBoard.w, 100.0f};
constexpr float kNameX = 116.0f;     // text start inside the field
constexpr float kNameWidth = 900.0f; // room for the text
constexpr float kNameSize = 54.0f;
constexpr float kPressDip = 0.06f;       // a pressed key shrinks to 0.94
constexpr float kRepeatDelay = 0.42f;    // held Square: wait, then ...
constexpr float kRepeatInterval = 0.09f; // ... one character per interval
constexpr float kMessageSeconds = 2.0f;

// Step 3: the summary card.
constexpr int kPrefs = 2;
constexpr int kStartItem = kPrefs; // focus order: the switches, then Start
constexpr Rect kCard{500.0f, 340.0f, 920.0f, 396.0f};
constexpr Rect kStart{kCentre - 190.0f, 792.0f, 380.0f, 84.0f};
constexpr const char *kPrefNames[kPrefs] = {"Show friends when I am online",
                                            "Share what I am playing"};
constexpr float kWelcomeSeconds = 2.5f;

constexpr const char *kTechniques[] = {
    "On-screen keyboard: one ui::SpringRect highlight that morphs between key sizes",
    "Key caps coloured by how much of the gliding highlight covers them",
    "Row wrap-around: out through one edge, in through the other; wide keys keep the column",
    "Shortcuts on Square, Triangle, L2 and R2, with held-Square repeat from is_held and a timer",
    "Characters slide in and sink out; the caret glides; refusals shake the field and say why",
    "Steps slide and cross-fade in the direction of travel under a springing step indicator",
    "Confetti and a frosted welcome card, then the wizard loops with its state kept",
};

constexpr app::TourStep kTour[] = {
    // Step 1: choose the fourth avatar.
    {0.5f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.8f, action_bit(Action::confirm)},
    // Step 2: R2 with nothing typed is refused; then type "Rio".
    {0.9f, action_bit(Action::jump_next)},
    {0.45f, 0, Direction::up, "empty"},
    {0.2f, 0, Direction::left},
    {0.3f, action_bit(Action::confirm)},
    {0.25f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {0.25f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {0.8f, action_bit(Action::jump_prev), Direction::none, "typing"},
    {0.7f, action_bit(Action::jump_next), Direction::none, "shift"},
    // Step 3: start, celebrate, loop.
    {1.0f, action_bit(Action::confirm), Direction::none, "summary"},
    {1.3f, action_bit(Action::confirm), Direction::none, "welcome"},
    {0.8f, 0},
};

// ---- avatars ---------------------------------------------------------------

struct Avatar
{
    const char *name;
    std::uint32_t light; // top of the disc
    std::uint32_t dark;  // bottom of the disc
};

constexpr Avatar kAvatarTable[kAvatars] = {
    {"Sunny", 0xffd166, 0xff8a3d},  {"Comet", 0x8fd0ff, 0x3f66f0},  {"Moss", 0x9be88f, 0x1fa77a},
    {"Summit", 0xcaa4ff, 0x6a4be0}, {"Ripple", 0x7df2e2, 0x1691b4}, {"Bolt", 0xffec7a, 0xf0a000},
    {"Gizmo", 0xc3cfee, 0x5d6da6},  {"Rosy", 0xffa9cb, 0xe6457e},
};

// An avatar is a gradient disc and a few shapes sized by its radius, so the
// same function draws the big one in the row and the small one in the field.
void draw_avatar(gfx::DrawList &list, int index, float cx, float cy, float r)
{
    const Avatar &avatar = kAvatarTable[index];
    const Color ink = Color::rgb(0x1b1640, 0.88f);
    list.gradient_rect({cx - r, cy - r, 2.0f * r, 2.0f * r}, r, Color::rgb(avatar.light),
                       Color::rgb(avatar.dark));
    const auto smile = [&]()
    {
        list.arc(cx, cy + 0.04f * r, 0.5f * r, 0.1f * r, 110.0f * kPi / 180.0f,
                 140.0f * kPi / 180.0f, ink);
    };
    switch (index)
    {
    case 0: // a smiling face
        list.circle(cx - 0.3f * r, cy - 0.14f * r, 0.095f * r, ink);
        list.circle(cx + 0.3f * r, cy - 0.14f * r, 0.095f * r, ink);
        smile();
        break;
    case 1: // a star with a tail
        list.line(cx - 0.32f * r, cy - 0.1f * r, cx - 0.62f * r, cy - 0.34f * r, 0.07f * r,
                  kWhite.with_alpha(0.55f));
        list.line(cx - 0.2f * r, cy - 0.24f * r, cx - 0.5f * r, cy - 0.54f * r, 0.07f * r,
                  kWhite.with_alpha(0.7f));
        list.line(cx - 0.06f * r, cy - 0.34f * r, cx - 0.3f * r, cy - 0.64f * r, 0.07f * r,
                  kWhite.with_alpha(0.55f));
        list.star(cx + 0.1f * r, cy + 0.1f * r, 0.46f * r, kWhite);
        break;
    case 2: // a wink
        list.line(cx - 0.4f * r, cy - 0.14f * r, cx - 0.2f * r, cy - 0.14f * r, 0.09f * r, ink);
        list.circle(cx + 0.3f * r, cy - 0.14f * r, 0.095f * r, ink);
        smile();
        break;
    case 3: // two peaks and a sun
        list.circle(cx + 0.36f * r, cy - 0.38f * r, 0.13f * r, Color::rgb(0xffe9a8));
        list.triangle({cx - 0.62f * r, cy - 0.36f * r, 0.84f * r, 0.78f * r}, kWhite);
        list.triangle({cx - 0.02f * r, cy - 0.04f * r, 0.6f * r, 0.46f * r},
                      kWhite.with_alpha(0.72f));
        break;
    case 4: // rings on water
        list.ring(cx, cy, 0.64f * r, 0.08f * r, kWhite.with_alpha(0.35f));
        list.ring(cx, cy, 0.42f * r, 0.08f * r, kWhite.with_alpha(0.62f));
        list.circle(cx, cy, 0.17f * r, kWhite);
        break;
    case 5: // a lightning bolt from three strokes
        list.line(cx + 0.14f * r, cy - 0.58f * r, cx - 0.24f * r, cy + 0.08f * r, 0.13f * r, ink);
        list.line(cx - 0.24f * r, cy + 0.08f * r, cx + 0.24f * r, cy - 0.08f * r, 0.13f * r, ink);
        list.line(cx + 0.24f * r, cy - 0.08f * r, cx - 0.14f * r, cy + 0.58f * r, 0.13f * r, ink);
        break;
    case 6: // a small robot
        list.line(cx, cy - 0.3f * r, cx, cy - 0.54f * r, 0.06f * r, kWhite);
        list.circle(cx, cy - 0.58f * r, 0.09f * r, kWarm);
        list.rounded_rect({cx - 0.5f * r, cy - 0.3f * r, r, 0.74f * r}, 0.18f * r,
                          kWhite.with_alpha(0.94f));
        list.circle(cx - 0.2f * r, cy - 0.02f * r, 0.1f * r, ink);
        list.circle(cx + 0.2f * r, cy - 0.02f * r, 0.1f * r, ink);
        list.line(cx - 0.16f * r, cy + 0.24f * r, cx + 0.16f * r, cy + 0.24f * r, 0.06f * r, ink);
        break;
    default: // a heart: a square on its corner and two discs on its upper edges
    {
        const float side = 0.52f * r;
        const float lift = side * 0.3536f; // half an edge, seen at 45 degrees
        const float y = cy + 0.1f * r;
        list.rotated_rect({cx - side * 0.5f, y - side * 0.5f, side, side}, 0.0f, kPi * 0.25f,
                          kWhite);
        list.circle(cx - lift, y - lift, side * 0.5f, kWhite);
        list.circle(cx + lift, y - lift, side * 0.5f, kWhite);
        break;
    }
    }
}

float avatar_x(int index)
{
    return kCentre + (static_cast<float>(index) - (kAvatars - 1) * 0.5f) * kAvatarPitch;
}

// ---- keys ------------------------------------------------------------------

enum class KeyKind : std::uint8_t
{
    character,
    shift,
    space,
    erase,
    done,
};

struct Key
{
    KeyKind kind = KeyKind::character;
    int row = 0;
    int column = 0;
    int span = 1; // columns covered
    char lower = 0;
    char upper = 0;
};

constexpr bool is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr std::array<Key, kKeys> make_keys()
{
    constexpr const char *kCharacters[kCharRows] = {"1234567890", "qwertyuiop", "asdfghjkl'",
                                                    "zxcvbnm-_."};
    std::array<Key, kKeys> keys{};
    for (int row = 0; row < kCharRows; ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            const char lower = kCharacters[row][column];
            const char upper = is_letter(lower) ? static_cast<char>(lower - 'a' + 'A') : lower;
            keys[static_cast<std::size_t>(row * kColumns + column)] = {
                KeyKind::character, row, column, 1, lower, upper};
        }
    }
    keys[kShiftKey] = {KeyKind::shift, kCharRows, 0, 2, 0, 0};
    keys[kSpaceKey] = {KeyKind::space, kCharRows, 2, 4, ' ', ' '};
    keys[kEraseKey] = {KeyKind::erase, kCharRows, 6, 2, 0, 0};
    keys[kDoneKey] = {KeyKind::done, kCharRows, 8, 2, 0, 0};
    return keys;
}

constexpr std::array<Key, kKeys> kKeyTable = make_keys();

const Key &key_info(int index)
{
    return kKeyTable[static_cast<std::size_t>(index)];
}

// The key that covers a grid cell.
int key_at(int row, int column)
{
    if (row < kCharRows)
        return row * kColumns + column;
    for (int index = kShiftKey; index < kKeys; ++index)
    {
        const Key &key = key_info(index);
        if (column >= key.column && column < key.column + key.span)
            return index;
    }
    return kSpaceKey;
}

Rect key_rect(int index)
{
    const Key &key = key_info(index);
    return {kBoardX + static_cast<float>(key.column) * (kKeyW + kKeyGap),
            kBoardY + static_cast<float>(key.row) * (kKeyH + kKeyGap),
            static_cast<float>(key.span) * (kKeyW + kKeyGap) - kKeyGap, kKeyH};
}

// How much of `inner` lies under `cover`, 0..1.
float coverage(const Rect &cover, const Rect &inner)
{
    const float w = std::min(cover.x + cover.w, inner.x + inner.w) - std::max(cover.x, inner.x);
    const float h = std::min(cover.y + cover.h, inner.y + inner.h) - std::max(cover.y, inner.y);
    if (w <= 0.0f || h <= 0.0f)
        return 0.0f;
    return tween::clamp01(w * h / (inner.w * inner.h));
}

Rect pref_rect(int index)
{
    return {kCard.x + 32.0f, kCard.y + 248.0f + static_cast<float>(index) * 68.0f, kCard.w - 64.0f,
            60.0f};
}

// A check mark that draws itself: the short stroke, then the long one.
void draw_check(gfx::DrawList &list, float cx, float cy, float size, float progress, float stroke,
                Color color)
{
    if (progress <= 0.01f)
        return;
    const float ax = cx - 0.9f * size, ay = cy + 0.05f * size;
    const float bx = cx - 0.25f * size, by = cy + 0.65f * size;
    const float ex = cx + 0.9f * size, ey = cy - 0.6f * size;
    const float first = tween::clamp01(progress / 0.4f);
    const float second = tween::clamp01((progress - 0.4f) / 0.6f);
    // A stroke of no length is still a dot: fade the first moments in.
    color = color.with_alpha(tween::clamp01(progress * 5.0f));
    list.line(ax, ay, tween::lerp(ax, bx, first), tween::lerp(ay, by, first), stroke, color);
    if (second > 0.0f)
        list.line(bx, by, tween::lerp(bx, ex, second), tween::lerp(by, ey, second), stroke, color);
}

enum class Shift : std::uint8_t
{
    off,
    once, // the next letter only
    lock, // until L2 again
};

// A deleted character on its way out of the field.
struct Ghost
{
    char character = 0;
    float x = 0.0f;    // offset from the start of the text
    float fade = 0.0f; // 1 just deleted .. 0 gone
};

class FirstRun final : public app::Concept
{
  public:
    explicit FirstRun(app::Context &context) : context_(context)
    {
        name_[0] = '\0';
        for (int i = 0; i < kAvatars; ++i)
            avatar_scale_[i].snap(i == avatar_ ? kAvatarGrow : kAvatarRest);
        avatar_ring_.snap(avatar_x(avatar_));
        highlight_.snap(key_rect(key_));
        summary_ring_.snap(kStart);
        for (int i = 0; i < kPrefs; ++i)
            pref_blend_[i].snap(prefs_[i] ? 1.0f : 0.0f);
        placeholder_.snap(1.0f);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "keyboard",
            "First Run",
            "A setup wizard: avatar, a name typed on a controller keyboard, a warm welcome",
            "src/concepts/keyboard.cpp",
            audio::SoundSet::paper,
            kWarm,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        erase_hold_ = 0.0f;
        // Coming back in the middle of the party would be odd: finish it. The
        // step, the avatar and the name stay as the player left them.
        if (celebrating_)
        {
            celebrating_ = false;
            welcome_.snap(0.0f);
            go_to(0, 1.0f);
            transition_.running = false;
        }
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (!context_.settings.reduced_motion)
            drift_ += dt;

        if (celebrating_)
            update_welcome(input, dt, feedback);
        else if (step_ == 0)
            update_avatars(input, feedback);
        else if (step_ == 1)
            update_keyboard(input, dt, feedback);
        else
            update_summary(input, feedback);

        animate(dt);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::waves;
        frame.backdrop.colors[0] = Color::rgb(0x161a52);
        frame.backdrop.colors[1] = Color::rgb(0x0a2c3c);
        frame.backdrop.colors[2] = Color::rgb(0x4a4fd0);
        frame.backdrop.colors[3] = Color::rgb(0x25a79c);
        frame.backdrop.time = drift_;

        gfx::DrawList &list = frame.scene;
        // The welcome card pushes the wizard back: it shrinks a little and dims.
        const float back = welcome_.value;
        list.push_transform(1.0f - 0.035f * back, kCentre, 540, 0, 0);
        draw_indicator(list);
        for (int step = 0; step < kSteps; ++step)
        {
            const View view = view_of(step);
            if (view.alpha <= 0.01f)
                continue;
            // Under the welcome card the step all but leaves, so the glass
            // has calm waves to blur and nothing competes with the card.
            list.push_opacity(view.alpha * (1.0f - 0.85f * back));
            list.push_transform(1.0f, 0, 0, view.slide, 0);
            if (step == 0)
                draw_avatars(list);
            else if (step == 1)
                draw_name(list);
            else
                draw_summary(list);
            list.pop_transform();
            list.pop_opacity();
        }
        list.pop_transform();

        if (back > 0.01f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              kShade.with_alpha(0.45f * back));
            frame.glass = true;
            draw_welcome(frame.overlay, frame.glass_texture);
        }
        // Confetti flies in front of the card and keeps falling after it left.
        confetti_.draw(frame.overlay);
        draw_hints(back > 0.5f ? frame.overlay : frame.scene);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    struct View
    {
        float alpha;
        float slide;
    };

    bool reduced() const
    {
        return context_.settings.reduced_motion;
    }

    std::string_view name() const
    {
        return {name_, static_cast<std::size_t>(length_)};
    }

    // The name is set as large as fits: only a row of the widest letters
    // makes it shrink.
    float name_size() const
    {
        const float width = context_.fonts.semibold.measure(name(), kNameSize);
        return width > kNameWidth ? kNameSize * kNameWidth / width : kNameSize;
    }

    float name_width(int characters) const
    {
        return context_.fonts.semibold.measure({name_, static_cast<std::size_t>(characters)},
                                               name_size());
    }

    // ---- logic -------------------------------------------------------------

    // direction: 1 the new step comes from the right (forward), -1 from the left.
    void go_to(int step, float direction)
    {
        from_step_ = step_;
        step_ = step;
        direction_ = direction;
        transition_.start(reduced() ? 0.2f : kStepSeconds);
        message_time_ = 0.0f;
        erase_hold_ = 0.0f;
        // A new name starts with a capital without the player asking for it.
        if (step_ == 1 && length_ == 0 && shift_ == Shift::off)
            shift_ = Shift::once;
    }

    void refuse(app::Feedback &feedback, float gain = 0.5f)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, gain);
        feedback.rumble(0.25f, 0.05f);
    }

    void update_avatars(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const float way = input.nav == Direction::right ? 1.0f : -1.0f;
            const int next = avatar_ + static_cast<int>(way);
            if (next >= 0 && next < kAvatars)
            {
                avatar_ = next;
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(avatar_x(avatar_)));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback);
                avatar_nudge_.trigger();
                avatar_nudge_way_ = way;
            }
        }
        if (input.is_pressed(Action::back))
        {
            // There is no step before the first: the row leans back and settles.
            refuse(feedback);
            avatar_nudge_.trigger();
            avatar_nudge_way_ = -1.0f;
        }
        else if (input.is_pressed(Action::confirm))
        {
            feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(avatar_x(avatar_)));
            button_press_.trigger();
            go_to(1, 1.0f);
        }
    }

    void update_keyboard(const InputFrame &input, float dt, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            go_to(0, -1.0f);
            return;
        }
        if (input.nav != Direction::none)
            move_key(input.nav, input.nav_repeat, feedback);

        // The shortcuts press their key on screen too, so the player learns
        // which key each button stands for.
        if (input.is_pressed(Action::jump_prev))
        {
            press_key(kShiftKey);
            cycle_shift(feedback);
        }
        if (input.is_pressed(Action::north))
        {
            press_key(kSpaceKey);
            type(' ', kSpaceKey, feedback);
        }
        if (input.is_pressed(Action::west))
        {
            press_key(kEraseKey);
            erase(false, feedback);
            erase_hold_ = 0.0f;
        }
        else if (input.is_held(Action::west))
        {
            // The input model reports navigation repeats only, so a held
            // Square repeats on its own timer: a pause, then a steady run.
            erase_hold_ += dt;
            while (erase_hold_ >= kRepeatDelay)
            {
                erase_hold_ -= kRepeatInterval;
                press_key(kEraseKey);
                erase(true, feedback);
            }
        }
        else
        {
            erase_hold_ = 0.0f;
        }
        if (input.is_pressed(Action::confirm))
            activate(key_, feedback);
        if (step_ == 1 && input.is_pressed(Action::jump_next))
        {
            press_key(kDoneKey);
            finish_name(feedback);
        }
    }

    void move_key(Direction direction, bool repeat, app::Feedback &feedback)
    {
        const Key &from = key_info(key_);
        int next = key_;
        if (direction == Direction::left || direction == Direction::right)
        {
            const bool right = direction == Direction::right;
            int column = right ? from.column + from.span : from.column - 1;
            const bool wrapped = column < 0 || column >= kColumns;
            column = (column + kColumns) % kColumns;
            next = key_at(from.row, column);
            const Key &to = key_info(next);
            // Arriving sideways on a wide key leaves the column in its middle.
            column_ = to.column + (to.span - 1) / 2;
            if (wrapped)
            {
                // The old highlight slides out through the edge it was pushed
                // against while the real one enters from behind the other.
                const float way = right ? 1.0f : -1.0f;
                wrap_from_ = highlight_.value();
                wrap_way_ = way;
                wrap_.trigger();
                Rect start = key_rect(next);
                start.x -= way * (start.w + kBoardPad + kKeyGap);
                highlight_.snap(start);
            }
        }
        else
        {
            const int row = from.row + (direction == Direction::down ? 1 : -1);
            if (row < 0 || row >= kRows)
            {
                // Top and bottom are real edges. A held direction stays quiet.
                if (!repeat)
                {
                    refuse(feedback);
                    bump_.trigger();
                }
                return;
            }
            // column_ is where the player really is: a wide key covers several
            // columns, and going back up returns to the one they left.
            next = key_at(row, column_);
        }
        key_ = next;
        const Key &to = key_info(key_);
        if (to.span == 1)
            column_ = to.column;
        // Rows sound like steps: lower rows, lower pitch.
        feedback.play(audio::Cue::focus, 1.1f - 0.035f * static_cast<float>(to.row),
                      ui::pan_for_x(key_rect(key_).cx()));
    }

    void press_key(int index)
    {
        pressed_key_ = index;
        press_.trigger();
    }

    void activate(int index, app::Feedback &feedback)
    {
        const Key &key = key_info(index);
        press_key(index);
        switch (key.kind)
        {
        case KeyKind::character:
            type(shift_ == Shift::off ? key.lower : key.upper, index, feedback);
            break;
        case KeyKind::space:
            type(' ', index, feedback);
            break;
        case KeyKind::shift:
            cycle_shift(feedback);
            break;
        case KeyKind::erase:
            erase(false, feedback);
            break;
        case KeyKind::done:
            finish_name(feedback);
            break;
        }
    }

    void show_message(const char *text)
    {
        message_ = text;
        message_time_ = kMessageSeconds;
        field_shake_.trigger();
    }

    void type(char character, int key, app::Feedback &feedback)
    {
        if (length_ >= kMaxName)
        {
            // Full: the field shakes and the counter, already at 14 / 14,
            // jumps. That is explanation enough, so no message.
            refuse(feedback);
            field_shake_.trigger();
            counter_pop_.trigger();
            return;
        }
        if (character == ' ' && length_ == 0)
        {
            refuse(feedback);
            show_message("A name starts with a letter or a number");
            return;
        }
        name_[length_] = character;
        char_in_[length_] = 0.0f;
        ++length_;
        name_[length_] = '\0';
        caret_clock_ = 0.0f;
        message_time_ = 0.0f; // typing answers the warning
        if (length_ == kMaxName)
            counter_pop_.trigger(0.6f);
        // The sound comes from where the key is; capitals sound a touch higher.
        const bool capital = character >= 'A' && character <= 'Z';
        feedback.play(audio::Cue::type, capital ? 1.08f : 1.0f, ui::pan_for_x(key_rect(key).cx()));
        if (shift_ == Shift::once && is_letter(character))
            shift_ = Shift::off;
    }

    // repeat: this delete comes from holding Square, not from a press.
    void erase(bool repeat, app::Feedback &feedback)
    {
        if (length_ == 0)
        {
            if (!repeat)
            {
                refuse(feedback);
                field_shake_.trigger();
            }
            return;
        }
        --length_;
        ghosts_[length_] = {name_[length_], name_width(length_), 1.0f};
        name_[length_] = '\0';
        caret_clock_ = 0.0f;
        // The pitch falls as the field empties.
        feedback.play(audio::Cue::erase, 0.9f + 0.014f * static_cast<float>(length_),
                      ui::pan_for_x(kField.x + kNameX + name_width(length_)));
        if (length_ == 0 && shift_ == Shift::off)
            shift_ = Shift::once;
    }

    void cycle_shift(app::Feedback &feedback)
    {
        const float x = ui::pan_for_x(key_rect(kShiftKey).cx());
        switch (shift_)
        {
        case Shift::off:
            shift_ = Shift::once;
            feedback.play(audio::Cue::toggle, 1.0f, x);
            break;
        case Shift::once:
            shift_ = Shift::lock;
            feedback.play(audio::Cue::toggle, 1.12f, x);
            break;
        case Shift::lock:
            shift_ = Shift::off;
            feedback.play(audio::Cue::toggle, 0.9f, x);
            break;
        }
    }

    void finish_name(app::Feedback &feedback)
    {
        while (length_ > 0 && name_[length_ - 1] == ' ')
            name_[--length_] = '\0';
        if (length_ == 0)
        {
            refuse(feedback, 0.7f);
            show_message("Enter a name to continue");
            return;
        }
        feedback.play(audio::Cue::select, 1.06f);
        summary_focus_ = kStartItem;
        go_to(2, 1.0f);
    }

    void update_summary(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = summary_focus_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next <= kStartItem)
            {
                summary_focus_ = next;
                feedback.play(audio::Cue::focus, 1.1f - 0.05f * static_cast<float>(next));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback);
                bump_.trigger();
            }
        }
        if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            go_to(1, -1.0f);
        }
        else if (input.is_pressed(Action::confirm))
        {
            if (summary_focus_ < kPrefs)
            {
                bool &pref = prefs_[summary_focus_];
                pref = !pref;
                feedback.play(audio::Cue::toggle, pref ? 1.06f : 0.94f, 0.3f);
            }
            else
            {
                celebrating_ = true;
                celebrate_time_ = 0.0f;
                button_press_.trigger();
                confetti_.burst(kWarm, 0x51f15eedu + 7919u * ++celebrations_, reduced());
                feedback.play(audio::Cue::complete);
                feedback.rumble(0.7f, 0.18f);
            }
        }
    }

    void update_welcome(const InputFrame &input, float dt, app::Feedback &feedback)
    {
        celebrate_time_ += dt;
        if (input.is_pressed(Action::back))
        {
            // One step back from the welcome is the summary it covers.
            celebrating_ = false;
            feedback.play(audio::Cue::back);
        }
        else if (input.is_pressed(Action::confirm) || celebrate_time_ >= kWelcomeSeconds)
        {
            // The wizard loops for the next viewer; nothing chosen is lost.
            if (celebrate_time_ < kWelcomeSeconds)
                feedback.play(audio::Cue::tab);
            celebrating_ = false;
            go_to(0, 1.0f);
        }
    }

    void animate(float dt)
    {
        const bool calm = reduced();
        transition_.update(dt);
        // The line overshoots the dot it reaches and settles on it.
        progress_.target =
            celebrating_ ? static_cast<float>(kSteps - 1) : static_cast<float>(step_);
        progress_.update(dt, 11.0f, calm ? 1.0f : 0.62f);
        for (int i = 0; i < kSteps; ++i)
        {
            check_[i].target = (celebrating_ || step_ > i) ? 1.0f : 0.0f;
            check_[i].update(dt, 9.0f);
        }

        if (avatar_ != shown_avatar_)
        {
            previous_avatar_ = shown_avatar_;
            shown_avatar_ = avatar_;
            avatar_fade_.start(calm ? 0.12f : 0.3f);
        }
        avatar_fade_.update(dt);
        for (int i = 0; i < kAvatars; ++i)
        {
            avatar_scale_[i].target = i == avatar_ ? kAvatarGrow : kAvatarRest;
            avatar_scale_[i].update(dt, 17.0f, calm ? 1.0f : 0.5f);
        }
        avatar_ring_.target = avatar_x(avatar_);
        avatar_ring_.update(dt, 20.0f);
        avatar_nudge_.update(dt, 9.0f);

        highlight_.target(key_rect(key_));
        highlight_.update(dt, 24.0f);
        wrap_.update(dt, 16.0f);
        bump_.update(dt, 10.0f);
        press_.update(dt, 14.0f);
        shift_blend_.target = shift_ == Shift::off ? 0.0f : 1.0f;
        shift_blend_.update(dt, 22.0f);
        lock_blend_.target = shift_ == Shift::lock ? 1.0f : 0.0f;
        lock_blend_.update(dt, 22.0f);
        for (int i = 0; i < length_; ++i)
            char_in_[i] = std::min(1.0f, char_in_[i] + dt / 0.2f);
        for (Ghost &ghost : ghosts_)
            ghost.fade = std::max(0.0f, ghost.fade - dt / 0.18f);
        caret_clock_ += dt;
        caret_.target = name_width(length_);
        caret_.update(dt, 30.0f);
        placeholder_.target = length_ == 0 ? 1.0f : 0.0f;
        placeholder_.update(dt, 18.0f);
        field_shake_.update(dt, 8.0f);
        counter_pop_.update(dt, 9.0f);
        message_time_ = std::max(0.0f, message_time_ - dt);
        message_level_.target = message_time_ > 0.0f ? 1.0f : 0.0f;
        message_level_.update(dt, 14.0f);

        summary_ring_.target(summary_focus_ < kPrefs ? pref_rect(summary_focus_) : kStart);
        summary_ring_.update(dt, 20.0f);
        for (int i = 0; i < kPrefs; ++i)
        {
            pref_blend_[i].target = prefs_[i] ? 1.0f : 0.0f;
            pref_blend_[i].update(dt, 20.0f, calm ? 1.0f : 0.6f);
        }
        button_press_.update(dt, 10.0f);

        welcome_.target = celebrating_ ? 1.0f : 0.0f;
        welcome_.update(dt, calm ? 40.0f : 13.0f);
        confetti_.update(dt);
    }

    // ---- drawing -----------------------------------------------------------

    // Where a step is while the wizard moves between two of them: the old one
    // leaves quickly against the direction of travel, the new one arrives a
    // beat later from the side the player is heading to.
    View view_of(int step) const
    {
        if (!transition_.running)
            return {step == step_ ? 1.0f : 0.0f, 0.0f};
        const float t = transition_.progress();
        const float travel = reduced() ? 0.0f : kStepSlide;
        if (step == step_)
        {
            const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
            return {tween::smoothstep(arrive),
                    direction_ * travel * (1.0f - tween::quint_out(arrive))};
        }
        if (step == from_step_)
        {
            const float leave = tween::clamp01(t * 2.2f);
            return {1.0f - tween::smoothstep(leave),
                    -direction_ * travel * 0.6f * tween::cubic_in(leave)};
        }
        return {0.0f, 0.0f};
    }

    void draw_indicator(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        const float y = kStepY - 14.0f * (1.0f - in);
        const float first = kCentre - kStepPitch;
        const float progress = std::clamp(progress_.value, 0.0f, static_cast<float>(kSteps - 1));
        list.push_opacity(in);
        list.line(first, y, first + kStepPitch * (kSteps - 1), y, 4, kInk.with_alpha(0.16f));
        if (progress > 0.002f)
            list.line(first, y, first + kStepPitch * progress, y, 4, kWarm);
        for (int i = 0; i < kSteps; ++i)
        {
            const float x = first + kStepPitch * static_cast<float>(i);
            // A dot lights up as the line arrives, not when the step changes.
            const float reached =
                tween::clamp01((progress_.value - (static_cast<float>(i) - 0.3f)) / 0.3f);
            const float here =
                tween::clamp01(1.0f - 2.0f * std::fabs(progress_.value - static_cast<float>(i))) *
                (1.0f - welcome_.value);
            const float done = check_[i].value;
            if (here > 0.01f)
            {
                const float breath = reduced() ? 0.5f : ui::breathe(clock_);
                list.glow({x - kStepDot, y - kStepDot, 2 * kStepDot, 2 * kStepDot}, kStepDot, 18,
                          kWarm.with_alpha(0.3f * here));
                list.ring(x, y, kStepDot + 7.0f + 2.0f * breath, 2.5f,
                          kWarm.with_alpha((0.45f + 0.3f * breath) * here));
            }
            list.circle(x, y, kStepDot, gfx::mix(Color::rgb(0x232a66), kWarm, reached));
            list.ring(x, y, kStepDot, 2, kInk.with_alpha(0.28f * (1.0f - reached)));
            char number[4];
            std::snprintf(number, sizeof(number), "%d", i + 1);
            ui::text(list, fonts.semibold, number, x, y + 8, 22,
                     gfx::mix(kInk.with_alpha(0.75f), kNight, reached).with_alpha(1.0f - done),
                     gfx::Align::center);
            draw_check(list, x, y, 10.0f, done, 4.0f, kNight);
            ui::text(list, fonts.semibold, kStepNames[i], x, y + 58, 16,
                     kInk.with_alpha(0.5f + 0.5f * here), gfx::Align::center, 3.0f);
        }
        list.pop_opacity();
    }

    void draw_heading(gfx::DrawList &list, const char *title, const char *subtitle, float baseline,
                      float size) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 1, 0.07f, 0.5f);
        list.push_opacity(in);
        ui::text(list, fonts.display, title, kCentre, baseline - 16.0f * (1.0f - in), size, kInk,
                 gfx::Align::center);
        if (subtitle != nullptr)
            ui::text(list, fonts.regular, subtitle, kCentre, baseline + 48 - 16.0f * (1.0f - in),
                     26, kInk.with_alpha(0.7f), gfx::Align::center);
        list.pop_opacity();
    }

    // A pill that says what Cross does, in the warm colour of "onward".
    void draw_onward(gfx::DrawList &list, const Rect &rect, const char *label, float size) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float glyph = rect.h * 0.52f;
        const float width = glyph + 14.0f + fonts.semibold.measure(label, size);
        const float x = rect.cx() - width * 0.5f;
        list.push_transform(1.0f - 0.05f * button_press_.value, rect.cx(), rect.cy(), 0, 0);
        list.shadow({rect.x, rect.y + 12, rect.w, rect.h}, rect.h * 0.5f, 26,
                    kShade.with_alpha(0.5f));
        list.glow(rect, rect.h * 0.5f, 22, kWarm.with_alpha(0.28f));
        list.gradient_rect(rect, rect.h * 0.5f, gfx::mix(kWarm, kWhite, 0.3f), kWarm);
        ui::draw_button(list, fonts, ui::GlyphStyle::light(), ui::Button::cross, x, rect.cy(),
                        glyph);
        ui::text(list, fonts.semibold, label, x + glyph + 14.0f, rect.cy() + 0.35f * size, size,
                 kNight);
        list.pop_transform();
    }

    void draw_avatars(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        draw_heading(list, "Pick your avatar",
                     "This is how friends will see you. You can change it later.", 288, 56);

        const float nudge = ui::shake(avatar_nudge_.value, clock_, 16.0f, 8.0f) * avatar_nudge_way_;
        // The focused avatar is drawn last so its shadow and glow lie on top.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < kAvatars; ++i)
            {
                const bool focused = i == avatar_;
                if (focused != (pass == 1))
                    continue;
                const float in = tween::stagger(age_, 2 + i, 0.045f, 0.45f);
                const float scale = avatar_scale_[i].value;
                const float r = kAvatarRadius * scale;
                const float x = avatar_x(i) + nudge * (focused ? 1.0f : 0.35f);
                const float y = kAvatarY + 36.0f * (1.0f - in);
                // 0 at rest .. 1 fully grown: drives everything that sets the
                // chosen one apart.
                const float lit =
                    tween::clamp01((scale - kAvatarRest) / (kAvatarGrow - kAvatarRest));
                list.push_opacity(in);
                if (focused)
                {
                    list.shadow({x - r, y - r + 18, 2 * r, 2 * r}, r, 34, kShade.with_alpha(0.55f));
                    list.glow({x - r, y - r, 2 * r, 2 * r}, r, 40,
                              Color::rgb(kAvatarTable[i].light, 0.35f * lit));
                }
                draw_avatar(list, i, x, y, r);
                // The others step back under a veil instead of going transparent,
                // which would let the waves show through their faces.
                list.circle(x, y, r + 0.5f, Color::rgb(0x0b0f33, 0.3f * (1.0f - lit)));
                list.pop_opacity();
            }
        }
        // The ring is its own object: it glides while the discs grow and shrink.
        const float ring_in = tween::stagger(age_, 2 + avatar_, 0.045f, 0.45f);
        const float breath = reduced() ? 0.5f : ui::breathe(clock_);
        const float ring_r = kAvatarRadius * kAvatarGrow + 13.0f;
        const float ring_x = avatar_ring_.value + nudge;
        list.push_opacity(ring_in);
        list.ring(ring_x, kAvatarY, ring_r + 5.0f, 9.0f, kCool.with_alpha(0.12f + 0.1f * breath));
        list.ring(ring_x, kAvatarY, ring_r, 4.0f, kCool);
        list.pop_opacity();

        // Name of the focused avatar: the old one leaves, the new one arrives.
        const float text_in = tween::stagger(age_, 11, 0.045f, 0.45f);
        list.push_opacity(text_in);
        const auto name_line = [&](int index, float alpha, float slide)
        {
            ui::text(list, fonts.semibold, kAvatarTable[index].name, kCentre + slide, 732, 38,
                     kInk.with_alpha(alpha), gfx::Align::center);
        };
        if (avatar_fade_.running)
        {
            const float t = avatar_fade_.progress();
            const float way = reduced() ? 0.0f : (shown_avatar_ > previous_avatar_ ? 1.0f : -1.0f);
            name_line(previous_avatar_, 1.0f - tween::smoothstep(t * 2.2f),
                      -24.0f * way * tween::cubic_in(tween::clamp01(t * 2.2f)));
            const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
            name_line(shown_avatar_, tween::smoothstep(arrive),
                      30.0f * way * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            name_line(shown_avatar_, 1.0f, 0.0f);
        }
        char count[24];
        std::snprintf(count, sizeof(count), "%d OF %d", avatar_ + 1, kAvatars);
        ui::text(list, fonts.semibold, count, kCentre, 770, 16, kInk.with_alpha(0.5f),
                 gfx::Align::center, 3.0f);
        list.pop_opacity();

        const float button_in = tween::stagger(age_, 13, 0.045f, 0.45f);
        list.push_opacity(button_in);
        draw_onward(list, {kCentre - 160, 828 + 20.0f * (1.0f - button_in), 320, 72}, "Continue",
                    28);
        list.pop_opacity();
    }

    void draw_field(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 2, 0.07f, 0.5f);
        const float warn = message_level_.value;
        Rect field = kField;
        field.x += ui::shake(field_shake_.value, clock_, 12.0f, 9.0f);
        field.y += 18.0f * (1.0f - in);
        list.push_opacity(in);
        list.shadow({field.x, field.y + 12, field.w, field.h}, kPanelRadius, 30,
                    kShade.with_alpha(0.4f));
        if (warn > 0.01f)
            list.glow(field, kPanelRadius, 22, kWarn.with_alpha(0.22f * warn));
        list.rounded_rect(field, kPanelRadius, kPanel.with_alpha(0.86f));
        list.bordered_rect(field, kPanelRadius, kClear, 2.0f + warn,
                           gfx::mix(kInk.with_alpha(0.24f), kWarn, warn));
        draw_avatar(list, avatar_, field.x + 58, field.cy(), 32);

        const float size = name_size();
        const float x = field.x + kNameX;
        const float baseline = field.cy() + 0.35f * size;
        if (placeholder_.value > 0.01f)
            ui::text(list, fonts.semibold, "Your name", x + 18, baseline, size,
                     kInk.with_alpha(0.3f * placeholder_.value));
        // Each character is placed on its own so it can arrive on its own: it
        // rises into the line while it fades in.
        const float rise = reduced() ? 0.0f : 16.0f;
        for (int i = 0; i < length_; ++i)
        {
            const float arrive = tween::cubic_out(char_in_[i]);
            ui::text(list, fonts.semibold, {name_ + i, 1}, x + name_width(i),
                     baseline + rise * (1.0f - arrive), size, kInk.with_alpha(arrive));
        }
        // ... and a deleted one sinks out below it.
        for (const Ghost &ghost : ghosts_)
        {
            if (ghost.fade <= 0.01f)
                continue;
            ui::text(list, fonts.semibold, {&ghost.character, 1}, x + ghost.x,
                     baseline + rise * (1.0f - ghost.fade), size,
                     kInk.with_alpha(0.7f * ghost.fade));
        }
        // The caret is solid right after an edit, then blinks softly.
        const float wave = 0.5f + 0.5f * std::cos(caret_clock_ * 2.0f * kPi);
        const float blink = reduced() ? 1.0f : 0.12f + 0.88f * tween::smoothstep(wave * 1.8f);
        list.rounded_rect({x + caret_.value + 5.0f, field.cy() - size * 0.48f, 4.0f, size * 0.96f},
                          2.0f, kWarm.with_alpha(blink));

        // The counter is monospaced so it does not shift as the digits change.
        char count[16];
        std::snprintf(count, sizeof(count), "%d / %d", length_, kMaxName);
        const float full = length_ >= kMaxName ? 1.0f : 0.0f;
        const float right = field.x + field.w - 34.0f;
        list.push_transform(1.0f + 0.22f * counter_pop_.value, right - 40.0f, field.cy(), 0, 0);
        ui::text(list, fonts.mono, count, right, field.cy() + 9, 24,
                 gfx::mix(kInk.with_alpha(0.6f), kWarm, std::max(full, counter_pop_.value)),
                 gfx::Align::right);
        list.pop_transform();

        // One line under the field: a quiet tip that gives way to the reason
        // of a refusal.
        const float line = field.y + field.h + 36.0f;
        ui::text(list, fonts.regular, "Up to 14 characters. Hold Square to delete faster.", kCentre,
                 line, 24, kInk.with_alpha(0.55f * (1.0f - warn)), gfx::Align::center);
        if (warn > 0.01f)
        {
            const float width = fonts.semibold.measure(message_, 24) + 34.0f;
            const float left = kCentre - width * 0.5f + ui::shake(field_shake_.value, clock_, 6.0f);
            list.push_opacity(warn);
            list.circle(left + 12, line - 8, 12, kWarn);
            ui::text(list, fonts.semibold, "!", left + 12, line - 1, 20, kNight,
                     gfx::Align::center);
            ui::text(list, fonts.semibold, message_, left + 34, line, 24, kWarn);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    // The entrance and the press of one key, as a transform around its centre.
    void push_key(gfx::DrawList &list, int index, const Rect &rect) const
    {
        const float in = tween::stagger(age_, 4 + key_info(index).row, 0.05f, 0.45f);
        const float press = index == pressed_key_ ? press_.value : 0.0f;
        list.push_opacity(in);
        list.push_transform(1.0f - kPressDip * press, rect.cx(), rect.cy(), 0, 22.0f * (1.0f - in));
    }

    static void pop_key(gfx::DrawList &list)
    {
        list.pop_transform();
        list.pop_opacity();
    }

    // Glyph, optional icon and label of a wide key, centred as one group.
    void draw_wide_label(gfx::DrawList &list, const Key &key, const Rect &rect, Color ink) const
    {
        const ui::Fonts &fonts = context_.fonts;
        constexpr float kGlyph = 32.0f;
        constexpr float kText = 25.0f;
        ui::Button button = ui::Button::none;
        const char *label = "";
        float icon = 0.0f; // width kept for the shift arrow
        switch (key.kind)
        {
        case KeyKind::shift:
            button = ui::Button::l2;
            label = "Shift";
            icon = 30.0f;
            break;
        case KeyKind::space:
            button = ui::Button::triangle;
            label = "Space";
            break;
        case KeyKind::erase:
            button = ui::Button::square;
            label = "Delete";
            break;
        default:
            button = ui::Button::r2;
            label = "Done";
            break;
        }
        const float glyph_w = ui::button_width(button, kGlyph);
        const float width = glyph_w + 12.0f + icon + fonts.semibold.measure(label, kText);
        float x = rect.cx() - width * 0.5f;
        ui::draw_button(list, fonts, ui::GlyphStyle::dark(), button, x, rect.cy(), kGlyph);
        x += glyph_w + 12.0f;
        if (key.kind == KeyKind::shift)
        {
            // The arrow tells the three states apart without words: outline
            // (off), filled (next letter), filled with a bar (locked).
            const float cx = x + 10.0f;
            const float cy = rect.cy();
            const float armed = shift_blend_.value;
            const Color lit = gfx::mix(ink, gfx::mix(kWarm, kNight, ink_cover(rect)), armed);
            list.triangle({cx - 11, cy - 14, 22, 15}, lit, 2.6f);
            list.triangle({cx - 11, cy - 14, 22, 15}, lit.with_alpha(armed));
            list.line(cx, cy + 2, cx, cy + 8, 5.0f, lit.with_alpha(0.35f + 0.65f * armed));
            list.line(cx - 8, cy + 14, cx + 8, cy + 14, 3.5f, lit.with_alpha(lock_blend_.value));
            x += icon;
        }
        ui::text(list, fonts.semibold, label, x, rect.cy() + 0.35f * kText, kText, ink);
    }

    // How much of a key the highlight covers right now.
    float ink_cover(const Rect &rect) const
    {
        return coverage(highlight_.value(), rect);
    }

    void draw_keyboard(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 3, 0.07f, 0.5f);
        list.push_opacity(in);
        list.shadow({kBoard.x, kBoard.y + 16, kBoard.w, kBoard.h}, kPanelRadius, 40,
                    kShade.with_alpha(0.45f));
        list.rounded_rect(kBoard, kPanelRadius, kPanel.with_alpha(0.72f));
        list.bordered_rect(kBoard, kPanelRadius, kClear, 1.5f, kInk.with_alpha(0.12f));
        list.pop_opacity();

        // 1. The tiles.
        for (int i = 0; i < kKeys; ++i)
        {
            const Key &key = key_info(i);
            const Rect rect = key_rect(i);
            Color top = kInk.with_alpha(key.kind == KeyKind::character ? 0.13f : 0.2f);
            Color bottom = kInk.with_alpha(key.kind == KeyKind::character ? 0.07f : 0.12f);
            if (key.kind == KeyKind::done)
            {
                // The way onward is the one warm key on the board.
                top = gfx::mix(kWarm, kWhite, 0.3f);
                bottom = kWarm;
            }
            push_key(list, i, rect);
            list.gradient_rect(rect, kKeyRadius, top, bottom);
            if (key.kind == KeyKind::shift && shift_blend_.value > 0.01f)
                list.bordered_rect(rect, kKeyRadius, kClear, 2.5f,
                                   kWarm.with_alpha(shift_blend_.value));
            pop_key(list);
        }

        // 2. The highlight, clipped to the board so that a wrap reads as
        //    "out through this edge, in through that one".
        const float focus_in = tween::stagger(age_, 4 + key_info(key_).row, 0.05f, 0.45f);
        const float breath = reduced() ? 0.5f : ui::breathe(clock_);
        Rect focus = highlight_.value();
        focus.y += ui::shake(bump_.value, clock_, 9.0f, 9.0f);
        list.push_clip(kBoard);
        list.push_opacity(focus_in);
        if (wrap_.value > 0.01f)
        {
            Rect leaving = wrap_from_;
            leaving.x += wrap_way_ * (1.0f - wrap_.value) * (kKeyW + kBoardPad);
            list.rounded_rect(leaving, kKeyRadius, kCool.with_alpha(0.85f * wrap_.value));
        }
        const float press = pressed_key_ == key_ ? press_.value : 0.0f;
        list.push_transform(1.0f - kPressDip * press, focus.cx(), focus.cy(), 0, 0);
        list.shadow({focus.x, focus.y + 8, focus.w, focus.h}, kKeyRadius, 18,
                    kShade.with_alpha(0.5f));
        list.glow(focus, kKeyRadius, 20, kCool.with_alpha(0.3f + 0.18f * breath));
        list.gradient_rect(focus, kKeyRadius, gfx::mix(kCool, kWhite, 0.45f), kCool);
        list.pop_transform();
        list.pop_opacity();
        list.pop_clip();

        // 3. The caps, coloured by the highlight above them.
        const float upper = shift_blend_.value;
        for (int i = 0; i < kKeys; ++i)
        {
            const Key &key = key_info(i);
            const Rect rect = key_rect(i);
            const Color ink =
                key.kind == KeyKind::done ? kNight : gfx::mix(kInk, kNight, ink_cover(rect));
            push_key(list, i, rect);
            if (key.kind != KeyKind::character)
            {
                draw_wide_label(list, key, rect, ink);
            }
            else if (key.lower != key.upper)
            {
                // Both cases are drawn; shift cross-fades between them.
                const float baseline = rect.cy() + 12.0f;
                ui::text(list, fonts.semibold, {&key.lower, 1}, rect.cx(), baseline, 36,
                         ink.with_alpha(1.0f - upper), gfx::Align::center);
                ui::text(list, fonts.semibold, {&key.upper, 1}, rect.cx(), baseline, 36,
                         ink.with_alpha(upper), gfx::Align::center);
            }
            else
            {
                ui::text(list, fonts.semibold, {&key.lower, 1}, rect.cx(), rect.cy() + 12.0f, 36,
                         ink.with_alpha(key.row == 0 ? 0.85f : 1.0f), gfx::Align::center);
            }
            pop_key(list);
        }
    }

    void draw_name(gfx::DrawList &list) const
    {
        draw_heading(list, "Name your profile", nullptr, 224, 42);
        draw_field(list);
        draw_keyboard(list);
    }

    void draw_summary(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        draw_heading(list, "All set", "Check your profile, then start.", 250, 56);

        const float in = tween::stagger(age_, 2, 0.07f, 0.5f);
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, 24.0f * (1.0f - in));
        list.shadow({kCard.x, kCard.y + 18, kCard.w, kCard.h}, 36, 44, kShade.with_alpha(0.5f));
        list.rounded_rect(kCard, 36, kPanel.with_alpha(0.86f));
        list.bordered_rect(kCard, 36, kClear, 1.5f, kInk.with_alpha(0.16f));

        const float ax = kCard.x + 128;
        const float ay = kCard.y + 122;
        list.glow({ax - 76, ay - 76, 152, 152}, 76, 30,
                  Color::rgb(kAvatarTable[avatar_].light, 0.28f));
        draw_avatar(list, avatar_, ax, ay, 76);
        const float x = kCard.x + 244;
        ui::text(list, fonts.semibold, "PROFILE", x, kCard.y + 80, 16, kCool, gfx::Align::left,
                 4.0f);
        // The widest 14 characters still fit: the size follows the width.
        const float room = kCard.w - 244 - 48;
        const float wide = fonts.display.measure(name(), 60);
        const float size = wide > room ? 60.0f * room / wide : 60.0f;
        ui::text(list, fonts.display, name(), x - 2, kCard.y + 146, size, kInk);
        char line[64];
        std::snprintf(line, sizeof(line), "%s avatar  \xC2\xB7  Local profile",
                      kAvatarTable[avatar_].name);
        ui::text(list, fonts.regular, line, x, kCard.y + 190, 24, kInk.with_alpha(0.65f));
        list.rounded_rect({kCard.x + 32, kCard.y + 232, kCard.w - 64, 1.5f}, 0,
                          kInk.with_alpha(0.12f));

        for (int i = 0; i < kPrefs; ++i)
        {
            const Rect row = pref_rect(i);
            const float on = tween::clamp01(pref_blend_[i].value);
            ui::text(list, fonts.regular, kPrefNames[i], row.x + 28, row.cy() + 9, 26,
                     kInk.with_alpha(summary_focus_ == i ? 1.0f : 0.82f));
            // A switch: the track takes the cool colour as the thumb crosses.
            const Rect track{row.x + row.w - 24 - 72, row.cy() - 19, 72, 38};
            list.rounded_rect(track, 19, gfx::mix(kInk.with_alpha(0.2f), kCool, on));
            list.circle(track.x + 19 + 34 * pref_blend_[i].value, track.cy(), 14,
                        gfx::mix(kInk, kNight, on));
        }
        list.pop_transform();
        list.pop_opacity();

        const float button_in = tween::stagger(age_, 4, 0.07f, 0.5f);
        list.push_opacity(button_in);
        list.push_transform(1.0f, 0, 0, 0, 24.0f * (1.0f - button_in));
        draw_onward(list, kStart, "Start", 34);
        // One ring for the whole step: it morphs from a switch row to the button.
        const float breath = reduced() ? 0.5f : ui::breathe(clock_);
        Rect ring = summary_ring_.value().inset(-7.0f);
        ring.y += ui::shake(bump_.value, clock_, 9.0f, 9.0f);
        // A glow would wash over what the ring surrounds, so its light is a
        // second, wider stroke.
        const Rect halo = ring.inset(-5.0f);
        list.bordered_rect(halo, halo.h * 0.5f, kClear, 8.0f,
                           kCool.with_alpha(0.12f + 0.1f * breath));
        list.bordered_rect(ring, ring.h * 0.5f, kClear, 3.5f, kCool);
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_welcome(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float t = welcome_.value;
        const bool calm = reduced();
        const auto pop = [&](float delay)
        { return calm ? 1.0f : tween::back_out((celebrate_time_ - delay) / 0.5f); };
        const Rect card{kCentre - 500, 270 + 50.0f * (1.0f - t), 1000, 500};
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.shadow({card.x, card.y + 22, card.w, card.h}, 44, 60, kShade.with_alpha(0.55f));
        // Frosted panel: the blurred wizard, a tint, then a hairline of light.
        list.glass(glass, card, 44, kWhite);
        list.rounded_rect(card, 44, kPanel.with_alpha(0.6f));
        list.bordered_rect(card, 44, kClear, 1.5f, kWhite.with_alpha(0.24f));

        // The avatar pops first, the line under it a beat later.
        const float r = 96.0f * pop(0.0f);
        const float ay = card.y + 146;
        list.glow({kCentre - r, ay - r, 2 * r, 2 * r}, r, 50,
                  Color::rgb(kAvatarTable[avatar_].light, 0.4f));
        draw_avatar(list, avatar_, kCentre, ay, r);

        char line[48];
        std::snprintf(line, sizeof(line), "Welcome, %s", name_);
        const float room = card.w - 120;
        const float wide = fonts.display.measure(line, 76);
        const float size = wide > room ? 76.0f * room / wide : 76.0f;
        const float grow = 0.72f + 0.28f * pop(0.1f);
        list.push_transform(grow, kCentre, card.y + 318, 0, 0);
        ui::text(list, fonts.display, line, kCentre, card.y + 342, size, kInk, gfx::Align::center);
        list.pop_transform();
        ui::text(list, fonts.regular, "Your profile is ready. Have fun!", kCentre, card.y + 398, 28,
                 kInk.with_alpha(0.78f), gfx::Align::center);
        // Honest about what happens next: the bar is the time left before the
        // wizard starts over by itself.
        const Rect bar{kCentre - 130, card.y + 440, 260, 6};
        const float left = 1.0f - tween::clamp01(celebrate_time_ / kWelcomeSeconds);
        list.rounded_rect(bar, 3, kInk.with_alpha(0.16f));
        if (left > 0.01f)
            list.rounded_rect({bar.x, bar.y, std::max(6.0f, bar.w * left), bar.h}, 3, kWarm);
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        constexpr float kRight = 1824.0f;
        if (celebrating_)
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Continue"},
                                      {ui::Button::circle, "Back"}};
            list.push_opacity(welcome_.value);
            ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            list.pop_opacity();
            return;
        }
        // Each step has its own row; they cross-fade with the steps.
        const float in = tween::stagger(age_, 14, 0.045f, 0.5f) * (1.0f - welcome_.value);
        for (int step = 0; step < kSteps; ++step)
        {
            const float alpha = view_of(step).alpha * in;
            if (alpha <= 0.01f)
                continue;
            list.push_opacity(alpha);
            if (step == 0)
            {
                const ui::Hint hints[] = {{ui::Button::dpad, "Choose"},
                                          {ui::Button::cross, "Continue"}};
                ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            }
            else if (step == 1)
            {
                const ui::Hint hints[] = {
                    {ui::Button::cross, "Type"},     {ui::Button::square, "Delete"},
                    {ui::Button::triangle, "Space"}, {ui::Button::l2, "Shift"},
                    {ui::Button::r2, "Done"},        {ui::Button::circle, "Back"},
                };
                ui::draw_hints(list, fonts, style, hints, 6, kRight, true);
            }
            else
            {
                const ui::Hint hints[] = {
                    {ui::Button::cross, summary_focus_ < kPrefs ? "Switch" : "Start"},
                    {ui::Button::circle, "Back"},
                };
                ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            }
            list.pop_opacity();
        }
    }

    app::Context &context_;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion
    float drift_ = 0.0f; // backdrop time: stands still under "Reduce motion"

    // The wizard.
    int step_ = 0;
    int from_step_ = 0;      // the step that is leaving during a transition
    float direction_ = 1.0f; // 1 forward, -1 back
    tween::Timer transition_;
    tween::Bounce progress_; // filled part of the indicator line, in steps
    tween::Spring check_[kSteps];
    ui::Pulse button_press_;
    ui::Pulse bump_; // a refused up or down

    // Step 1.
    int avatar_ = 0;
    int shown_avatar_ = 0;    // the name under the row ...
    int previous_avatar_ = 0; // ... and the one it is fading out
    tween::Timer avatar_fade_;
    tween::Bounce avatar_scale_[kAvatars];
    tween::Spring avatar_ring_;
    ui::Pulse avatar_nudge_;
    float avatar_nudge_way_ = 0.0f;

    // Step 2.
    char name_[kMaxName + 1];
    int length_ = 0;
    float char_in_[kMaxName] = {}; // arrival progress of each character
    Ghost ghosts_[kMaxName];
    int key_ = kFirstKey;
    int column_ = kFirstKey % kColumns; // the column the player is in, also on wide keys
    Shift shift_ = Shift::off;
    tween::Spring shift_blend_;
    tween::Spring lock_blend_;
    ui::SpringRect highlight_;
    Rect wrap_from_; // the highlight that left through an edge
    float wrap_way_ = 0.0f;
    ui::Pulse wrap_;
    int pressed_key_ = -1;
    ui::Pulse press_;
    tween::Spring caret_;
    float caret_clock_ = 0.0f; // seconds since the last edit
    tween::Spring placeholder_;
    ui::Pulse field_shake_;
    ui::Pulse counter_pop_;
    const char *message_ = "";
    float message_time_ = 0.0f;
    tween::Spring message_level_;
    float erase_hold_ = 0.0f; // how long Square has been held

    // Step 3 and the welcome.
    bool prefs_[kPrefs] = {true, true};
    tween::Bounce pref_blend_[kPrefs];
    int summary_focus_ = kStartItem;
    ui::SpringRect summary_ring_;
    bool celebrating_ = false;
    float celebrate_time_ = 0.0f;
    tween::Spring welcome_;
    ui::Confetti confetti_;
    std::uint32_t celebrations_ = 0;
};

} // namespace

std::unique_ptr<app::Concept> make_keyboard(app::Context &context)
{
    return std::make_unique<FirstRun>(context);
}

} // namespace hui::concepts
