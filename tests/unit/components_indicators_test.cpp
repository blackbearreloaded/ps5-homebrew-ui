// ps5-homebrew-ui - Tests: the indicator components (progress, badges, rating, counter, ...).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/badge.hpp"
#include "ui/components/counter.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/rating.hpp"
#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::ui::Event;
namespace ui = hui::ui;

class ComponentsIndicators : public hui::testing::ComponentFixture
{
  protected:
    // Lets a component animate for `seconds`.
    template <typename Component> static void run(Component &component, float seconds)
    {
        const int frames = static_cast<int>(seconds / kFrame);
        for (int i = 0; i < frames; ++i)
            component.update(kFrame);
    }

    Event send(ui::Rating &rating, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = rating.handle(input, feedback_);
        run(rating, 0.5f);
        return event;
    }

    // The rectangles of the last draw, to compare two poses.
    std::vector<float> pose() const
    {
        std::vector<float> out;
        for (const hui::gfx::Instance &instance : list_.instances())
            out.insert(out.end(), std::begin(instance.rect), std::end(instance.rect));
        return out;
    }
};

TEST_F(ComponentsIndicators, ProgressEasesToItsValueAndStaysInRange)
{
    ui::ProgressBar bar;
    bar.set_value(0.8f);
    EXPECT_FLOAT_EQ(bar.value(), 0.8f);
    EXPECT_FLOAT_EQ(bar.shown(), 0.0f); // it travels, it does not jump
    float highest = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        bar.update(kFrame);
        highest = std::max(highest, bar.shown());
    }
    EXPECT_NEAR(bar.shown(), 0.8f, 0.001f);
    EXPECT_LE(highest, 0.81f);

    bar.set_value(7.0f);
    EXPECT_FLOAT_EQ(bar.value(), 1.0f);
    bar.set_value(-1.0f, true);
    EXPECT_FLOAT_EQ(bar.shown(), 0.0f);

    ui::ProgressRing ring;
    ring.set_value(0.5f, true);
    ring.set_buffer(2.0f);
    run(ring, 2.0f);
    EXPECT_FLOAT_EQ(ring.shown(), 0.5f);
}

TEST_F(ComponentsIndicators, MeterNamesItsZoneAndHoldsThePeak)
{
    ui::Meter meter;
    EXPECT_EQ(meter.zone(0.2f), ui::Status::success);
    EXPECT_EQ(meter.zone(0.7f), ui::Status::warning);
    EXPECT_EQ(meter.zone(0.9f), ui::Status::danger);

    meter.set_value(0.9f, true);
    meter.set_value(0.2f);
    run(meter, 0.8f);
    EXPECT_NEAR(meter.shown(), 0.2f, 0.02f);
    EXPECT_NEAR(meter.peak(), 0.9f, 0.001f); // still held
    run(meter, 3.0f);
    EXPECT_NEAR(meter.peak(), meter.shown(), 0.001f); // then it fell

    meter.style.peak_hold = false;
    meter.style.shape = ui::MeterShape::radial;
    ui::Canvas target = canvas();
    meter.draw(target);
    expect_drawn("radial meter");
}

TEST_F(ComponentsIndicators, SpinnerStandsStillWithReducedMotion)
{
    for (ui::SpinnerKind kind : {ui::SpinnerKind::arc, ui::SpinnerKind::dots, ui::SpinnerKind::bars,
                                 ui::SpinnerKind::orbit})
    {
        ui::Spinner spinner;
        spinner.style.kind = kind;
        spinner.style.reduced_motion = true;
        spinner.update(0.1f);
        ui::Canvas first = canvas();
        spinner.draw(first);
        const std::vector<float> before = pose();
        spinner.update(0.23f);
        ui::Canvas second = canvas();
        spinner.draw(second);
        EXPECT_EQ(before, pose()) << "a reduced-motion spinner must only pulse";
    }

    ui::Spinner dots;
    dots.style.kind = ui::SpinnerKind::dots;
    dots.update(0.1f);
    ui::Canvas first = canvas();
    dots.draw(first);
    const std::vector<float> before = pose();
    dots.update(0.23f);
    ui::Canvas second = canvas();
    dots.draw(second);
    EXPECT_NE(before, pose());

    dots.set_spinning(false, true);
    ui::Canvas third = canvas();
    dots.draw(third);
    EXPECT_TRUE(list_.empty());
}

TEST_F(ComponentsIndicators, BadgeShowsCountsWordsAndNothing)
{
    ui::Badge badge;
    EXPECT_FALSE(badge.visible()); // a count of zero hides it
    badge.set_count(7);
    EXPECT_TRUE(badge.visible());
    EXPECT_EQ(badge.text(), "7");
    badge.set_count(250);
    EXPECT_EQ(badge.text(), "99+");
    badge.style.max_count = 999;
    EXPECT_EQ(badge.text(), "250");
    badge.set_text("NEW");
    EXPECT_EQ(badge.text(), "NEW");
    badge.set_text("");
    badge.set_count(0);
    EXPECT_FALSE(badge.visible());
    badge.style.hide_zero = false;
    EXPECT_TRUE(badge.visible());

    // A wider text makes a wider pill; a dot ignores the text.
    ui::Painter paint(list_, fonts_, badge.style.theme);
    badge.set_count(5);
    const float narrow = badge.width(paint);
    badge.set_text("UPDATED");
    EXPECT_GT(badge.width(paint), narrow);
    badge.style.dot = true;
    EXPECT_FLOAT_EQ(badge.width(paint), badge.style.dot_size);
}

TEST_F(ComponentsIndicators, AvatarDerivesInitialsAndAStableColour)
{
    EXPECT_EQ(ui::Avatar::initials_of("mara voss"), "MV");
    EXPECT_EQ(ui::Avatar::initials_of("Ada  de  Brandt"), "AB");
    EXPECT_EQ(ui::Avatar::initials_of("Solo"), "S");
    EXPECT_EQ(ui::Avatar::initials_of("   "), "?");

    const hui::gfx::Color first = ui::Avatar::color_of("Mara Voss", 0.55f, 0.72f);
    const hui::gfx::Color again = ui::Avatar::color_of("Mara Voss", 0.55f, 0.72f);
    const hui::gfx::Color other = ui::Avatar::color_of("Tobi Okafor", 0.55f, 0.72f);
    EXPECT_FLOAT_EQ(first.r, again.r);
    EXPECT_FLOAT_EQ(first.g, again.g);
    EXPECT_FLOAT_EQ(first.b, again.b);
    EXPECT_TRUE(first.r != other.r || first.g != other.g || first.b != other.b);

    ui::AvatarStack stack;
    stack.set_bounds({0.0f, 0.0f, 400.0f, 50.0f});
    std::vector<ui::AvatarPerson> people;
    for (const char *name : {"A B", "C D", "E F", "G H", "I J", "K L", "M N"})
        people.push_back({name});
    stack.set_people(people);
    EXPECT_EQ(stack.hidden(), 3);
    // Four faces and the "+3", each overlapping the one before.
    EXPECT_FLOAT_EQ(stack.width(), 50.0f + 4.0f * 50.0f * (1.0f - stack.style.overlap));
    stack.style.max_shown = 10;
    EXPECT_EQ(stack.hidden(), 0);
}

TEST_F(ComponentsIndicators, RatingStepsRefusesAtTheEndsAndClimbsInPitch)
{
    ui::Rating rating;
    rating.set_value(3.0f, true);
    EXPECT_EQ(send(rating, nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(rating.value(), 4.0f);
    ASSERT_TRUE(asked(Cue::slider));
    const float pitch_at_four = feedback_.cues.front().pitch;
    EXPECT_EQ(send(rating, nav(Direction::right)), Event::changed);
    EXPECT_GT(feedback_.cues.front().pitch, pitch_at_four);

    EXPECT_EQ(send(rating, nav(Direction::right)), Event::refused);
    EXPECT_FLOAT_EQ(rating.value(), 5.0f);
    EXPECT_TRUE(asked(Cue::error));
    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    EXPECT_EQ(send(rating, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    EXPECT_EQ(send(rating, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(rating, press(Action::back)), Event::cancelled);

    rating.style.allow_zero = false;
    rating.set_value(1.0f, true);
    EXPECT_EQ(send(rating, nav(Direction::left)), Event::refused);
    EXPECT_FLOAT_EQ(rating.value(), 1.0f);
}

TEST_F(ComponentsIndicators, RatingSnapsFractionsToItsStepAndCanBeDisplayOnly)
{
    ui::Rating rating;
    rating.set_value(4.3f, true);
    EXPECT_EQ(send(rating, nav(Direction::left)), Event::changed);
    EXPECT_FLOAT_EQ(rating.value(), 4.0f);

    rating.style.step = 0.5f;
    rating.set_value(4.3f, true);
    EXPECT_EQ(send(rating, nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(rating.value(), 4.5f);

    rating.style.interactive = false;
    EXPECT_EQ(send(rating, nav(Direction::right)), Event::none);
    EXPECT_FLOAT_EQ(rating.value(), 4.5f);
    EXPECT_TRUE(feedback_.cues.empty());

    rating.set_value(99.0f);
    EXPECT_FLOAT_EQ(rating.value(), 5.0f);
}

TEST_F(ComponentsIndicators, CounterFormatsEveryKindOfFigure)
{
    ui::Counter counter;
    EXPECT_EQ(counter.format(1234567.0), "1,234,567");
    EXPECT_EQ(counter.format(-1200.0), "-1,200");
    EXPECT_EQ(counter.format(999.0), "999");
    counter.style.separator = '\0';
    EXPECT_EQ(counter.format(1234567.0), "1234567");

    counter.style.separator = ',';
    counter.style.format = ui::CounterFormat::decimal;
    counter.style.decimals = 2;
    EXPECT_EQ(counter.format(1234.5), "1,234.50");
    counter.style.format = ui::CounterFormat::percent;
    counter.style.decimals = 1;
    EXPECT_EQ(counter.format(64.25), "64.3%");
    counter.style.decimals = 0;
    EXPECT_EQ(counter.format(64.25), "64%");

    counter.style.format = ui::CounterFormat::time;
    EXPECT_EQ(counter.format(205.0), "03:25");
    EXPECT_EQ(counter.format(3665.0), "1:01:05");
}

TEST_F(ComponentsIndicators, CounterCountsToItsTargetWithoutOvershoot)
{
    ui::Counter counter;
    counter.set_value(100.0, true);
    EXPECT_EQ(counter.text(), "100");
    counter.set_value(1000.0);
    EXPECT_DOUBLE_EQ(counter.value(), 1000.0);
    bool between = false;
    for (int i = 0; i < 600; ++i)
    {
        counter.update(kFrame);
        EXPECT_LE(counter.shown(), 1000.0);
        EXPECT_GE(counter.shown(), 100.0);
        between = between || (counter.shown() > 300.0 && counter.shown() < 700.0);
    }
    EXPECT_TRUE(between); // it passed through the middle
    EXPECT_DOUBLE_EQ(counter.shown(), 1000.0);
    EXPECT_EQ(counter.text(), "1,000");

    // Large numbers keep their last digit (a float spring would not).
    counter.set_value(123456789.0);
    run(counter, 12.0f);
    EXPECT_EQ(counter.text(), "123,456,789");
}

TEST_F(ComponentsIndicators, CounterDigitsShareOneCellWidthAndRoll)
{
    ui::Counter counter;
    ui::Painter paint(list_, fonts_, counter.style.theme);
    counter.set_value(1111.0, true);
    const float ones = counter.width(paint);
    counter.set_value(8888.0, true);
    EXPECT_FLOAT_EQ(counter.width(paint), ones); // tabular: no jitter
    counter.set_value(88888.0, true);
    EXPECT_GT(counter.width(paint), ones);

    // Rolling: half-way between 41 and 42 the wheel still shows 41, and the
    // draw holds both digits behind a clip.
    counter.style.rolling = true;
    counter.set_value(41.0, true);
    counter.set_value(42.0);
    while (counter.shown() < 41.4)
        counter.update(kFrame / 8.0f);
    EXPECT_EQ(counter.text(), "41");
    ui::Canvas target = canvas();
    counter.draw(target);
    bool clipped = false;
    for (const hui::gfx::Run &entry : list_.runs())
        clipped = clipped || entry.clipped;
    EXPECT_TRUE(clipped);
    run(counter, 4.0f);
    EXPECT_EQ(counter.text(), "42");

    // Reduced motion: the same figure, no roll.
    counter.style.reduced_motion = true;
    counter.set_value(43.0);
    counter.update(kFrame);
    ui::Canvas plain = canvas();
    counter.draw(plain);
    for (const hui::gfx::Run &entry : list_.runs())
        EXPECT_FALSE(entry.clipped);
}

TEST_F(ComponentsIndicators, SkeletonLaysOutBonesAndCrossFadesToContent)
{
    ui::Skeleton skeleton;
    skeleton.set_bounds({0.0f, 0.0f, 300.0f, 240.0f});
    skeleton.style.kind = ui::SkeletonKind::list_row;
    skeleton.style.rows = 3;
    EXPECT_EQ(skeleton.bones().size(), 9u); // a circle and two lines per row
    skeleton.style.rows = 20;
    EXPECT_LT(skeleton.bones().size(), 60u); // rows that do not fit are left out
    skeleton.style.kind = ui::SkeletonKind::line;
    skeleton.style.lines = 4;
    const std::vector<ui::Skeleton::Bone> lines = skeleton.bones();
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_LT(lines.back().rect.w, lines.front().rect.w);

    int drawn = 0;
    skeleton.content = [&](ui::Canvas &, const hui::gfx::Rect &, float) { ++drawn; };
    ui::Canvas before = canvas();
    skeleton.draw(before);
    EXPECT_EQ(drawn, 0); // nothing of the content while it loads
    EXPECT_FLOAT_EQ(skeleton.content_alpha(), 0.0f);

    skeleton.set_loaded(true);
    run(skeleton, skeleton.style.fade * 0.5f);
    EXPECT_GT(skeleton.content_alpha(), 0.05f);
    EXPECT_LT(skeleton.content_alpha(), 0.95f);
    ui::Canvas during = canvas();
    skeleton.draw(during);
    EXPECT_EQ(drawn, 1);
    run(skeleton, 1.0f);
    EXPECT_FLOAT_EQ(skeleton.content_alpha(), 1.0f);

    skeleton.set_loaded(false, true);
    EXPECT_FLOAT_EQ(skeleton.content_alpha(), 0.0f);
}

TEST_F(ComponentsIndicators, StatTileCountsItsValueInItsFormat)
{
    ui::StatTile tile;
    tile.label = "Frame time";
    tile.style.format = ui::CounterFormat::decimal;
    tile.style.decimals = 1;
    tile.set_value(16.4, true);
    EXPECT_EQ(tile.counter().text(), "16.4");
    tile.set_value(14.2);
    run(tile, 6.0f);
    EXPECT_EQ(tile.counter().text(), "14.2");
    tile.set_delta(-3.5f);
    EXPECT_FLOAT_EQ(tile.delta(), -3.5f);

    // A long figure shrinks to fit instead of leaving the tile.
    const std::array<float, 5> series = {1.0f, 3.0f, 2.0f, 5.0f, 4.0f};
    tile.set_series(series);
    tile.style.format = ui::CounterFormat::integer;
    tile.set_bounds({0.0f, 0.0f, 200.0f, 220.0f});
    tile.set_value(123456789012.0, true);
    ui::Canvas target = canvas();
    tile.draw(target);
    expect_drawn("stat tile");
    for (const hui::gfx::Instance &instance : list_.instances())
        EXPECT_LE(instance.rect[0] + instance.rect[2], 200.0f + 40.0f); // panel shadow aside
}

TEST_F(ComponentsIndicators, StateSurvivesRestyling)
{
    ui::Rating rating;
    rating.set_value(2.0f, true);
    ui::Counter counter;
    counter.set_value(77.0, true);
    ui::Badge badge;
    badge.set_count(4);
    ui::Skeleton skeleton;
    skeleton.set_loaded(true, true);
    ui::ProgressBar bar;
    bar.set_value(0.4f, true);

    const hui::ui::Theme &other = hui::ui::themes()[5];
    rating.style.theme = other;
    counter.style.theme = other;
    badge.style.theme = other;
    skeleton.style.theme = other;
    bar.style.theme = other;
    rating.update(kFrame);
    counter.update(kFrame);
    badge.update(kFrame);
    skeleton.update(kFrame);
    bar.update(kFrame);

    EXPECT_FLOAT_EQ(rating.value(), 2.0f);
    EXPECT_EQ(counter.text(), "77");
    EXPECT_EQ(badge.count(), 4);
    EXPECT_TRUE(skeleton.loaded());
    EXPECT_FLOAT_EQ(bar.shown(), 0.4f);
}

TEST_F(ComponentsIndicators, DrawsInEveryThemeAndVariant)
{
    const std::array<float, 6> series = {3.0f, 5.0f, 4.0f, 8.0f, 6.0f, 9.0f};
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            const bool reduced = variant == 3;
            const auto base = [&](ui::ComponentStyle &style)
            {
                style.theme = theme;
                style.reduced_motion = reduced;
            };
            ui::Canvas target = canvas();

            ui::ProgressBar bar;
            base(bar.style);
            bar.label = "Downloading";
            bar.style.mode = static_cast<ui::ProgressMode>(variant);
            bar.style.placement = static_cast<ui::LabelPlacement>(variant);
            bar.style.track = static_cast<ui::TrackStyle>(variant % 3);
            bar.style.radius_source = static_cast<ui::RadiusSource>(variant);
            bar.style.height = variant == 2 ? 28.0f : 14.0f;
            bar.style.sheen = true;
            bar.set_value(0.6f, true);
            bar.set_buffer(0.8f, true);
            bar.update(0.4f);
            bar.draw(target);

            ui::ProgressRing ring;
            base(ring.style);
            ring.label = "Left";
            ring.style.mode = static_cast<ui::ProgressMode>(variant);
            ring.style.two_tone = variant == 0;
            ring.style.ticks = variant * 4;
            ring.style.caps = static_cast<ui::RingCaps>(variant % 3);
            ring.style.sweep = variant == 2 ? 4.7f : 6.2831853f;
            ring.set_value(0.6f, true);
            ring.update(0.4f);
            ring.draw(target);

            ui::Spinner spinner;
            base(spinner.style);
            spinner.style.kind = static_cast<ui::SpinnerKind>(variant);
            spinner.update(0.4f);
            spinner.draw(target);

            ui::Meter meter;
            base(meter.style);
            meter.label = "Signal";
            meter.style.shape = variant % 2 == 0 ? ui::MeterShape::linear : ui::MeterShape::radial;
            meter.style.segments = variant == 2 ? 10 : 0;
            meter.set_bounds({0.0f, 0.0f, 200.0f, variant % 2 == 0 ? 60.0f : 200.0f});
            meter.set_value(0.9f, true);
            meter.set_value(0.5f);
            meter.update(0.1f);
            meter.draw(target);

            ui::Badge badge;
            base(badge.style);
            badge.style.kind = static_cast<ui::Status>(variant);
            badge.style.fill = static_cast<ui::BadgeFill>(variant % 3);
            badge.style.dot = variant == 3;
            badge.style.pulse = true;
            badge.style.cutout = 3.0f;
            badge.set_count(12);
            badge.update(0.1f);
            badge.draw(target);

            ui::Chip chip;
            base(chip.style);
            chip.label = "Co-op";
            chip.style.leading_dot = variant % 2 == 0;
            chip.style.removable = variant > 1;
            chip.set_selected(variant == 1, true);
            chip.set_focused(variant == 2);
            chip.update(0.1f);
            chip.draw(target);

            ui::Avatar avatar;
            base(avatar.style);
            avatar.style.shape = static_cast<ui::AvatarShape>(variant % 2);
            avatar.set_name("Mara Voss");
            avatar.set_presence(static_cast<ui::Presence>(variant + 1));
            avatar.update(0.1f);
            avatar.draw(target);

            ui::AvatarStack stack;
            base(stack.style);
            stack.style.shape = static_cast<ui::AvatarShape>(variant % 2);
            stack.set_people({{"A B"}, {"C D"}, {"E F"}, {"G H"}, {"I J"}, {"K L"}});
            stack.update(0.1f);
            stack.draw(target);

            ui::Rating rating;
            base(rating.style);
            rating.style.glyph = static_cast<ui::RatingGlyph>(variant % 3);
            rating.style.show_value = true;
            rating.set_active(true);
            rating.set_value(3.5f, true);
            rating.update(0.1f);
            rating.draw(target);

            ui::Counter counter;
            base(counter.style);
            counter.style.format = static_cast<ui::CounterFormat>(variant);
            counter.style.face = static_cast<ui::CounterFace>(variant % 2);
            counter.style.rolling = variant % 2 == 1;
            counter.style.prefix = "x";
            counter.style.suffix = "pts";
            counter.set_value(998.5, true);
            counter.set_value(1250.75);
            counter.update(0.2f);
            counter.draw(target);

            ui::Skeleton skeleton;
            base(skeleton.style);
            skeleton.style.kind = static_cast<ui::SkeletonKind>(variant + 1);
            skeleton.style.panel = variant % 2 == 0;
            skeleton.update(0.3f);
            skeleton.draw(target);

            ui::StatTile tile;
            base(tile.style);
            tile.label = "Players online";
            tile.style.rolling = variant == 1;
            tile.set_value(12480.0, true);
            tile.set_delta(static_cast<float>(variant) - 1.0f);
            tile.set_series(series);
            tile.update(0.3f);
            tile.draw(target);

            ui::EmptyState empty;
            base(empty.style);
            empty.title = "No saves yet";
            empty.body = "Finish a chapter and it appears here, with its date and place.";
            empty.action = "New game";
            empty.style.panel = variant % 2 == 0;
            empty.enter();
            empty.update(0.3f);
            empty.draw(target);

            expect_drawn(theme.id);
        }
    }
}

} // namespace
