// ps5-homebrew-ui - Design "Now Playing": a living-room music player.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A now-playing screen: large artwork, the track, a spectrum visualizer, a
// scrubber, a transport row and a frosted queue. It plays no audio (a design
// has no access to the music player); it keeps its own playback clock, which
// is all a UI reference needs. What makes it feel finished:
//
//   - rhythm: one pseudo-spectrum drives the visualizer bars, the breathing
//     glow behind the artwork, the play button's halo and the tiny equalizer
//     in the queue, so the whole screen appears to listen to the same music;
//   - the artwork is the play state: it rests at 94 % and dims when paused and
//     grows back on play, and on a track change the cover and the text slide
//     through in the direction of travel, title first;
//   - one focus ring glides between the transport buttons and locks onto the
//     scrubber's thumb; in the queue a highlight springs between the rows;
//   - the scrubber is a spring chasing the playback clock, so seeks glide and a
//     track change sweeps back instead of jumping; seek ticks rise in pitch
//     with the position and are panned to where the thumb is;
//   - the backdrop, the bars and every accent take the palette of the current
//     album, eased; the queue is real glass over all of it and the layout
//     re-centres itself when the queue slides away.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
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
const Color kInk = Color::rgb(0x0b0d16); // icons on the white play button
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kGold = Color::rgb(0xffd166); // the favourite star

constexpr float kMargin = 96.0f;
constexpr float kArt = 520.0f; // artwork size while playing
constexpr float kArtY = 172.0f;
constexpr float kArtRadius = 40.0f;
constexpr float kPausedScale = 0.94f; // the paused artwork cue
constexpr float kColumnGap = 84.0f;   // artwork to text column
constexpr float kWideMargin = 218.0f; // side margin when the queue is hidden

constexpr int kBars = 32;
constexpr float kBarsBase = 640.0f; // the line the bars stand on
constexpr float kBarsHeight = 204.0f;

constexpr float kScrubY = 772.0f;
constexpr float kTransportY = 888.0f;
constexpr float kPlayRadius = 46.0f;
constexpr float kButtonRadius = 32.0f;

constexpr float kQueueX = 1404.0f;
constexpr float kQueueY = 96.0f;
constexpr float kQueueW = 420.0f;
constexpr float kQueueH = 856.0f;
constexpr float kQueueRadius = 36.0f;
constexpr float kQueueListTop = 104.0f; // from the panel's top to the first row
constexpr float kQueueRow = 66.0f;

constexpr float kSeekStep = 5.0f;     // left / right on the scrubber
constexpr float kSeekJump = 15.0f;    // L2 / R2 from anywhere
constexpr float kRestartAfter = 3.0f; // "previous" restarts a track after this
constexpr float kSoft = 10.0f;        // spring omega of the large, soft moves

constexpr const char *kTechniques[] = {
    "One pseudo-spectrum (sines + a beat, fast attack, slow decay) drives bars, glow and icons",
    "Artwork as play state: 94 % and dimmed when paused, slide-through on track change",
    "A focus ring that glides between round buttons and locks onto the scrubber thumb",
    "Spring-chased scrubber with pitched, panned seek ticks; hold to repeat",
    "Frosted queue (Frame::glass) that slides away while the layout re-centres",
    "Backdrop and accents eased from the current album's palette (ui::SpringColor)",
};

constexpr app::TourStep kTour[] = {
    // Next track; the picture catches the covers and the text mid-slide.
    {0.8f, 0, Direction::right},
    {0.5f, action_bit(Action::confirm)},
    {0.34f, 0, Direction::none, "track-change"},
    // Pause: the artwork shrinks, the bars fall to a flat line.
    {1.2f, 0, Direction::left},
    {0.4f, action_bit(Action::confirm)},
    {1.4f, action_bit(Action::confirm), Direction::none, "paused"},
    // Scrub forward.
    {0.8f, 0, Direction::up},
    {0.4f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.5f, 0, Direction::down, "scrub"},
    // Into the queue, pick a track three rows down.
    {0.4f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.6f, action_bit(Action::confirm), Direction::none, "queue"},
    {0.8f, action_bit(Action::back)},
    // The queue slides away and the screen re-centres; then bring it back.
    {0.4f, action_bit(Action::west)},
    {1.2f, action_bit(Action::west), Direction::none, "wide"},
    {0.8f, 0, Direction::none},
};

// ---- content ---------------------------------------------------------------

// Invented track names. Every album's soundtrack is this table rotated by the
// album's position in the catalogue, so the content is stable between runs.
struct Song
{
    const char *title;
    int seconds;
};
constexpr Song kSongs[] = {
    {"First Light", 214},  {"Open Road", 187},    {"Low Clouds", 242},  {"Paper Boats", 163},
    {"Night Market", 228}, {"Slow Current", 271}, {"Far Shore", 196},   {"Homeward", 254},
    {"Hidden Rooms", 205}, {"Small Hours", 289},  {"Morning Run", 176}, {"Last Lantern", 233},
};
constexpr int kSongCount = static_cast<int>(sizeof(kSongs) / sizeof(kSongs[0]));
constexpr int kQueueCount = kSongCount; // the playlist: one track from each of twelve albums

struct Track
{
    int album = 0;  // catalogue index
    int number = 1; // position on that album
    const char *title = "";
    int seconds = 0;
    float bpm = 100.0f; // tempo of the pretend music: sets the visualizer's beat
};

// Track `number` (0-based) of an album's soundtrack.
Track album_track(int album, int number)
{
    const int song = (album * 5 + number) % kSongCount;
    Track track;
    track.album = album;
    track.number = number + 1;
    track.title = kSongs[song].title;
    track.seconds = kSongs[song].seconds + (album % 4) * 3;
    track.bpm = 92.0f + static_cast<float>((song * 7) % 36);
    return track;
}

enum class Zone
{
    scrubber,
    transport,
    queue,
};

enum Control
{
    kShuffle,
    kPrevious,
    kPlay,
    kNext,
    kRepeat,
    kControls,
};
constexpr float kControlOffset[kControls] = {-212.0f, -114.0f, 0.0f, 114.0f, 212.0f};

void format_time(char *out, std::size_t size, float seconds)
{
    const int whole = std::max(0, static_cast<int>(seconds));
    std::snprintf(out, size, "%d:%02d", whole / 60, whole % 60);
}

// A triangle pointing left (dir -1) or right (dir 1), `size` tall. The kit's
// triangle only points up and polygons are not anti-aliased, so this is three
// capsule lines thick enough to close the middle: smooth edges, soft corners.
void side_triangle(gfx::DrawList &list, float cx, float cy, float size, float dir, Color color)
{
    const float stroke = size * 0.4f;
    const float half = size * 0.3f;
    const float back = cx - dir * size * 0.22f;
    const float tip = cx + dir * size * 0.3f;
    list.line(back, cy - half, back, cy + half, stroke, color);
    list.line(back, cy - half, tip, cy, stroke, color);
    list.line(back, cy + half, tip, cy, stroke, color);
}

class Player final : public app::Concept
{
  public:
    explicit Player(app::Context &context) : context_(context)
    {
        // The playlist takes one track from every other album, so each track
        // change brings a new cover and a new palette.
        const int albums = static_cast<int>(context.catalog.size());
        int total = 0;
        for (int i = 0; i < kQueueCount; ++i)
        {
            const int album = (i * 2 + 1) % std::max(1, albums);
            const int number = ((i - album * 5) % kSongCount + kSongCount) % kSongCount;
            tracks_[static_cast<std::size_t>(i)] = album_track(album, number);
            total += tracks_[static_cast<std::size_t>(i)].seconds;
        }
        total_minutes_ = (total + 30) / 60;
        favorite_.fill(false);
        favorite_[4] = true;

        previous_ = index_;
        apply_palette(true);
        shown_.snap(position_ / duration());
        play_.snap(1.0f);
        morph_.snap(1.0f);
        queue_.snap(1.0f);
        queue_row_.snap(static_cast<float>(index_));
        const Rect ring = ring_target();
        ring_.snap(ring);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "player",
            "Now Playing",
            "A music player: breathing artwork, a visualizer, a scrubber and a glass queue",
            "src/concepts/player.cpp",
            audio::SoundSet::glass,
            Color::rgb(0xff8fb1),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f; // the playback state stays: coming back finds the music where it was
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        const bool reduced = context_.settings.reduced_motion;
        if (!reduced)
            drift_ += dt;

        update_shortcuts(input, dt, feedback);
        switch (zone_)
        {
        case Zone::scrubber:
            update_scrubber(input, feedback);
            break;
        case Zone::transport:
            update_transport(input, feedback);
            break;
        case Zone::queue:
            update_queue(input, feedback);
            break;
        }

        // ---- the playback clock ----
        if (playing_)
        {
            position_ += dt;
            if (position_ >= duration())
                finish_track();
        }

        // ---- animation state ----
        update_spectrum(dt);
        const float glide = reduced ? 40.0f : kSoft;
        shown_.target = position_ / duration();
        shown_.update(dt, 14.0f);
        play_.target = playing_ ? 1.0f : 0.0f;
        play_.update(dt, glide);
        morph_.target = play_.target;
        morph_.update(dt, reduced ? 40.0f : 16.0f);
        // The glow follows the bass, but softly: it should breathe, not blink.
        glow_.target = energy_;
        glow_.update(dt, 9.0f);
        queue_.target = queue_open_ ? 1.0f : 0.0f;
        queue_.update(dt, reduced ? 40.0f : 11.0f);
        swap_.update(dt);
        for (ui::SpringColor &colour : palette_)
            colour.update(dt, 3.5f);
        accent_.update(dt, 4.0f);
        body_.update(dt, 4.0f);

        scrub_focus_.target = zone_ == Zone::scrubber ? 1.0f : 0.0f;
        scrub_focus_.update(dt, 16.0f);
        queue_focus_.target = zone_ == Zone::queue ? 1.0f : 0.0f;
        queue_focus_.update(dt, 16.0f);
        ring_.target(ring_target());
        ring_.update(dt, 18.0f);
        queue_row_.target = static_cast<float>(queue_row_index_);
        queue_row_.update(dt, 18.0f);
        // The list keeps the focused row in view; left alone it follows the
        // track that is playing.
        const float row = static_cast<float>(zone_ == Zone::queue ? queue_row_index_ : index_);
        queue_scroll_.reveal(row * kQueueRow, (row + 1.0f) * kQueueRow, queue_view_height(),
                             kQueueRow, static_cast<float>(kQueueCount) * kQueueRow);
        queue_scroll_.update(dt, 12.0f);

        nudge_.update(dt, 9.0f);
        seek_.update(dt, 6.0f);
        // The time bubble stays up while the scrubber has the focus, and for a
        // moment after a seek made from elsewhere (L2 / R2).
        seek_hold_ = std::max(0.0f, seek_hold_ - dt);
        bubble_.target = zone_ == Zone::scrubber || seek_hold_ > 0.0f ? 1.0f : 0.0f;
        bubble_.update(dt, 16.0f);
        star_.update(dt, 5.0f);
        volume_flash_.update(dt, 1.2f);
        for (ui::Pulse &press : press_)
            press.update(dt, 10.0f);
        hints_.update(dt);
        if (zone_ != hinted_zone_)
        {
            hinted_zone_ = zone_;
            hints_.start(0.22f);
        }
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::bokeh;
        frame.backdrop.colors[0] = palette_[0].value();
        frame.backdrop.colors[1] = palette_[1].value();
        frame.backdrop.colors[2] = palette_[2].value();
        frame.backdrop.colors[3] = palette_[3].value();
        frame.backdrop.time = drift_;

        const Layout l = layout();
        gfx::DrawList &list = frame.scene;
        draw_header(list, l);
        draw_artwork(list, l);
        draw_info(list, l);
        draw_visualizer(list, l);
        // The ring's light goes under the controls and its line over them, so
        // the glow never tints the white play button or the thumb.
        draw_focus_ring(list, l, true);
        draw_scrubber(list, l);
        draw_transport(list, l);
        draw_focus_ring(list, l, false);
        draw_hints(list);

        if (queue_.value > 0.01f)
        {
            frame.glass = true;
            draw_queue(frame.overlay, frame.glass_texture);
        }
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // Where the main block sits. With the queue open it keeps to the left;
    // when the queue slides away the block widens and re-centres.
    struct Layout
    {
        float x = 0.0f;  // left edge of the artwork, the scrubber and the header
        float w = 0.0f;  // width of the scrubber
        float cx = 0.0f; // centre of the transport row
        Rect art;
        float column_x = 0.0f; // the text and the visualizer
        float column_w = 0.0f;
    };

    Layout layout() const
    {
        const float q = queue_.value;
        Layout l;
        l.x = tween::lerp(kWideMargin, kMargin, q);
        l.w = tween::lerp(gfx::kVirtualWidth - 2.0f * kWideMargin, kQueueX - 64.0f - kMargin, q);
        l.cx = l.x + l.w * 0.5f;
        l.art = {l.x, kArtY, kArt, kArt};
        l.column_x = l.x + kArt + kColumnGap;
        l.column_w = l.w - kArt - kColumnGap;
        return l;
    }

    const Track &track(int index) const
    {
        return tracks_[static_cast<std::size_t>(index)];
    }

    const demo::Item &album(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(track(index).album)];
    }

    float duration() const
    {
        return static_cast<float>(track(index_).seconds);
    }

    float appear(int order) const
    {
        return tween::stagger(age_, order, 0.07f, 0.55f);
    }

    // Entrance slides are decoration: with reduced motion parts only fade in.
    float slide(float in, float distance) const
    {
        return context_.settings.reduced_motion ? 0.0f : distance * (1.0f - in);
    }

    float queue_view_height() const
    {
        return kQueueH - kQueueListTop - 20.0f;
    }

    float control_x(const Layout &l, int control) const
    {
        return l.cx + kControlOffset[control];
    }

    float thumb_x(const Layout &l) const
    {
        return l.x + tween::clamp01(shown_.value) * l.w;
    }

    // The focus ring's target, as a circle: x and y hold the centre (x relative
    // to the row's centre, so the ring rides along when the layout re-centres)
    // and w the diameter.
    Rect ring_target() const
    {
        if (zone_ == Zone::scrubber)
        {
            const Layout l = layout();
            return {(position_ / duration() - 0.5f) * l.w, kScrubY, 52.0f, 52.0f};
        }
        const float radius = (button_ == kPlay ? kPlayRadius : kButtonRadius) + 9.0f;
        return {kControlOffset[button_], kTransportY, radius * 2.0f, radius * 2.0f};
    }

    // The backdrop and the accents come from the album that is playing.
    void apply_palette(bool snap)
    {
        const demo::Item &item = album(index_);
        const Color targets[4] = {
            gfx::mix(item.dark, Color::rgb(0x05060c), 0.45f),
            gfx::mix(item.dark, item.mid, 0.3f),
            item.mid,
            gfx::mix(item.mid, item.accent, 0.6f),
        };
        for (int i = 0; i < 4; ++i)
        {
            if (snap)
                palette_[i].snap(targets[i]);
            else
                palette_[i].target(targets[i]);
        }
        if (snap)
        {
            accent_.snap(item.accent);
            body_.snap(item.mid);
        }
        else
        {
            accent_.target(item.accent);
            body_.target(item.mid);
        }
    }

    // ---- input ----

    void refuse(app::Feedback &feedback, float direction)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_direction_ = direction;
    }

    void set_zone(Zone zone, app::Feedback &feedback)
    {
        zone_ = zone;
        const Layout l = layout();
        switch (zone)
        {
        case Zone::scrubber:
            feedback.play(audio::Cue::focus, 1.1f, ui::pan_for_x(thumb_x(l)));
            break;
        case Zone::transport:
            feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(control_x(l, button_)));
            break;
        case Zone::queue:
            // The highlight starts on the track that is playing.
            queue_row_index_ = index_;
            queue_row_.snap(static_cast<float>(index_));
            feedback.play(audio::Cue::focus, 0.94f, ui::pan_for_x(kQueueX + kQueueW * 0.5f));
            break;
        }
    }

    // Buttons that work from every zone.
    void update_shortcuts(const InputFrame &input, float dt, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::north))
        {
            const std::size_t i = static_cast<std::size_t>(index_);
            favorite_[i] = !favorite_[i];
            feedback.play(favorite_[i] ? audio::Cue::favorite_on : audio::Cue::favorite_off);
            if (favorite_[i])
                star_.trigger();
        }
        if (input.is_pressed(Action::west))
        {
            queue_open_ = !queue_open_;
            feedback.play(queue_open_ ? audio::Cue::open : audio::Cue::modal_close, 1.0f, 0.4f);
            if (!queue_open_ && zone_ == Zone::queue)
                zone_ = Zone::transport; // the focus never stays on something hidden
        }
        if (input.is_pressed(Action::jump_prev))
            seek(-kSeekJump, false, feedback);
        if (input.is_pressed(Action::jump_next))
            seek(kSeekJump, false, feedback);

        // The right stick steps the app's real music volume: the one thing on
        // this screen that is not simulated.
        if (std::fabs(input.stick2_y) > 0.5f)
        {
            volume_hold_ -= dt;
            if (volume_hold_ <= 0.0f)
            {
                volume_hold_ = 0.14f;
                const int next = context_.settings.music_volume + (input.stick2_y < 0.0f ? 1 : -1);
                if (next >= 0 && next <= 10)
                {
                    context_.settings.music_volume = next;
                    context_.settings_changed = true;
                    feedback.play(audio::Cue::slider, 0.8f + 0.04f * static_cast<float>(next),
                                  0.3f);
                }
                volume_flash_.trigger();
            }
        }
        else
        {
            volume_hold_ = 0.0f;
        }
    }

    void update_transport(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::left:
        case Direction::right:
        {
            const int step = input.nav == Direction::right ? 1 : -1;
            const int next = button_ + step;
            if (next >= 0 && next < kControls)
            {
                button_ = next;
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(control_x(layout(), next)));
            }
            else if (!input.nav_repeat)
            {
                // Past the last button lies the queue, when it is showing.
                if (step > 0 && queue_open_)
                    set_zone(Zone::queue, feedback);
                else
                    refuse(feedback, static_cast<float>(step));
            }
            break;
        }
        case Direction::up:
            set_zone(Zone::scrubber, feedback);
            break;
        case Direction::down:
            if (queue_open_)
                set_zone(Zone::queue, feedback);
            else if (!input.nav_repeat)
                refuse(feedback, 0.0f);
            break;
        case Direction::none:
            break;
        }
        if (input.is_pressed(Action::confirm))
            activate(button_, feedback);
    }

    void update_scrubber(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::left:
            seek(-kSeekStep, input.nav_repeat, feedback);
            break;
        case Direction::right:
            seek(kSeekStep, input.nav_repeat, feedback);
            break;
        case Direction::down:
            set_zone(Zone::transport, feedback);
            break;
        case Direction::up:
            if (!input.nav_repeat)
                refuse(feedback, 0.0f);
            break;
        case Direction::none:
            break;
        }
        if (input.is_pressed(Action::confirm))
            activate(kPlay, feedback);
        if (input.is_pressed(Action::back))
        {
            zone_ = Zone::transport;
            feedback.play(audio::Cue::back);
        }
    }

    void update_queue(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::up:
        case Direction::down:
        {
            const int next = queue_row_index_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kQueueCount)
            {
                queue_row_index_ = next;
                // Rows lower in the list sound a little lower.
                feedback.play(audio::Cue::focus, 1.08f - 0.012f * static_cast<float>(next),
                              ui::pan_for_x(kQueueX + kQueueW * 0.5f));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f);
            }
            break;
        }
        case Direction::left:
            set_zone(Zone::transport, feedback);
            break;
        case Direction::right:
            if (!input.nav_repeat)
                refuse(feedback, 1.0f);
            break;
        case Direction::none:
            break;
        }
        if (input.is_pressed(Action::confirm))
        {
            // Choosing the track that is playing starts it again.
            const int row = queue_row_index_;
            if (row == index_)
                position_ = 0.0f;
            else
                change_track(row, row > index_ ? 1 : -1);
            playing_ = true;
            feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(kQueueX + kQueueW * 0.5f));
        }
        if (input.is_pressed(Action::back))
        {
            zone_ = Zone::transport;
            feedback.play(audio::Cue::back);
        }
    }

    void activate(int control, app::Feedback &feedback)
    {
        const float pan = ui::pan_for_x(control_x(layout(), control));
        press_[control].trigger();
        switch (control)
        {
        case kShuffle:
            shuffle_ = !shuffle_;
            feedback.play(audio::Cue::toggle, shuffle_ ? 1.06f : 0.94f, pan);
            break;
        case kRepeat:
            repeat_ = !repeat_;
            feedback.play(audio::Cue::toggle, repeat_ ? 1.06f : 0.94f, pan);
            break;
        case kPlay:
            playing_ = !playing_;
            // Play at the very end of the last track starts it over.
            if (playing_ && position_ >= duration() - 0.01f)
                position_ = 0.0f;
            feedback.play(audio::Cue::select, playing_ ? 1.0f : 0.9f, pan);
            break;
        case kPrevious:
            // The rule every player follows: a few seconds into a track,
            // "previous" means "from the top"; at the top it means the one before.
            if (position_ > kRestartAfter)
            {
                position_ = 0.0f;
                feedback.play(audio::Cue::tab, 0.94f, -0.4f);
            }
            else if (index_ > 0 || repeat_)
            {
                change_track((index_ + kQueueCount - 1) % kQueueCount, -1);
                feedback.play(audio::Cue::tab, 0.94f, -0.4f);
            }
            else
            {
                refuse(feedback, -1.0f);
            }
            break;
        case kNext:
        {
            const int next = following_track();
            if (next >= 0)
            {
                change_track(next, 1);
                feedback.play(audio::Cue::tab, 1.0f, 0.4f);
            }
            else
            {
                refuse(feedback, 1.0f);
            }
            break;
        }
        default:
            break;
        }
    }

    // The track after this one, or -1 at the end of the list without repeat.
    int following_track()
    {
        if (shuffle_)
        {
            // Any other track: a small generator keeps the order repeatable.
            shuffle_state_ = shuffle_state_ * 1664525u + 1013904223u;
            const int hop = 1 + static_cast<int>((shuffle_state_ >> 16) %
                                                 static_cast<std::uint32_t>(kQueueCount - 1));
            return (index_ + hop) % kQueueCount;
        }
        if (index_ + 1 < kQueueCount)
            return index_ + 1;
        return repeat_ ? 0 : -1;
    }

    void change_track(int index, int travel)
    {
        previous_ = index_;
        index_ = index;
        travel_ = static_cast<float>(travel);
        position_ = 0.0f;
        swap_.start(context_.settings.reduced_motion ? 0.14f : 0.6f);
        apply_palette(false);
    }

    // The clock reached the end of the track: carry on, or stop at the end of
    // the list. Nobody pressed anything, so no sound is played.
    void finish_track()
    {
        const int next = following_track();
        if (next >= 0)
        {
            change_track(next, 1);
        }
        else
        {
            position_ = duration();
            playing_ = false;
        }
    }

    void seek(float delta, bool repeat, app::Feedback &feedback)
    {
        const float target = std::clamp(position_ + delta, 0.0f, duration() - 0.25f);
        if (std::fabs(target - position_) < 0.5f)
        {
            // Already at that end of the track.
            if (!repeat)
                refuse(feedback, delta > 0.0f ? 1.0f : -1.0f);
            return;
        }
        position_ = target;
        const float fraction = position_ / duration();
        const Layout l = layout();
        // The tick rises as the track fills and sits where the thumb is.
        feedback.play(audio::Cue::slider, 0.85f + 0.4f * fraction,
                      ui::pan_for_x(l.x + fraction * l.w));
        seek_.trigger();
        seek_hold_ = 1.1f;
    }

    // ---- the pretend music ----

    // A believable spectrum without audio: every bar sums a few sines of
    // different speeds, bass bars are taller, and a beat at the track's tempo
    // kicks the low end (with a softer off-beat in the mids). Each bar then
    // rises fast and falls slowly, like a real meter, and carries a peak cap
    // that hangs for a moment before it drops.
    void update_spectrum(float dt)
    {
        const Track &now = track(index_);
        const float t = position_;
        const float seed = static_cast<float>(index_) * 1.7f;
        const float beat = t * now.bpm / 60.0f;
        const float kick = std::exp(-5.0f * (beat - std::floor(beat)));
        const float off = beat + 0.5f;
        const float snare = std::exp(-7.0f * (off - std::floor(off)));
        float bass = 0.0f;
        for (int i = 0; i < kBars; ++i)
        {
            const float fi = static_cast<float>(i);
            const float u = fi / static_cast<float>(kBars - 1);
            float wanted = 0.0f;
            if (playing_)
            {
                const float shape = 1.0f - 0.62f * std::pow(u, 0.7f);
                const float slow =
                    0.5f + 0.5f * std::sin(t * (1.9f + 0.31f * fi) + fi * 1.7f + seed);
                const float fast = 0.5f + 0.5f * std::sin(t * (5.3f + 0.17f * fi) + fi * 0.6f);
                const float air = 0.5f + 0.5f * std::sin(t * (10.0f + 0.9f * std::fmod(fi, 5.0f)) +
                                                         fi * 2.3f + seed);
                const float low = tween::clamp01(1.0f - u * 3.2f);
                const float mid = std::exp(-(u - 0.45f) * (u - 0.45f) / 0.03f);
                wanted = tween::clamp01(shape * (0.12f + 0.46f * slow + 0.34f * fast * air) +
                                        0.42f * kick * low + 0.22f * snare * mid);
            }
            float &level = levels_[static_cast<std::size_t>(i)];
            const float rate = wanted > level ? 30.0f : 7.0f; // fast attack, slow decay
            level += (wanted - level) * (1.0f - std::exp(-rate * dt));
            float &peak = peaks_[static_cast<std::size_t>(i)];
            peak = std::max(level, peak - 0.7f * dt);
            if (i < 6)
                bass += level;
        }
        energy_ = bass / 6.0f;
    }

    // ---- drawing ----

    void draw_header(gfx::DrawList &list, const Layout &l) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = appear(0);
        char text[64];
        list.push_opacity(in);
        const float y = 116.0f - slide(in, 12.0f);
        const float w = ui::text(list, fonts.display, "Soundtrack Mix", l.x, y, 32, kWhite);
        std::snprintf(text, sizeof(text), "%d tracks  \xC2\xB7  %d min", kQueueCount,
                      total_minutes_);
        ui::text(list, fonts.regular, text, l.x + w + 24, y, 22, kWhite.with_alpha(0.6f));
        list.pop_opacity();
    }

    // One cover with its shadow, as a unit, so two of them can pass each other.
    void draw_cover(gfx::DrawList &list, const Rect &art, int index, float alpha, float dx,
                    float live) const
    {
        if (alpha <= 0.01f)
            return;
        const Rect r{art.x + dx, art.y, art.w, art.h};
        list.push_opacity(alpha);
        list.shadow({r.x, r.y + 28, r.w, r.h}, kArtRadius, 50, Color::rgb(0x000000, 0.5f));
        // Paused artwork also loses a little light.
        const Color tint = gfx::mix(Color::rgb(0xb9bcc8), kWhite, live);
        list.image(album(index).cover, r, gfx::kCanvasUv, tint, kArtRadius);
        list.bordered_rect(r, kArtRadius, kClear, 2, kWhite.with_alpha(0.16f));
        list.pop_opacity();
    }

    void draw_artwork(gfx::DrawList &list, const Layout &l) const
    {
        const bool reduced = context_.settings.reduced_motion;
        const float in = appear(1);
        const float live = play_.value;
        const float pulse = glow_.value;
        Rect art = l.art;
        art.x -= slide(in, 36.0f);

        // Paused: 94 %. Playing: full size, plus a breath of the bass.
        const float scale =
            tween::lerp(kPausedScale, 1.0f, live) + (reduced ? 0.0f : 0.008f * pulse);
        list.push_opacity(in);
        list.push_transform(scale, art.cx(), art.cy(), 0, 0);
        // The glow is the album's accent, spilling further on every beat and
        // sinking back when the music stops.
        const float lit = (0.14f + 0.36f * pulse) * (0.3f + 0.7f * live);
        list.glow(art.inset(-4), kArtRadius + 4, 90.0f + 60.0f * pulse,
                  gfx::mix(accent_.value(), body_.value(), 0.25f).with_alpha(lit));
        if (swap_.running)
        {
            // The old cover leaves in the direction of travel, the new one
            // follows it in from the other side.
            const float t = swap_.progress();
            const float leave = tween::clamp01(t * 2.3f);
            const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
            const float distance = reduced ? 0.0f : 150.0f;
            draw_cover(list, art, previous_, 1.0f - tween::smoothstep(leave),
                       -travel_ * distance * tween::cubic_in(leave), live);
            draw_cover(list, art, index_, tween::smoothstep(arrive * 1.6f),
                       travel_ * distance * (1.0f - tween::quint_out(arrive)), live);
        }
        else
        {
            draw_cover(list, art, index_, 1.0f, 0.0f, live);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // The track's title, artist and album, at an opacity and an offset so two
    // tracks can cross-fade.
    void draw_track_text(gfx::DrawList &list, const Layout &l, int index, float alpha,
                         float dx) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Track &tr = track(index);
        const demo::Item &item = album(index);
        const float x = l.column_x + dx;
        char text[128];
        list.push_opacity(alpha);
        // A long title first drops a size, then takes an ellipsis.
        const float size = fonts.display.measure(tr.title, 76) <= l.column_w ? 76.0f : 60.0f;
        ui::text(list, fonts.display, fonts.display.font->fit(tr.title, size, l.column_w), x - 3,
                 306, size, kWhite);
        ui::text(list, fonts.semibold, fonts.semibold.font->fit(item.studio, 30, l.column_w), x,
                 356, 30, kWhite.with_alpha(0.92f));
        std::snprintf(text, sizeof(text), "%s \xE2\x80\x93 Original Soundtrack  \xC2\xB7  Track %d",
                      item.title, tr.number);
        ui::text(list, fonts.regular, fonts.regular.font->fit(text, 24, l.column_w), x, 396, 24,
                 kWhite.with_alpha(0.62f));
        list.pop_opacity();
    }

    void draw_info(gfx::DrawList &list, const Layout &l) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = appear(2);
        const float live = play_.value;
        const Color accent = gfx::mix(accent_.value(), kWhite, 0.3f);
        char text[48];
        list.push_opacity(in);
        const float enter = slide(in, 30.0f);

        // The state line: the two words trade places with a small vertical slide.
        const float label_y = 206.0f;
        const float x = l.column_x + enter;
        if (live > 0.01f)
            ui::text(list, fonts.semibold, "NOW PLAYING", x, label_y + 8.0f * (1.0f - live), 18,
                     accent.with_alpha(live), gfx::Align::left, 4.0f);
        if (live < 0.99f)
            ui::text(list, fonts.semibold, "PAUSED", x, label_y - 8.0f * live, 18,
                     kWhite.with_alpha(0.7f * (1.0f - live)), gfx::Align::left, 4.0f);
        std::snprintf(text, sizeof(text), "%02d / %02d", index_ + 1, kQueueCount);
        ui::text(list, fonts.mono, text, l.column_x + l.column_w + enter, label_y, 20,
                 kWhite.with_alpha(0.6f), gfx::Align::right);

        if (swap_.running)
        {
            // The old lines leave quickly; the new ones land a beat later.
            const float t = swap_.progress();
            const float distance = context_.settings.reduced_motion ? 0.0f : 1.0f;
            const float leave = tween::clamp01(t * 3.2f);
            const float arrive = tween::clamp01((t - 0.26f) / 0.74f);
            draw_track_text(list, l, previous_, 1.0f - tween::smoothstep(leave),
                            -travel_ * 40.0f * distance * tween::cubic_in(leave));
            draw_track_text(list, l, index_, tween::smoothstep(arrive),
                            travel_ * 56.0f * distance * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_track_text(list, l, index_, 1.0f, enter);
        }
        list.pop_opacity();
    }

    void draw_visualizer(gfx::DrawList &list, const Layout &l) const
    {
        const float in = appear(3);
        const Color top = gfx::mix(accent_.value(), kWhite, 0.4f);
        const Color bottom = gfx::mix(body_.value(), accent_.value(), 0.45f);
        const float width = l.column_w / static_cast<float>(kBars) * 0.56f;
        const float pitch = (l.column_w - width) / static_cast<float>(kBars - 1);
        list.push_opacity(in);
        for (int i = 0; i < kBars; ++i)
        {
            // The bars also rise from the line one after another on entrance.
            const float grow = tween::stagger(age_, i, 0.012f, 0.5f);
            const float level = levels_[static_cast<std::size_t>(i)] * grow;
            const float h = width + level * (kBarsHeight - width);
            const float x = l.column_x + static_cast<float>(i) * pitch;
            // Colour runs from the album's body tone at the foot to its accent
            // at full height, so a tall bar is also a brighter one.
            const Color tip = gfx::mix(bottom, top, 0.35f + 0.65f * level);
            list.gradient_rect({x, kBarsBase - h, width, h}, width * 0.5f, tip, bottom);
            // The floor is a mirror: a short, fading copy below the line.
            const float mirror = width + (h - width) * 0.42f;
            list.gradient_rect({x, kBarsBase + 8.0f, width, mirror}, width * 0.5f,
                               bottom.with_alpha(0.3f), bottom.with_alpha(0.0f));
            // Peak cap: hangs above the bar for a moment, then falls. It
            // belongs to the music, so it goes out with it.
            const float peak = peaks_[static_cast<std::size_t>(i)] * grow;
            const float lifted = peak - level;
            if (lifted > 0.02f)
            {
                const float y = kBarsBase - width - peak * (kBarsHeight - width) - 8.0f;
                list.rounded_rect(
                    {x, y, width, 3.0f}, 1.5f,
                    top.with_alpha(tween::clamp01(lifted * 8.0f) * 0.55f * play_.value));
            }
        }
        list.pop_opacity();
    }

    void draw_scrubber(gfx::DrawList &list, const Layout &l) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = appear(4);
        const float focus = scrub_focus_.value;
        const float thumb = thumb_x(l);
        const Color accent = gfx::mix(accent_.value(), kWhite, 0.25f);
        char text[16];
        list.push_opacity(in);
        const float y = kScrubY + slide(in, 24.0f);

        // The track thickens when the scrubber has the focus.
        const float thick = 8.0f + 4.0f * focus;
        const Rect bar{l.x, y - thick * 0.5f, l.w, thick};
        list.rounded_rect(bar, thick * 0.5f, kWhite.with_alpha(0.16f + 0.06f * focus));
        const float filled = std::max(thick, thumb - l.x);
        list.glow({bar.x, bar.y, filled, bar.h}, thick * 0.5f, 10.0f + 8.0f * focus,
                  accent.with_alpha(0.22f + 0.2f * focus));
        list.gradient_rect_h({bar.x, bar.y, filled, bar.h}, thick * 0.5f,
                             gfx::mix(body_.value(), accent, 0.5f), accent);
        // ... and the thumb grows.
        const float radius = 9.0f + 6.0f * focus + 3.0f * seek_.value;
        list.shadow({thumb - radius, y - radius + 4, radius * 2, radius * 2}, radius, 10,
                    Color::rgb(0x000000, 0.45f));
        list.circle(thumb, y, radius, kWhite);

        format_time(text, sizeof(text), position_);
        ui::text(list, fonts.mono, text, l.x, y + 46, 22, kWhite.with_alpha(0.86f));
        format_time(text, sizeof(text), duration());
        ui::text(list, fonts.mono, text, l.x + l.w, y + 46, 22, kWhite.with_alpha(0.6f),
                 gfx::Align::right);

        // While scrubbing, the time rides above the thumb.
        const float bubble = tween::clamp01(bubble_.value);
        if (bubble > 0.01f)
        {
            const float cx = std::clamp(thumb, l.x + 44.0f, l.x + l.w - 44.0f);
            const Rect pill{cx - 44, y - 66 + 8.0f * (1.0f - bubble), 88, 36};
            list.push_opacity(bubble);
            list.shadow({pill.x, pill.y + 6, pill.w, pill.h}, 18, 14, Color::rgb(0x000000, 0.4f));
            list.rounded_rect(pill, 18, kWhite);
            format_time(text, sizeof(text), shown_.value * duration() + 0.5f);
            ui::text(list, fonts.mono, text, pill.cx(), pill.cy() + 8, 22, kInk,
                     gfx::Align::center);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    void draw_shuffle_icon(gfx::DrawList &list, float cx, float cy, Color c) const
    {
        // Two paths that cross, each ending in an arrow head on the right.
        constexpr float t = 3.5f;
        for (int i = 0; i < 2; ++i)
        {
            const float s = i == 0 ? 1.0f : -1.0f; // the second path is the first, mirrored
            list.line(cx - 15, cy - 8 * s, cx - 7, cy - 8 * s, t, c);
            list.line(cx - 7, cy - 8 * s, cx + 5, cy + 8 * s, t, c);
            list.line(cx + 5, cy + 8 * s, cx + 14, cy + 8 * s, t, c);
            list.line(cx + 14, cy + 8 * s, cx + 9, cy + 8 * s - 5, t, c);
            list.line(cx + 14, cy + 8 * s, cx + 9, cy + 8 * s + 5, t, c);
        }
    }

    void draw_repeat_icon(gfx::DrawList &list, float cx, float cy, Color c) const
    {
        // Two arcs chasing each other round a circle, each ending in an arrow
        // head laid along the tangent at its tip.
        constexpr float t = 3.5f;
        constexpr float radius = 12.5f; // to the middle of the stroke
        constexpr float barb = 5.5f;
        constexpr float sweep = 2.25f;
        for (int i = 0; i < 2; ++i)
        {
            const float start = -0.95f + 3.1415927f * static_cast<float>(i);
            list.arc(cx, cy, radius + t * 0.5f, t, start, sweep, c);
            const float a = start + sweep;
            // Angles run clockwise from 12 o'clock: n points outward, d along the arc.
            const float nx = std::sin(a);
            const float ny = -std::cos(a);
            const float dx = std::cos(a);
            const float dy = std::sin(a);
            const float px = cx + nx * radius + dx * 2.0f;
            const float py = cy + ny * radius + dy * 2.0f;
            list.line(px, py, px - (dx + nx) * barb, py - (dy + ny) * barb, t, c);
            list.line(px, py, px - (dx - nx) * barb, py - (dy - ny) * barb, t, c);
        }
    }

    void draw_skip_icon(gfx::DrawList &list, float cx, float cy, float dir, Color c) const
    {
        side_triangle(list, cx - dir * 6.0f, cy, 24, dir, c);
        list.line(cx + dir * 12.0f, cy - 9, cx + dir * 12.0f, cy + 9, 4.5f, c);
    }

    void draw_transport(gfx::DrawList &list, const Layout &l) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = appear(5);
        const Color accent = gfx::mix(accent_.value(), kWhite, 0.3f);
        list.push_opacity(in);
        const float y = kTransportY + slide(in, 28.0f);

        for (int i = 0; i < kControls; ++i)
        {
            const float cx = control_x(l, i);
            const bool focused = zone_ == Zone::transport && i == button_;
            list.push_transform(1.0f - 0.08f * press_[i].value, cx, y, 0, 0);
            if (i == kPlay)
            {
                // The primary button: a white disc whose halo keeps the beat.
                const float live = play_.value;
                list.glow({cx - kPlayRadius, y - kPlayRadius, kPlayRadius * 2, kPlayRadius * 2},
                          kPlayRadius, 22.0f + 14.0f * glow_.value,
                          accent.with_alpha(0.2f + 0.3f * glow_.value * live));
                list.shadow(
                    {cx - kPlayRadius, y - kPlayRadius + 8, kPlayRadius * 2, kPlayRadius * 2},
                    kPlayRadius, 18, Color::rgb(0x000000, 0.4f));
                list.circle(cx, y, kPlayRadius, kWhite);
                // Play and pause trade places: each shrinks as it fades. The
                // fade is done in colour (toward the disc's white), so the
                // overlapping strokes of an icon never show through.
                const float m = morph_.value; // 1: playing, so the pause bars show
                if (m < 0.99f)
                {
                    const float s = 0.6f + 0.4f * (1.0f - m);
                    side_triangle(list, cx + 2.0f, y, 38.0f * s, 1.0f, gfx::mix(kInk, kWhite, m));
                }
                if (m > 0.01f)
                {
                    const float s = 0.6f + 0.4f * m;
                    const Color c = gfx::mix(kWhite, kInk, m);
                    list.rounded_rect({cx - 15 * s, y - 17 * s, 10 * s, 34 * s}, 4 * s, c);
                    list.rounded_rect({cx + 5 * s, y - 17 * s, 10 * s, 34 * s}, 4 * s, c);
                }
            }
            else
            {
                const bool lit = (i == kShuffle && shuffle_) || (i == kRepeat && repeat_);
                list.circle(cx, y, kButtonRadius, kWhite.with_alpha(focused ? 0.18f : 0.08f));
                list.ring(cx, y, kButtonRadius, 1.5f, kWhite.with_alpha(0.16f));
                // Opaque on purpose: an icon is several strokes that overlap,
                // and a translucent ink would show every crossing.
                const Color c = lit ? accent : (focused ? kWhite : Color::rgb(0xd4d7e2));
                if (i == kShuffle)
                    draw_shuffle_icon(list, cx, y, c);
                else if (i == kRepeat)
                    draw_repeat_icon(list, cx, y, c);
                else
                    draw_skip_icon(list, cx, y, i == kNext ? 1.0f : -1.0f, c);
                // A toggle that is on carries a small lit dot.
                if (lit)
                {
                    list.glow({cx - 4, y + kButtonRadius + 10, 8, 8}, 4, 8,
                              accent.with_alpha(0.6f));
                    list.circle(cx, y + kButtonRadius + 14, 4, accent);
                }
            }
            list.pop_transform();
        }

        // Left end of the row: the favourite star and the button that sets it.
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const bool starred = favorite_[static_cast<std::size_t>(index_)];
        ui::draw_button(list, fonts, style, ui::Button::triangle, l.x, y, 34);
        list.star(l.x + 66, y - 1, 15.0f + 9.0f * star_.value,
                  starred ? kGold : kWhite.with_alpha(0.8f), starred ? 0.0f : 2.5f);

        // Right end: the music volume, a ramp of ten steps on the right stick.
        const int volume = std::clamp(context_.settings.music_volume, 0, 10);
        const float right = l.x + l.w;
        for (int i = 0; i < 10; ++i)
        {
            const float h = 8.0f + 2.0f * static_cast<float>(i);
            const float x = right - 106.0f + static_cast<float>(i) * 11.0f;
            list.rounded_rect({x, y + 13 - h, 6, h}, 3,
                              i < volume ? gfx::mix(kWhite, accent, volume_flash_.value)
                                         : kWhite.with_alpha(0.2f));
        }
        ui::draw_button(list, fonts, style, ui::Button::right_stick, right - 106.0f - 48.0f, y, 34);
        list.pop_opacity();
    }

    // One ring for the scrubber and the transport row. On the buttons it is a
    // plain spring; on the scrubber it locks onto the thumb, so the two never
    // drift apart during a seek.
    void draw_focus_ring(gfx::DrawList &list, const Layout &l, bool light) const
    {
        const float alpha = appear(5) * (1.0f - queue_focus_.value);
        if (alpha <= 0.01f)
            return;
        const Rect ring = ring_.value();
        const float lock = scrub_focus_.value;
        const bool sideways = nudge_direction_ != 0.0f;
        const float cx =
            tween::lerp(l.cx + ring.x, thumb_x(l), lock) +
            (sideways ? ui::shake(nudge_.value, clock_, 12.0f) * nudge_direction_ : 0.0f);
        const float cy = ring.y + slide(appear(5), 28.0f) +
                         (sideways ? 0.0f : ui::shake(nudge_.value, clock_, 8.0f));
        const float radius = ring.w * 0.5f;
        list.push_opacity(alpha);
        if (light)
        {
            const Color accent = gfx::mix(accent_.value(), kWhite, 0.3f);
            list.glow({cx - radius, cy - radius, radius * 2, radius * 2}, radius, 18,
                      accent.with_alpha(0.34f + 0.2f * ui::breathe(clock_)));
        }
        else
        {
            list.ring(cx, cy, radius, 3.5f, kWhite);
        }
        list.pop_opacity();
    }

    // The three-bar equalizer that marks the playing row: it reads three bars
    // of the real spectrum, so it moves with the big visualizer.
    void draw_equalizer(gfx::DrawList &list, float cx, float cy, Color color) const
    {
        constexpr int kSource[3] = {2, 9, 17};
        for (int i = 0; i < 3; ++i)
        {
            const float h = 5.0f + 17.0f * levels_[static_cast<std::size_t>(kSource[i])];
            list.rounded_rect({cx - 10.0f + static_cast<float>(i) * 8.0f, cy + 10.0f - h, 5, h},
                              2.5f, color);
        }
    }

    void draw_queue(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = appear(6);
        const float open = queue_.value * (context_.settings.reduced_motion ? 1.0f : in);
        const Color accent = gfx::mix(accent_.value(), kWhite, 0.3f);
        char text[32];
        // The panel slides in from beyond the right edge.
        const Rect panel{kQueueX + (1.0f - open) * (kQueueW + kMargin + 60.0f), kQueueY, kQueueW,
                         kQueueH};
        list.push_opacity(tween::clamp01(queue_.value * 1.5f) * in);
        list.shadow({panel.x, panel.y + 20, panel.w, panel.h}, kQueueRadius, 56,
                    Color::rgb(0x000000, 0.42f));
        // Frosted glass: the blurred screen, a tint in the album's dark tone,
        // then a hairline of light.
        list.glass(glass, panel, kQueueRadius, kWhite);
        list.rounded_rect(
            panel, kQueueRadius,
            gfx::mix(palette_[0].value(), Color::rgb(0x0b0d16), 0.4f).with_alpha(0.5f));
        list.bordered_rect(panel, kQueueRadius, kClear, 1.5f, kWhite.with_alpha(0.2f));

        ui::text(list, fonts.semibold, "QUEUE", panel.x + 32, panel.y + 56, 18, accent,
                 gfx::Align::left, 4.0f);
        const int after = kQueueCount - 1 - index_;
        if (shuffle_)
            std::snprintf(text, sizeof(text), "Shuffled");
        else if (after > 0)
            std::snprintf(text, sizeof(text), "%d up next", after);
        else
            std::snprintf(text, sizeof(text), repeat_ ? "Repeats" : "Last track");
        ui::text(list, fonts.regular, text, panel.x + panel.w - 32, panel.y + 56, 20,
                 kWhite.with_alpha(0.6f), gfx::Align::right);
        list.rounded_rect({panel.x + 32, panel.y + 84, panel.w - 64, 1.5f}, 0.75f,
                          kWhite.with_alpha(0.12f));

        const float top = panel.y + kQueueListTop - 20.0f;
        const float view = queue_view_height() + 20.0f;
        const Rect clip{panel.x, top, panel.w, view};
        const float origin = panel.y + kQueueListTop - queue_scroll_.offset();
        list.push_clip(clip);

        // The highlight springs between rows; it is there only while the
        // queue has the focus. The playing row keeps a faint plate of its own.
        const float inset = 14.0f;
        const float focus = queue_focus_.value;
        const Rect playing{panel.x + inset, origin + static_cast<float>(index_) * kQueueRow + 3,
                           panel.w - 2 * inset, kQueueRow - 6};
        list.rounded_rect(playing, 18, kWhite.with_alpha(0.07f));
        if (focus > 0.01f)
        {
            Rect row{panel.x + inset, origin + queue_row_.value * kQueueRow + 3,
                     panel.w - 2 * inset, kQueueRow - 6};
            row.x += ui::shake(nudge_.value, clock_, 10.0f, 9.0f) * nudge_direction_;
            row.y += nudge_direction_ == 0.0f ? ui::shake(nudge_.value, clock_, 7.0f, 9.0f) : 0.0f;
            list.glow(row, 18, 14, accent.with_alpha(0.22f * focus));
            list.rounded_rect(row, 18, kWhite.with_alpha(0.16f * focus));
            list.bordered_rect(row, 18, kClear, 2.5f, kWhite.with_alpha(focus));
        }

        // Rows fade as they pass under the edges of the list instead of being
        // cut by the clip, and arrive one after another on entrance.
        const auto row_alpha = [&](int i)
        {
            const float y = origin + static_cast<float>(i) * kQueueRow;
            return tween::clamp01((y - top + 24.0f) / 36.0f) *
                   tween::clamp01((top + view - (y + kQueueRow) + 24.0f) / 36.0f) *
                   tween::stagger(age_, i, 0.03f, 0.4f);
        };
        // Two passes, one per font: text of one face shares a draw call, so
        // twelve rows cost two calls instead of three dozen.
        for (int i = 0; i < kQueueCount; ++i)
        {
            const float alpha = row_alpha(i);
            if (alpha <= 0.01f)
                continue;
            const bool current = i == index_;
            const float cy = origin + (static_cast<float>(i) + 0.5f) * kQueueRow;
            list.push_opacity(alpha);
            if (current)
            {
                draw_equalizer(list, panel.x + 50, cy, accent);
            }
            else
            {
                std::snprintf(text, sizeof(text), "%02d", i + 1);
                ui::text(list, fonts.mono, text, panel.x + 50, cy + 7, 20, kWhite.with_alpha(0.5f),
                         gfx::Align::center);
            }
            if (favorite_[static_cast<std::size_t>(i)])
                list.star(panel.x + panel.w - 112, cy, 8, kGold);
            format_time(text, sizeof(text), static_cast<float>(track(i).seconds));
            ui::text(list, fonts.mono, text, panel.x + panel.w - 34, cy + 7, 20,
                     kWhite.with_alpha(current ? 0.9f : 0.6f), gfx::Align::right);
            list.pop_opacity();
        }
        for (int i = 0; i < kQueueCount; ++i)
        {
            const float alpha = row_alpha(i);
            if (alpha <= 0.01f)
                continue;
            const bool current = i == index_;
            const bool focused = zone_ == Zone::queue && i == queue_row_index_;
            const float cy = origin + (static_cast<float>(i) + 0.5f) * kQueueRow;
            const float room =
                panel.w - 84 - 108 - (favorite_[static_cast<std::size_t>(i)] ? 26.0f : 0.0f);
            const ui::FontRef &face = current ? fonts.semibold : fonts.regular;
            ui::text(list, face, face.font->fit(track(i).title, 24, room), panel.x + 84, cy + 8, 24,
                     kWhite.with_alpha(alpha * (current || focused ? 1.0f : 0.8f)));
        }
        list.pop_clip();
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        constexpr const char *kLabels[kControls] = {"Shuffle", "Previous", "Play", "Next",
                                                    "Repeat"};
        const char *play = playing_ ? "Pause" : "Play";
        const char *queue = queue_open_ ? "Hide queue" : "Show queue";
        // The row fades back in whenever the zone, and so its meaning, changes.
        list.push_opacity(appear(8) *
                          (hints_.running ? tween::smoothstep(hints_.progress()) : 1.0f));
        switch (zone_)
        {
        case Zone::scrubber:
        {
            const ui::Hint hints[] = {{ui::Button::dpad, "Seek 5 s"},
                                      {ui::Button::cross, play},
                                      {ui::Button::square, queue},
                                      {ui::Button::l2, "Seek 15 s", ui::Button::r2}};
            ui::draw_hints(list, fonts, style, hints, 4, 1824, true);
            break;
        }
        case Zone::transport:
        {
            const ui::Hint hints[] = {
                {ui::Button::cross, button_ == kPlay ? play : kLabels[button_]},
                {ui::Button::square, queue},
                {ui::Button::l2, "Seek 15 s", ui::Button::r2}};
            ui::draw_hints(list, fonts, style, hints, 3, 1824, true);
            break;
        }
        case Zone::queue:
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Play track"},
                                      {ui::Button::circle, "Back"},
                                      {ui::Button::square, queue}};
            ui::draw_hints(list, fonts, style, hints, 3, 1824, true);
            break;
        }
        }
        list.pop_opacity();
    }

    app::Context &context_;
    std::array<Track, kQueueCount> tracks_{};
    std::array<bool, kQueueCount> favorite_{};
    int total_minutes_ = 0;

    // ---- playback ----
    int index_ = 2;          // the track that is playing
    float position_ = 47.0f; // seconds into it
    bool playing_ = true;
    bool shuffle_ = false;
    bool repeat_ = false;
    std::uint32_t shuffle_state_ = 0x51f15eedu;

    // ---- focus ----
    Zone zone_ = Zone::transport;
    int button_ = kPlay;
    bool queue_open_ = true;
    int queue_row_index_ = 2;

    // ---- animation ----
    float age_ = 0.0f;                  // seconds since enter(): drives the entrance
    float clock_ = 0.0f;                // free-running, for idle motion
    float drift_ = 0.0f;                // backdrop time; stands still with reduced motion
    std::array<float, kBars> levels_{}; // the spectrum, smoothed
    std::array<float, kBars> peaks_{};  // peak caps
    float energy_ = 0.0f;               // mean of the bass bars
    tween::Spring glow_;                // ... eased: the artwork's breath
    tween::Spring shown_;               // scrubber fraction chasing the clock
    tween::Spring play_;                // 1 playing, 0 paused: artwork scale and light
    tween::Spring morph_;               // the play / pause icon
    tween::Spring queue_;               // 1 open, 0 hidden: panel slide and layout
    tween::Timer swap_;                 // track change cross-fade
    int previous_ = 0;                  // the track that is leaving
    float travel_ = 1.0f;               // 1 forward, -1 back
    ui::SpringColor palette_[4];
    ui::SpringColor accent_;
    ui::SpringColor body_;
    tween::Spring scrub_focus_;
    tween::Spring queue_focus_;
    ui::SpringRect ring_;
    tween::Spring queue_row_;
    ui::Scroller queue_scroll_;
    ui::Pulse nudge_;
    float nudge_direction_ = 0.0f; // -1 / 1 sideways, 0 vertical
    ui::Pulse seek_;               // the thumb's bump on a seek
    float seek_hold_ = 0.0f;       // seconds the time bubble stays after a seek
    tween::Spring bubble_;
    ui::Pulse star_;
    ui::Pulse volume_flash_;
    float volume_hold_ = 0.0f;
    ui::Pulse press_[kControls];
    tween::Timer hints_;
    Zone hinted_zone_ = Zone::transport;
};

} // namespace

std::unique_ptr<app::Concept> make_player(app::Context &context)
{
    return std::make_unique<Player>(context);
}

} // namespace hui::concepts
