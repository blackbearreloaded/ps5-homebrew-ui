// ps5-homebrew-ui - Tests that hold for every design in the registry.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"

#include <set>
#include <string>

namespace
{

using hui::app::Concept;
using hui::testing::ConceptFixture;

class EveryConcept : public ConceptFixture
{
};

TEST_F(EveryConcept, DescribesItself)
{
    std::set<std::string> ids;
    for (hui::app::ConceptFactory factory : hui::app::concept_registry())
    {
        const std::unique_ptr<Concept> design = factory(context_);
        const hui::app::ConceptInfo &info = design->info();
        ASSERT_NE(info.id, nullptr);
        EXPECT_TRUE(ids.insert(info.id).second) << "duplicate id " << info.id;
        EXPECT_GT(std::string(info.name).size(), 2u) << info.id;
        EXPECT_GT(std::string(info.tagline).size(), 10u) << info.id;
        // The info panel shows the path: it must be the real one.
        EXPECT_EQ(std::string(info.source), std::string("src/concepts/") + info.id + ".cpp");
        EXPECT_GE(info.techniques.size(), 3u) << info.id;
        EXPECT_LT(info.sounds, hui::audio::SoundSet::count) << info.id;
    }
    EXPECT_GE(ids.size(), 2u);
}

// The tour is what the snapshots and the console validation run: every design
// must get through its own script, drawing a valid frame after every step.
TEST_F(EveryConcept, SurvivesItsTourAndAlwaysDraws)
{
    for (hui::app::ConceptFactory factory : hui::app::concept_registry())
    {
        const std::unique_ptr<Concept> design = factory(context_);
        design->enter();
        send(*design, idle(), 1.5f);
        expect_drawable(*design);
        for (const hui::app::TourStep &step : design->tour())
        {
            hui::InputFrame input = idle();
            input.stick_x = step.stick_x;
            input.stick_y = step.stick_y;
            input.held = step.hold;
            input.trigger_l = step.trigger_l;
            input.trigger_r = step.trigger_r;
            send(*design, input, step.wait);
            input.pressed = step.press;
            input.held = step.press | step.hold;
            input.nav = step.nav;
            send(*design, input, 0.05f);
            expect_drawable(*design);
        }
    }
}

// A player can lean on any button: no input sequence may break a design.
TEST_F(EveryConcept, SurvivesArbitraryInput)
{
    const hui::Action actions[] = {
        hui::Action::confirm, hui::Action::back,      hui::Action::north,
        hui::Action::west,    hui::Action::jump_prev, hui::Action::jump_next,
        hui::Action::menu,    hui::Action::l3,        hui::Action::r3};
    const hui::Direction directions[] = {hui::Direction::up, hui::Direction::down,
                                         hui::Direction::left, hui::Direction::right};
    for (hui::app::ConceptFactory factory : hui::app::concept_registry())
    {
        const std::unique_ptr<Concept> design = factory(context_);
        design->enter();
        std::uint32_t state = 0x1234567u; // the same pseudo-random walk every run
        for (int step = 0; step < 600; ++step)
        {
            state = state * 1664525u + 1013904223u;
            hui::InputFrame input = idle();
            const std::uint32_t pick = (state >> 8) % 16;
            if (pick < 4)
                input.nav = directions[pick];
            else if (pick < 13)
                input.pressed = input.held = hui::action_bit(actions[pick - 4]);
            input.stick_x = static_cast<float>((state >> 12) % 200) / 100.0f - 1.0f;
            input.stick_y = static_cast<float>((state >> 20) % 200) / 100.0f - 1.0f;
            input.nav_repeat = (state & 0x40000000u) != 0 && input.nav != hui::Direction::none;
            send(*design, input, 0.03f);
            if (step % 20 == 0)
                expect_drawable(*design);
        }
        // Reduced motion must not break anything either.
        settings_.reduced_motion = true;
        design->enter();
        send(*design, press(hui::Action::confirm), 0.3f);
        expect_drawable(*design);
        settings_.reduced_motion = false;
    }
}

} // namespace
