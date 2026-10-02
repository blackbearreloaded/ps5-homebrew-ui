// ps5-homebrew-ui - Tests: TextView, ImageViewer, MediaControls and LoadingScreen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/image_viewer.hpp"
#include "ui/components/loading_screen.hpp"
#include "ui/components/media_controls.hpp"
#include "ui/components/text_view.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::InputFrame;
using hui::audio::Cue;
using hui::ui::Event;
using hui::ui::ImageViewer;
using hui::ui::LoadingScreen;
using hui::ui::MediaCommand;
using hui::ui::MediaControl;
using hui::ui::MediaControls;
using hui::ui::TextBlock;
using hui::ui::TextView;
using hui::ui::ZoomMode;

class ComponentsMedia : public hui::testing::ComponentFixture
{
  protected:
    ComponentsMedia()
    {
        std::vector<TextBlock> blocks;
        for (int section = 0; section < 4; ++section)
        {
            blocks.push_back(
                TextBlock::heading("Section " + std::to_string(section + 1), section == 0 ? 1 : 2));
            blocks.push_back(TextBlock::paragraph(
                "A paragraph long enough to wrap over several lines in a narrow column, so the "
                "article is taller than the view it scrolls in and the ends can be reached."));
            blocks.push_back(TextBlock::bullet("A list item"));
            blocks.push_back(TextBlock::numbered("A numbered item"));
            blocks.push_back(TextBlock::quote("Something somebody said"));
            blocks.push_back(TextBlock::code("key = value\n  indented = yes"));
            blocks.push_back(TextBlock::key_value("Version", "1.4.0"));
            blocks.push_back(TextBlock::divider());
            blocks.push_back(TextBlock::image("A picture", 120.0f));
        }
        text_.set_content(blocks);
        text_.set_bounds({100.0f, 100.0f, 520.0f, 420.0f});
        text_.layout(fonts_);

        std::vector<hui::ui::ViewerImage> images(5);
        for (std::size_t i = 0; i < images.size(); ++i)
        {
            images[i].title = "Picture " + std::to_string(i + 1);
            images[i].aspect = 2.0f;
            images[i].pixel_width = 512.0f;
        }
        viewer_.set_images(images);
        viewer_.set_bounds({100.0f, 100.0f, 1000.0f, 400.0f});

        controls_.title = "Track";
        controls_.artist = "Artist";
        controls_.set_bounds({100.0f, 600.0f, 1000.0f, 236.0f});
        controls_.set_duration(200.0f);
        controls_.set_chapters({{0.0f, "Opening"}, {60.0f, "Middle"}, {150.0f, "End"}});

        loader_.title = "Loading";
        loader_.set_stages({{"First", 1.0f}, {"Second", 3.0f}});
        loader_.set_tips({"Tip one.", "Tip two.", "Tip three."});
    }

    template <typename Component> Event send(Component &component, const InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        settle(component);
        return event;
    }
    // Half a second of animation without input.
    template <typename Component> void settle(Component &component, int frames = 30)
    {
        for (int i = 0; i < frames; ++i)
            component.update(kFrame);
    }
    static InputFrame repeat(Direction direction)
    {
        InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }

    TextView text_;
    ImageViewer viewer_;
    MediaControls controls_;
    LoadingScreen loader_;
};

TEST_F(ComponentsMedia, TextViewScrollsByStepsAndByTheRightStick)
{
    ASSERT_GT(text_.scroll_limit(), 400.0f);
    EXPECT_EQ(send(text_, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_FLOAT_EQ(text_.scroll(), text_.style.step);
    // Left and right are not the article's.
    EXPECT_EQ(send(text_, nav(Direction::right)), Event::none);

    // The stick scrolls without events, and faster the longer it is held.
    InputFrame stick = idle();
    stick.stick2_y = 1.0f;
    const float before = text_.scroll();
    text_.handle(stick, feedback_);
    text_.update(kFrame);
    const float first = text_.scroll() - before;
    EXPECT_GT(first, 0.0f);
    for (int i = 0; i < 80; ++i)
    {
        EXPECT_EQ(text_.handle(stick, feedback_), Event::none);
        text_.update(kFrame);
    }
    const float held = text_.scroll();
    text_.handle(stick, feedback_);
    text_.update(kFrame);
    EXPECT_GT(text_.scroll() - held, first * 1.5f);
}

TEST_F(ComponentsMedia, TextViewEndsRefuseOnceAndStaySilentOnRepeat)
{
    EXPECT_EQ(send(text_, nav(Direction::up)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(text_, repeat(Direction::up)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    text_.scroll_to(text_.scroll_limit(), true);
    EXPECT_FLOAT_EQ(text_.progress(), 1.0f);
    EXPECT_EQ(send(text_, nav(Direction::down)), Event::refused);
    feedback_.clear();
    EXPECT_EQ(text_.page(1, feedback_), Event::refused);

    // An edge that leads somewhere hands the focus on instead.
    text_.style.exits.down = true;
    EXPECT_EQ(send(text_, nav(Direction::down)), Event::none);
    EXPECT_EQ(text_.exit(), Direction::down);
}

TEST_F(ComponentsMedia, TextViewPagesAndJumpsBetweenHeadings)
{
    ASSERT_EQ(text_.headings().size(), 4u);
    EXPECT_EQ(text_.current_heading(), text_.headings()[0]);
    feedback_.clear();
    EXPECT_EQ(text_.next_heading(1, feedback_), Event::moved);
    EXPECT_TRUE(asked(Cue::tab));
    settle(text_, 90);
    EXPECT_EQ(text_.current_heading(), text_.headings()[1]);
    EXPECT_GT(text_.progress(), 0.0f);
    EXPECT_LT(text_.progress(), 1.0f);

    const float before = text_.scroll();
    EXPECT_EQ(text_.page(1, feedback_), Event::moved);
    EXPECT_GT(text_.scroll(), before + 100.0f);
    EXPECT_EQ(text_.next_heading(-1, feedback_), Event::moved);
    EXPECT_LE(text_.scroll(), before + 100.0f);
}

TEST_F(ComponentsMedia, TextViewKeepsItsPlaceWhenTheThemeReWrapsIt)
{
    const int block = text_.headings()[2];
    text_.scroll_to_block(block, true);
    settle(text_);
    const float height = text_.content_height();
    const float before = text_.scroll();
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        if (std::string(theme.id) == "pixel")
            text_.style.theme = theme;
    }
    settle(text_);
    // Another face, other line breaks: the same block is still at the top.
    EXPECT_NE(text_.content_height(), height);
    EXPECT_NE(text_.scroll(), before);
    EXPECT_NEAR(text_.scroll(), text_.block_offset(block), 1.0f);
    EXPECT_EQ(text_.current_heading(), block);
}

TEST_F(ComponentsMedia, ImageViewerBrowsesAndItsEndsRefuseOrHandOn)
{
    EXPECT_EQ(send(viewer_, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(viewer_, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(viewer_.index(), 1);
    // At fit there is nothing to pan: up and down are not the viewer's.
    EXPECT_EQ(send(viewer_, nav(Direction::up)), Event::none);
    EXPECT_EQ(viewer_.exit(), Direction::none);

    viewer_.set_index(0);
    viewer_.style.exits.left = true;
    EXPECT_EQ(send(viewer_, nav(Direction::left)), Event::none);
    EXPECT_EQ(viewer_.exit(), Direction::left);

    viewer_.style.wrap = true;
    EXPECT_EQ(send(viewer_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(viewer_.index(), 4);
}

TEST_F(ComponentsMedia, ImageViewerCyclesFitFillActualAndZoomsByTrigger)
{
    EXPECT_EQ(viewer_.mode(), ZoomMode::fit);
    EXPECT_FALSE(viewer_.zoomed());
    EXPECT_EQ(send(viewer_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(viewer_.mode(), ZoomMode::fill);
    EXPECT_TRUE(viewer_.zoomed());
    // Filled, the picture covers the whole stage.
    const hui::gfx::Rect stage = viewer_.stage();
    const hui::gfx::Rect picture = viewer_.picture_rect();
    EXPECT_LE(picture.x, stage.x + 4.0f);
    EXPECT_GE(picture.x + picture.w, stage.x + stage.w - 4.0f);
    EXPECT_LE(picture.y, stage.y + 4.0f);

    EXPECT_EQ(send(viewer_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(viewer_.mode(), ZoomMode::actual);
    EXPECT_EQ(viewer_.percent(), 100);
    EXPECT_EQ(send(viewer_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(viewer_.mode(), ZoomMode::fit);

    // The right trigger zooms in up to the limit; back returns to fit first.
    InputFrame pull = idle();
    pull.trigger_r = 1.0f;
    for (int i = 0; i < 300; ++i)
    {
        viewer_.handle(pull, feedback_);
        viewer_.update(kFrame);
    }
    EXPECT_EQ(viewer_.mode(), ZoomMode::free);
    EXPECT_GE(viewer_.zoom(), viewer_.style.max_zoom - 0.01f);
    feedback_.clear();
    EXPECT_EQ(viewer_.zoom_in(feedback_), Event::refused);
    EXPECT_EQ(send(viewer_, press(Action::back)), Event::changed);
    EXPECT_EQ(viewer_.mode(), ZoomMode::fit);
    EXPECT_EQ(send(viewer_, press(Action::back)), Event::cancelled);
}

TEST_F(ComponentsMedia, ImageViewerPansInsideThePictureAndSpringsBack)
{
    viewer_.set_zoom(3.0f, true);
    EXPECT_FLOAT_EQ(viewer_.pan_x(), 0.5f);
    // Hold the stick right for long enough to run into the edge and past it.
    InputFrame push = idle();
    push.stick_x = 1.0f;
    for (int i = 0; i < 240; ++i)
    {
        viewer_.handle(push, feedback_);
        viewer_.update(kFrame);
    }
    const float pulled = viewer_.pan_x();
    EXPECT_GT(pulled, 0.5f);
    settle(viewer_, 60); // released: the rubber band lets go
    EXPECT_LT(viewer_.pan_x(), pulled);
    const hui::gfx::Rect stage = viewer_.stage();
    const hui::gfx::Rect picture = viewer_.picture_rect();
    EXPECT_NEAR(picture.x + picture.w, stage.x + stage.w, 6.0f);

    // The D-pad pans in steps and the edge refuses.
    EXPECT_EQ(send(viewer_, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(viewer_, nav(Direction::left)), Event::changed);
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_EQ(viewer_.index(), 0); // panned, not browsed
    // A step the tracker derived from the stick is not a D-pad press.
    InputFrame derived = nav(Direction::left);
    derived.nav_from_stick = true;
    const float before = viewer_.pan_x();
    EXPECT_EQ(send(viewer_, derived), Event::none);
    EXPECT_FLOAT_EQ(viewer_.pan_x(), before);
}

TEST_F(ComponentsMedia, MediaControlsReportsCommandsAndLeavesPlaybackToTheScreen)
{
    EXPECT_EQ(controls_.focus(), MediaControl::play);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(controls_.command(), MediaCommand::play);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_FALSE(controls_.playing()); // the screen decides
    controls_.set_playing(true);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(controls_.command(), MediaCommand::pause);

    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(controls_.command(), MediaCommand::none);
    EXPECT_EQ(controls_.focus(), MediaControl::forward);
    controls_.set_position(100.0f);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(controls_.command(), MediaCommand::forward);
    EXPECT_FLOAT_EQ(controls_.seek_position(), 110.0f);
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(controls_.command(), MediaCommand::next);

    // Past the last button the row refuses, or hands the focus on.
    controls_.set_focus(MediaControl::volume);
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::refused);
    controls_.style.exits.right = true;
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::none);
    EXPECT_EQ(controls_.exit(), Direction::right);
}

TEST_F(ComponentsMedia, MediaControlsScrubbingAcceleratesOnHold)
{
    EXPECT_EQ(send(controls_, nav(Direction::up)), Event::moved);
    EXPECT_EQ(controls_.focus(), MediaControl::scrubber);
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::changed);
    EXPECT_EQ(controls_.command(), MediaCommand::seek);
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_FLOAT_EQ(controls_.seek_position(), controls_.style.seek_step);
    EXPECT_EQ(controls_.chapter_at(controls_.position()), 0);

    float last = controls_.position();
    float step = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_EQ(send(controls_, repeat(Direction::right)), Event::changed);
        EXPECT_GT(controls_.position() - last, step); // each repeat goes further
        step = controls_.position() - last;
        last = controls_.position();
    }
    // A fresh press is a single step again.
    EXPECT_EQ(send(controls_, nav(Direction::left)), Event::changed);
    EXPECT_FLOAT_EQ(last - controls_.position(), controls_.style.seek_step);

    controls_.set_position(0.0f);
    EXPECT_EQ(send(controls_, nav(Direction::left)), Event::refused);
    EXPECT_EQ(controls_.command(), MediaCommand::none);
    // Confirm on the scrubber is play / pause; down returns to the buttons.
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(controls_.command(), MediaCommand::play);
    EXPECT_EQ(send(controls_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(controls_.focus(), MediaControl::play);
}

TEST_F(ComponentsMedia, MediaControlsTogglesAndVolume)
{
    controls_.set_focus(MediaControl::shuffle);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(controls_.command(), MediaCommand::shuffle);
    EXPECT_TRUE(controls_.shuffle());
    EXPECT_TRUE(asked(Cue::toggle));

    controls_.set_focus(MediaControl::repeat);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(controls_.repeat(), hui::ui::MediaRepeat::all);
    send(controls_, press(Action::confirm));
    EXPECT_EQ(controls_.repeat(), hui::ui::MediaRepeat::one);
    send(controls_, press(Action::confirm));
    EXPECT_EQ(controls_.repeat(), hui::ui::MediaRepeat::off);

    // Confirm on the volume captures left and right until confirm or back.
    controls_.set_focus(MediaControl::volume);
    controls_.set_volume(0.5f);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::none);
    EXPECT_TRUE(controls_.adjusting());
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::changed);
    EXPECT_EQ(controls_.command(), MediaCommand::volume);
    EXPECT_FLOAT_EQ(controls_.volume(), 0.5f + controls_.style.volume_step);
    EXPECT_EQ(controls_.focus(), MediaControl::volume);
    controls_.set_volume(1.0f);
    EXPECT_EQ(send(controls_, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(controls_, press(Action::back)), Event::none);
    EXPECT_FALSE(controls_.adjusting());

    feedback_.clear();
    EXPECT_EQ(controls_.toggle_mute(feedback_), Event::changed);
    EXPECT_TRUE(controls_.muted());

    // A control the style hides cannot hold the focus.
    controls_.style.show_volume = false;
    EXPECT_EQ(send(controls_, idle()), Event::none);
    EXPECT_EQ(controls_.focus(), MediaControl::play);
}

TEST_F(ComponentsMedia, MediaControlsHideWhenIdleAndTheFirstPressOnlyWakesThem)
{
    controls_.style.auto_hide = 1.0f;
    settle(controls_, 120);
    EXPECT_FALSE(controls_.hidden()); // paused: they stay
    controls_.set_playing(true);
    settle(controls_, 200);
    EXPECT_TRUE(controls_.hidden());
    EXPECT_LT(controls_.visibility(), 0.05f);

    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::none);
    EXPECT_EQ(controls_.command(), MediaCommand::none);
    EXPECT_FALSE(controls_.hidden());
    EXPECT_GT(controls_.visibility(), 0.9f);
    EXPECT_EQ(send(controls_, press(Action::confirm)), Event::activated);
}

TEST_F(ComponentsMedia, LoadingScreenStagesPromptAndTips)
{
    EXPECT_FALSE(loader_.visible());
    EXPECT_EQ(send(loader_, press(Action::confirm)), Event::none); // closed: not its input
    feedback_.clear();
    loader_.show(feedback_);
    EXPECT_TRUE(asked(Cue::modal_open));
    settle(loader_);
    EXPECT_TRUE(loader_.is_open());
    EXPECT_FLOAT_EQ(loader_.opacity(), 1.0f);

    // Stages share the bar by weight: 1 and 3.
    EXPECT_EQ(loader_.stage(), 0);
    loader_.set_progress(0.2f);
    EXPECT_EQ(loader_.stage(), 0);
    loader_.set_progress(0.3f);
    EXPECT_EQ(loader_.stage(), 1);
    loader_.set_stage(1, 0.5f);
    EXPECT_FLOAT_EQ(loader_.progress(), 0.625f);

    EXPECT_EQ(send(loader_, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_TRUE(loader_.is_open());

    loader_.set_ready(true);
    EXPECT_FLOAT_EQ(loader_.progress(), 1.0f);
    EXPECT_EQ(send(loader_, idle()), Event::none);
    EXPECT_TRUE(asked(Cue::notify)); // the chime, once
    EXPECT_EQ(send(loader_, idle()), Event::none);
    EXPECT_FALSE(asked(Cue::notify));
    EXPECT_EQ(send(loader_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_FALSE(loader_.is_open());
    settle(loader_, 60);
    EXPECT_FALSE(loader_.visible());

    // Tips rotate on a timer and step by hand.
    loader_.set_tips({"Tip one.", "Tip two.", "Tip three."});
    loader_.style.tip_seconds = 1.0f;
    loader_.show(feedback_);
    EXPECT_EQ(loader_.tip(), 0);
    EXPECT_EQ(send(loader_, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::tab));
    EXPECT_EQ(loader_.tip(), 1);
    EXPECT_EQ(send(loader_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(send(loader_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(loader_.tip(), 2); // a ring
    settle(loader_, 45);         // 0.5 s + 0.75 s since the last step
    EXPECT_EQ(loader_.tip(), 0);
    EXPECT_EQ(send(loader_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(loader_.is_open()); // the screen decides what cancel means
}

TEST_F(ComponentsMedia, DrawsInEveryThemeAndVariant)
{
    feedback_.clear();
    loader_.show(feedback_);
    controls_.set_position(70.0f);
    controls_.set_buffered(120.0f);
    int variant = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        const auto draw_one = [&](const auto &component)
        {
            hui::ui::Canvas target = canvas();
            component.draw(target);
            expect_drawn(theme.id);
        };
        for (int pass = 0; pass < 3; ++pass, ++variant)
        {
            text_.style.theme = theme;
            text_.style.panel = pass != 1;
            text_.style.toc = pass == 2;
            text_.style.footer = pass != 1;
            text_.scroll_to(text_.scroll_limit() * 0.4f, true);
            text_.update(kFrame);
            draw_one(text_);

            viewer_.style.theme = theme;
            viewer_.style.frame = pass != 1;
            viewer_.style.filmstrip = pass != 2;
            viewer_.set_bounds(viewer_.bounds());
            viewer_.set_mode(pass == 0 ? ZoomMode::fit : ZoomMode::fill, true);
            viewer_.update(kFrame);
            draw_one(viewer_);

            controls_.style.theme = theme;
            controls_.style.layout =
                pass == 1 ? hui::ui::MediaLayout::compact : hui::ui::MediaLayout::full;
            controls_.style.buttons =
                pass == 2 ? hui::ui::MediaButtons::ghost : hui::ui::MediaButtons::surface;
            controls_.style.panel = pass != 2;
            controls_.set_focus(pass == 0 ? MediaControl::scrubber : MediaControl::play);
            controls_.set_playing(variant % 2 == 0);
            controls_.update(kFrame);
            draw_one(controls_);

            loader_.style.theme = theme;
            loader_.style.layout =
                pass == 1 ? hui::ui::LoadingLayout::center : hui::ui::LoadingLayout::corner;
            loader_.style.indicator =
                pass == 2 ? hui::ui::LoadingIndicator::spinner : hui::ui::LoadingIndicator::both;
            loader_.set_progress(pass == 1 ? -1.0f : 0.5f);
            loader_.set_ready(pass == 2);
            loader_.update(0.5f);
            draw_one(loader_);
        }
    }
}

TEST_F(ComponentsMedia, SlotsDrawWhatTheComponentsLeaveOpen)
{
    int article = 0;
    int pictures = 0;
    int artwork = 0;
    int backdrop = 0;
    text_.image =
        [&](hui::ui::Canvas &target, const hui::gfx::Rect &box, const TextBlock &block, int)
    {
        EXPECT_EQ(block.kind, hui::ui::TextBlockKind::image);
        target.list.rounded_rect(box, 0.0f, hui::gfx::Color::rgb(0x336699));
        ++article;
    };
    viewer_.picture = [&](hui::ui::Canvas &target, const hui::gfx::Rect &where,
                          const hui::ui::ViewerImage &, int, float alpha)
    {
        target.list.rounded_rect(where, 0.0f, hui::gfx::Color::rgb(0x336699, alpha));
        ++pictures;
    };
    controls_.art = [&](hui::ui::Canvas &target, const hui::gfx::Rect &box, float radius)
    {
        target.list.rounded_rect(box, radius, hui::gfx::Color::rgb(0x336699));
        ++artwork;
    };
    loader_.art = [&](hui::ui::Canvas &target, const hui::gfx::Rect &screen, float progress)
    {
        EXPECT_GE(progress, 0.0f);
        target.list.rounded_rect(screen, 0.0f, hui::gfx::Color::rgb(0x336699));
        ++backdrop;
    };
    feedback_.clear();
    loader_.show(feedback_);
    loader_.update(0.5f);
    text_.scroll_to_block(8, true); // the first section's image block
    viewer_.set_zoom(2.0f, true);   // cropped: the minimap draws the picture as well
    viewer_.update(0.5f);

    {
        hui::ui::Canvas target = canvas();
        text_.draw(target);
        expect_drawn("text");
    }
    {
        hui::ui::Canvas target = canvas();
        viewer_.draw(target);
        expect_drawn("viewer");
    }
    {
        hui::ui::Canvas target = canvas();
        controls_.draw(target);
        expect_drawn("controls");
    }
    {
        hui::ui::Canvas target = canvas();
        loader_.draw(target);
        expect_drawn("loader");
    }
    EXPECT_GE(article, 1);  // the image block in view
    EXPECT_GE(pictures, 7); // the stage, the minimap and five thumbnails
    EXPECT_EQ(artwork, 1);
    EXPECT_EQ(backdrop, 1);
}

} // namespace
