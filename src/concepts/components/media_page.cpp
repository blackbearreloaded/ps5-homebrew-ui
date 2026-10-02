// ps5-homebrew-ui - Component Library page: TextView, ImageViewer, MediaControls, LoadingScreen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Content and what surrounds it: an article of patch notes on the left, a
// gallery of covers with zoom and pan at the top right, a player's transport
// under it, and a button that opens the loading screen over the whole page.
// The page owns what the components leave to a screen: the playback clock the
// transport reports to, the load the loading screen shows, and which of the
// four areas has the focus. Square swaps the knobs of all four together.
//
// A real screen would bind the viewer's zoom to the analog triggers. Here L2
// and R2 turn the gallery's pages, so Triangle steps the zoom instead.

#include "concepts/components/page.hpp"

#include "ui/components/image_viewer.hpp"
#include "ui/components/loading_screen.hpp"
#include "ui/components/media_controls.hpp"
#include "ui/components/text_view.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kCaptionY = 268.0f;
constexpr float kRowHeight = 56.0f; // the launcher's row
constexpr float kLaunchWidth = 330.0f;
constexpr int kPictures = 10;
constexpr int kTracks = 6;
constexpr float kLoadSeconds = 3.4f;

// The covers are square with their lettering at the top and the bottom; the
// viewer shows the band between, twice as wide as it is high.
constexpr Rect kBandUv{0.0f, 0.75f, 1.0f, -0.5f};
constexpr float kBandAspect = 2.0f;
// The same for the loading screen's artwork: a 16:9 band.
constexpr Rect kWideUv{0.0f, 0.78125f, 1.0f, -0.5625f};

struct Variant
{
    const char *name;
    Rect text;
    Rect viewer;
    Rect controls;
    float row_y; // the launcher's row, between the viewer and the controls
    const char *text_caption;
    const char *viewer_caption;
    const char *controls_caption;
};
constexpr Variant kVariants[] = {
    {"Full player, filmstrip",
     {96.0f, 286.0f, 600.0f, 656.0f},
     {736.0f, 286.0f, 1088.0f, 348.0f},
     {736.0f, 712.0f, 1088.0f, 230.0f},
     645.0f,
     "TextView: panel, footer, progress",
     "ImageViewer: filmstrip, minimap",
     "MediaControls: full"},
    {"Compact bar, contents",
     {96.0f, 286.0f, 740.0f, 656.0f},
     {876.0f, 286.0f, 948.0f, 416.0f},
     {876.0f, 788.0f, 948.0f, 114.0f},
     716.0f,
     "TextView: contents from the headings",
     "ImageViewer: no strip, wraps around",
     "MediaControls: compact"},
    {"Ghost buttons, reader",
     {96.0f, 286.0f, 600.0f, 656.0f},
     {736.0f, 286.0f, 1088.0f, 358.0f},
     {736.0f, 722.0f, 1088.0f, 220.0f},
     655.0f,
     "TextView: on the page, narrow measure",
     "ImageViewer: no frame, large thumbnails",
     "MediaControls: ghost, hides in 6 s"},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

enum class Area : std::uint8_t
{
    text,
    viewer,
    launch,
    controls,
};

constexpr ui::Hint kTextHints[] = {
    {ui::Button::dpad, "Scroll", ui::Button::right_stick},
    {ui::Button::triangle, "Next section"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kViewerHints[] = {
    {ui::Button::dpad, "Browse"},
    {ui::Button::cross, "Fit / fill"},
    {ui::Button::triangle, "Zoom"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kZoomHints[] = {
    {ui::Button::left_stick, "Pan"},
    {ui::Button::triangle, "Zoom"},
    {ui::Button::circle, "Fit"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kLaunchHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Open"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kControlsHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Select"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kScrubHints[] = {
    {ui::Button::dpad, "Seek"},
    {ui::Button::cross, "Play / pause"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kVolumeHints[] = {
    {ui::Button::dpad, "Volume"},
    {ui::Button::cross, "Done"},
};
constexpr ui::Hint kLoadingHints[] = {
    {ui::Button::dpad, "Tips"},
    {ui::Button::circle, "Cancel"},
};
constexpr ui::Hint kReadyHints[] = {
    {ui::Button::dpad, "Tips"},
    {ui::Button::cross, "Continue"},
};

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.45f, action_bit(Action::north)},
    {0.5f, 0, Direction::right},
    {0.4f, 0, Direction::right},
    {0.4f, 0, Direction::right},
    {0.8f, action_bit(Action::confirm), Direction::none, "media"},
    {0.35f, 0, Direction::none, nullptr, 0.0f, -0.6f},
    {0.5f, action_bit(Action::north), Direction::none, "media-zoom"},
    {0.6f, action_bit(Action::back)},
    {0.4f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.4f, action_bit(Action::west)},
    {0.9f, action_bit(Action::west), Direction::none, "media-compact"},
    {0.5f, 0, Direction::up},
    {0.3f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.7f, action_bit(Action::west), Direction::none, "media-reader"},
    {0.5f, 0, Direction::up},
    {0.4f, action_bit(Action::confirm)},
    {1.9f, 0, Direction::right, "media-loading"},
    {2.4f, action_bit(Action::confirm)},
};

constexpr const char *kTips[] = {
    "Lanterns you light stay lit: they mark the way back when the fog comes in.",
    "Hold Square while steering to trim the sail without leaving the tiller.",
    "The harbour master buys charts of any coastline you have sailed twice.",
    "Storm glass turns cloudy an hour before the weather does. Trust it over the sky.",
    "A full crew rows faster, but an empty bench carries more cargo.",
};

// A load is never even: it stalls on some stages and races through others.
float load_curve(float t)
{
    struct Knot
    {
        float t;
        float value;
    };
    constexpr Knot kKnots[] = {{0.0f, 0.0f},  {0.5f, 0.12f}, {0.9f, 0.14f}, {1.8f, 0.55f},
                               {2.2f, 0.57f}, {3.0f, 0.93f}, {3.4f, 1.0f}};
    for (std::size_t i = 1; i < std::size(kKnots); ++i)
    {
        if (t < kKnots[i].t)
            return tween::lerp(kKnots[i - 1].value, kKnots[i].value,
                               tween::inverse_lerp(kKnots[i - 1].t, kKnots[i].t, t));
    }
    return 1.0f;
}

const char *event_name(ui::Event event)
{
    switch (event)
    {
    case ui::Event::moved:
        return "moved";
    case ui::Event::changed:
        return "changed";
    case ui::Event::activated:
        return "activated";
    case ui::Event::cancelled:
        return "cancelled";
    case ui::Event::refused:
        return "refused";
    case ui::Event::none:
        break;
    }
    return "none";
}

const char *command_name(ui::MediaCommand command)
{
    switch (command)
    {
    case ui::MediaCommand::play:
        return "play";
    case ui::MediaCommand::pause:
        return "pause";
    case ui::MediaCommand::previous:
        return "previous";
    case ui::MediaCommand::next:
        return "next";
    case ui::MediaCommand::rewind:
        return "rewind";
    case ui::MediaCommand::forward:
        return "forward";
    case ui::MediaCommand::seek:
        return "seek";
    case ui::MediaCommand::shuffle:
        return "shuffle";
    case ui::MediaCommand::repeat:
        return "repeat";
    case ui::MediaCommand::volume:
        return "volume";
    case ui::MediaCommand::mute:
        return "mute";
    case ui::MediaCommand::none:
        break;
    }
    return "none";
}

std::vector<ui::TextBlock> patch_notes()
{
    using ui::TextBlock;
    return {
        TextBlock::heading("Update 1.4: Quiet Harbour"),
        TextBlock::key_value("Version", "1.4.0"),
        TextBlock::key_value("Released", "2 October"),
        TextBlock::key_value("Download", "2.3 GB"),
        TextBlock::key_value("Save data", "Compatible"),
        TextBlock::paragraph("The harbour town of Low Wick opens its gates. This update adds a "
                             "new region, a calmer way to sail at night and a long list of fixes "
                             "from your reports. Thank you for every one of them."),
        TextBlock::image("Low Wick at dusk, seen from the breakwater", 150.0f, 9),
        TextBlock::heading("Highlights", 2),
        TextBlock::bullet("Low Wick: a harbour town with eleven new errands and a night market."),
        TextBlock::bullet("Night sailing: lanterns on the bow light the water ahead of you."),
        TextBlock::bullet("Photo mode keeps its settings between sessions."),
        TextBlock::bullet("Loading a save is about a third faster on every console."),
        TextBlock::heading("Balance", 2),
        TextBlock::paragraph("Storms were punishing small boats more than we meant them to. "
                             "Three changes, in the order you will notice them:"),
        TextBlock::numbered("Waves build over forty seconds instead of fifteen."),
        TextBlock::numbered("A reefed sail now holds in a gale; before, it tore at once."),
        TextBlock::numbered("Repairs at sea cost rope, not planks."),
        TextBlock::quote("We would rather you lose a race to the weather than a boat. The sea "
                         "should be a rival, not a trap."),
        TextBlock::heading("Fixes", 2),
        TextBlock::heading("Crashes", 3),
        TextBlock::bullet("Opening the chart during a cutscene no longer closes the game."),
        TextBlock::bullet("A save made while capsizing loads the right way up."),
        TextBlock::heading("Sound and picture", 3),
        TextBlock::bullet("Gulls no longer follow the camera indoors."),
        TextBlock::bullet("Rain stops under bridges, as rain should."),
        TextBlock::bullet("The compass rose is readable on bright sand."),
        TextBlock::heading("For modders", 2),
        TextBlock::paragraph("Weather tables moved to a plain text format. A storm front is now "
                             "four lines:"),
        TextBlock::code("front \"autumn gale\"\n  wind   38 kn  from west\n  swell  4.5 m\n"
                        "  builds 40 s"),
        TextBlock::divider(),
        TextBlock::heading("Known issues", 2),
        TextBlock::bullet("The night market's music can start a few seconds late."),
        TextBlock::bullet("Very long boat names overflow the harbour ledger."),
        TextBlock::paragraph("Both are fixed in 1.4.1, due next week. Fair winds."),
    };
}

class MediaPage final : public Page
{
  public:
    explicit MediaPage(app::Context &context) : context_(context)
    {
        text_.set_content(patch_notes());
        text_.image = [this](ui::Canvas &canvas, const Rect &box, const ui::TextBlock &block, int)
        {
            const demo::Item &item = context_.catalog[static_cast<std::size_t>(block.tag)];
            const float radius =
                theme_.corner == ui::Corner::round ? std::min(theme_.radius, 12.0f) : 0.0f;
            // The band is cut to the box's own shape, so nothing is squeezed.
            const float share = std::min(box.h / box.w, 1.0f);
            const Rect uv{0.0f, 0.5f + share * 0.5f, 1.0f, -share};
            if (item.cover != 0)
                canvas.list.image(item.cover, box, uv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(box, radius, item.mid, item.dark);
        };

        std::vector<ui::ViewerImage> images;
        for (int i = 0; i < kPictures; ++i)
        {
            const demo::Item &item = context_.catalog[static_cast<std::size_t>(i)];
            ui::ViewerImage image;
            image.title = item.title;
            image.caption = std::string(item.genre) + " \xC2\xB7 " + item.studio;
            image.texture = item.cover;
            image.uv = kBandUv;
            image.aspect = kBandAspect;
            image.pixel_width = static_cast<float>(demo::Catalog::kCoverSize);
            image.top = item.mid;
            image.bottom = item.dark;
            images.push_back(image);
        }
        viewer_.set_images(std::move(images));

        controls_.art = [this](ui::Canvas &canvas, const Rect &box, float radius)
        {
            const demo::Item &item = track_item();
            if (item.cover != 0)
                canvas.list.image(item.cover, box, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(box, radius, item.mid, item.dark);
        };
        controls_.set_volume(0.7f);

        loader_.set_stages({{"Reading save data", 1.0f},
                            {"Compiling shaders", 3.0f},
                            {"Building the harbour", 2.0f},
                            {"Lighting the lanterns", 1.0f}});
        std::vector<std::string> tips;
        for (const char *tip : kTips)
            tips.emplace_back(tip);
        loader_.set_tips(std::move(tips));

        set_track(0);
        playing_ = true;
        restyle(ui::default_theme(), false);
        // The article is wrapped at its first draw unless it is told the
        // fonts earlier; input can arrive before a frame is drawn.
        text_.layout(context_.fonts);
        apply_focus();
    }

    const char *title() const override
    {
        return "Media";
    }
    const char *summary() const override
    {
        return "Articles, pictures, a player's transport and the loading screen";
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
        loader_.hide();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        if (loader_.is_open())
            update_loader(input, dt, feedback);
        else if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, text_.style, text_.style.sounds.change);
        }
        else
        {
            switch (area_)
            {
            case Area::text:
                update_text(input, feedback);
                break;
            case Area::viewer:
                update_viewer(input, feedback);
                break;
            case Area::launch:
                update_launch(input, feedback);
                break;
            case Area::controls:
                update_controls(input, feedback);
                break;
            }
        }

        // The playback the transport reports to: a clock, six tracks.
        if (playing_)
        {
            position_ += dt;
            if (position_ >= duration_)
                set_track((track_ + 1) % kTracks);
        }
        controls_.set_playing(playing_);
        controls_.set_position(position_);
        controls_.set_buffered(std::min(duration_, position_ + 45.0f));

        text_.update(dt);
        viewer_.update(dt);
        controls_.update(dt);
        loader_.update(dt);
        launch_focus_.target = area_ == Area::launch ? 1.0f : 0.0f;
        launch_focus_.update(dt, 18.0f);
        launch_press_.update(dt, 9.0f);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        const Variant &v = kVariants[variant_];
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);
        const auto caption = [&](const char *words, float x, float baseline, Area area)
        {
            const bool active = area_ == area || (area == Area::controls && area_ == Area::launch);
            return paint.label(words, x + (area_ == area ? nudge : 0.0f), baseline, 20.0f,
                               active ? paint.page_text() : paint.page_text_muted());
        };
        caption(v.text_caption, v.text.x, kCaptionY, Area::text);
        caption(v.viewer_caption, v.viewer.x, kCaptionY, Area::viewer);
        const float row_baseline = v.row_y + kRowHeight * 0.5f + 7.0f;
        const float taken = caption(v.controls_caption, v.controls.x, row_baseline, Area::controls);

        text_.draw(canvas);
        viewer_.draw(canvas);
        controls_.draw(canvas);

        const Rect button = launch_rect();
        ui::Look look;
        look.focus = launch_focus_.value;
        look.press = launch_press_.value;
        paint.button(button, "Loading screen", ui::ButtonKind::secondary, look);
        // What the focused component last reported, between caption and button.
        const float room = button.x - 28.0f - (v.controls.x + taken + 28.0f);
        if (!status_.empty() && room > 120.0f)
            paint.body(ui::fit_body(paint, status_, 20.0f, room), button.x - 28.0f, row_baseline,
                       20.0f, paint.page_text_muted(), gfx::Align::right);
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        loader_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (loader_.is_open())
        {
            if (loader_.ready())
                return kReadyHints;
            return kLoadingHints;
        }
        switch (area_)
        {
        case Area::text:
            return kTextHints;
        case Area::viewer:
            if (viewer_.zoomed())
                return kZoomHints;
            return kViewerHints;
        case Area::launch:
            return kLaunchHints;
        case Area::controls:
            break;
        }
        if (controls_.adjusting())
            return kVolumeHints;
        if (controls_.focus() == ui::MediaControl::scrubber)
            return kScrubHints;
        return kControlsHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    const demo::Item &track_item() const
    {
        // The tracks are the catalogue read from its far end, so the player
        // and the viewer do not show the same covers.
        return context_.catalog[context_.catalog.size() - 1 - static_cast<std::size_t>(track_)];
    }

    Rect launch_rect() const
    {
        const Variant &v = kVariants[variant_];
        return {v.viewer.x + v.viewer.w - kLaunchWidth, v.row_y, kLaunchWidth, kRowHeight};
    }

    void set_track(int index)
    {
        track_ = (index + kTracks) % kTracks;
        const demo::Item &item = track_item();
        duration_ = 168.0f + 23.0f * static_cast<float>((track_ * 5) % 7);
        position_ = 0.0f;
        controls_.title = item.title;
        controls_.artist = item.studio;
        controls_.set_duration(duration_);
        controls_.set_position(0.0f, true);
        controls_.set_buffered(45.0f);
        controls_.set_chapters({{0.0f, "Opening"},
                                {duration_ * 0.22f, "First light"},
                                {duration_ * 0.55f, "Open water"},
                                {duration_ * 0.82f, "Landfall"}});
    }

    // Every style starts from its defaults: a variant is a whole set of
    // knobs, and nothing of the last one may linger. The components keep
    // their scroll, picture, zoom and focus through it.
    void apply_variant()
    {
        const Variant &v = kVariants[variant_];
        ui::TextViewStyle text;
        ui::ImageViewerStyle viewer;
        ui::MediaControlsStyle controls;
        ui::LoadingScreenStyle loader;
        text.theme = viewer.theme = controls.theme = loader.theme = theme_;
        text.reduced_motion = viewer.reduced_motion = reduced_;
        controls.reduced_motion = loader.reduced_motion = reduced_;

        // What the page needs whatever the variant: the gallery owns L2 / R2,
        // and the edges that lead to a neighbour hand the focus on.
        viewer.trigger_zoom = false;
        viewer.exits.left = viewer.exits.up = viewer.exits.down = true;
        controls.exits.up = controls.exits.left = true;
        viewer.thumb_size = 52.0f;
        // The gallery draws its hint row over a modal layer: keep clear of it.
        loader.bottom = 152.0f;
        loader_.art = nullptr;

        switch (variant_)
        {
        case 1:
            text.toc = true;
            text.body_size = 23.0f;
            text.toc_width = 210.0f;
            viewer.filmstrip = false;
            viewer.wrap = true;
            viewer.minimap_size = 200.0f;
            controls.layout = ui::MediaLayout::compact;
            controls.play_size = controls.button_size;
            controls.show_skip = false;
            controls.title_size = 24.0f;
            loader.layout = ui::LoadingLayout::center;
            loader.indicator = ui::LoadingIndicator::bar;
            break;
        case 2:
            text.panel = false;
            text.footer = false;
            text.heading_rule = false;
            text.max_width = 520.0f;
            text.body_size = 26.0f;
            text.padding = 8.0f;
            viewer.frame = false;
            viewer.thumb_size = 70.0f;
            viewer.arrows = false;
            controls.buttons = ui::MediaButtons::ghost;
            controls.panel = false;
            controls.show_art = false;
            controls.show_shuffle = false;
            controls.show_repeat = false;
            controls.remaining = false;
            controls.auto_hide = 6.0f;
            controls.padding = 8.0f;
            loader.indicator = ui::LoadingIndicator::spinner;
            loader.spinner = ui::SpinnerKind::dots;
            loader.spinner_size = 36.0f;
            break;
        default:
            // The cover on show becomes the loading screen's artwork.
            loader_.art = [this](ui::Canvas &canvas, const Rect &screen, float)
            {
                const demo::Item &item =
                    context_.catalog[static_cast<std::size_t>(viewer_.index())];
                if (item.cover != 0)
                    canvas.list.image(item.cover, screen, kWideUv, Color::rgb(0xffffff), 0.0f);
                else
                    canvas.list.gradient_rect(screen, 0.0f, item.mid, item.dark);
            };
            break;
        }
        text_.style = text;
        viewer_.style = viewer;
        controls_.style = controls;
        loader_.style = loader;
        text_.set_bounds(v.text);
        viewer_.set_bounds(v.viewer);
        controls_.set_bounds(v.controls);
        controls_.wake();
    }

    void apply_focus()
    {
        text_.set_active(area_ == Area::text);
        viewer_.set_active(area_ == Area::viewer);
        controls_.set_active(area_ == Area::controls);
    }

    void focus(Area area, ui::Feedback &feedback)
    {
        area_ = area;
        if (area != Area::text)
            right_area_ = area;
        // Controls that hid themselves come back when the focus arrives.
        if (area == Area::controls)
            controls_.wake();
        apply_focus();
        const Variant &v = kVariants[variant_];
        const float x = area == Area::text ? v.text.cx() : v.viewer.cx();
        ui::play_cue(feedback, text_.style, text_.style.sounds.move, x);
    }

    void report(const char *who, ui::Event event, const std::string &detail = {})
    {
        if (event == ui::Event::none)
            return;
        status_ = std::string(who) + " \xC2\xB7 Event::" + event_name(event);
        if (!detail.empty())
            status_ += " \xC2\xB7 " + detail;
    }

    void update_text(const InputFrame &input, ui::Feedback &feedback)
    {
        const Variant &v = kVariants[variant_];
        if (input.nav == Direction::left)
        {
            ui::refuse(feedback, text_.style, input, edge_, v.text.cx());
        }
        else if (input.nav == Direction::right)
        {
            focus(right_area_, feedback);
        }
        else
        {
            const ui::Event event = input.is_pressed(Action::north)
                                        ? text_.next_heading(1, feedback)
                                        : text_.handle(input, feedback);
            char detail[24];
            std::snprintf(
                detail, sizeof(detail), "%d%% read",
                static_cast<int>(text_.scroll() / std::max(text_.scroll_limit(), 1.0f) * 100.0f +
                                 0.5f));
            report("TextView", event, detail);
        }
    }

    void update_viewer(const InputFrame &input, ui::Feedback &feedback)
    {
        const Variant &v = kVariants[variant_];
        const ui::Event event = input.is_pressed(Action::north) ? viewer_.cycle_zoom(feedback)
                                                                : viewer_.handle(input, feedback);
        if (!input.is_pressed(Action::north))
        {
            switch (viewer_.exit())
            {
            case Direction::left:
                focus(Area::text, feedback);
                return;
            case Direction::down:
                focus(Area::launch, feedback);
                return;
            case Direction::up:
                ui::refuse(feedback, viewer_.style, input, edge_, v.viewer.cx());
                return;
            default:
                break;
            }
        }
        char detail[48];
        std::snprintf(detail, sizeof(detail), "picture %d, %d%%", viewer_.index() + 1,
                      viewer_.percent());
        report("ImageViewer", event, detail);
    }

    void update_launch(const InputFrame &input, ui::Feedback &feedback)
    {
        const Rect button = launch_rect();
        if (input.is_pressed(Action::confirm))
        {
            launch_press_.trigger();
            const demo::Item &item = context_.catalog[static_cast<std::size_t>(viewer_.index())];
            loader_.title = item.title;
            loader_.subtitle = std::string(item.genre) + " \xC2\xB7 " + item.studio;
            load_clock_ = 0.0f;
            loader_.show(feedback);
            status_ = "LoadingScreen \xC2\xB7 show()";
            return;
        }
        switch (input.nav)
        {
        case Direction::up:
            focus(Area::viewer, feedback);
            break;
        case Direction::down:
            focus(Area::controls, feedback);
            break;
        case Direction::left:
            focus(Area::text, feedback);
            break;
        case Direction::right:
            ui::refuse(feedback, text_.style, input, edge_, button.cx());
            break;
        case Direction::none:
            break;
        }
    }

    void update_controls(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = controls_.handle(input, feedback);
        if (controls_.exit() == Direction::up)
        {
            focus(Area::launch, feedback);
            return;
        }
        if (controls_.exit() == Direction::left)
        {
            focus(Area::text, feedback);
            return;
        }
        // The transport asks; the player (this page's clock) acts.
        switch (controls_.command())
        {
        case ui::MediaCommand::play:
            playing_ = true;
            break;
        case ui::MediaCommand::pause:
            playing_ = false;
            break;
        case ui::MediaCommand::next:
            set_track(track_ + 1);
            break;
        case ui::MediaCommand::previous:
            // Like every player: back to the start first, then a track back.
            if (position_ > 3.0f)
                position_ = 0.0f;
            else
                set_track(track_ - 1);
            break;
        case ui::MediaCommand::seek:
        case ui::MediaCommand::rewind:
        case ui::MediaCommand::forward:
            position_ = controls_.seek_position();
            break;
        default:
            break;
        }
        if (event == ui::Event::changed || event == ui::Event::activated)
            report("MediaControls", event,
                   std::string("command ") + command_name(controls_.command()));
        else
            report("MediaControls", event);
    }

    void update_loader(const InputFrame &input, float dt, ui::Feedback &feedback)
    {
        // The load the screen shows. It is fake, but the screen is told the
        // truth about it: progress as it goes, ready only when it is done.
        load_clock_ += dt;
        if (!loader_.ready())
        {
            loader_.set_progress(load_curve(load_clock_));
            if (load_clock_ >= kLoadSeconds)
                loader_.set_ready(true);
        }
        const ui::Event event = loader_.handle(input, feedback);
        if (event == ui::Event::cancelled)
            loader_.hide();
        report("LoadingScreen", event);
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::TextView text_;
    ui::ImageViewer viewer_;
    ui::MediaControls controls_;
    ui::LoadingScreen loader_;
    Area area_ = Area::text;
    Area right_area_ = Area::viewer; // where "right" from the article returns to
    int variant_ = 0;
    float clock_ = 0.0f;
    // The player.
    int track_ = 0;
    bool playing_ = true;
    float position_ = 0.0f;
    float duration_ = 0.0f;
    // The load.
    float load_clock_ = 0.0f;
    tween::Spring launch_focus_;
    ui::Pulse launch_press_;
    ui::Pulse edge_;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_media_page(app::Context &context)
{
    return std::make_unique<MediaPage>(context);
}

} // namespace hui::concepts::gallery
