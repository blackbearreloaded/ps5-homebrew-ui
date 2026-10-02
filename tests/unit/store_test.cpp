// ps5-homebrew-ui - Behaviour tests for the Storefront design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A shop is mostly numbers, so besides the cues these tests read the screen
// the way the player does: prices and counts are set in the mono face, whose
// digits can be recognised in the draw list by their atlas rectangles. That
// keeps the tests on the real interface: what is drawn is what is checked.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::audio::CueEvent;
using hui::gfx::Rect;

constexpr float kFrame = 1.0f / 60.0f;

// The filter chips, in their order on screen.
enum Shelf
{
    kAll,
    kOnSale,
    kNew,
    kFree,
    kOwned,
    kShelves,
};

// Where the numbers live (virtual pixels).
constexpr Rect kCounter{1760.0f, 36.0f, 70.0f, 48.0f};       // the cart's badge, top right
constexpr Rect kPriceBlock{1336.0f, 462.0f, 488.0f, 118.0f}; // the product page's buy box
constexpr Rect kSubtotal{1280.0f, 836.0f, 640.0f, 56.0f};    // the foot of the cart drawer

// A run of mono digits found on screen.
struct Number
{
    int value = 0;
    float height = 0.0f; // of its glyphs: tells a headline price from a small one
};

class Store : public hui::testing::ConceptFixture
{
  protected:
    void SetUp() override
    {
        // The atlas rectangle of each digit: its signature in a draw list.
        for (int digit = 0; digit < 10; ++digit)
        {
            const char text[2] = {static_cast<char>('0' + digit), '\0'};
            std::vector<hui::gfx::GlyphQuad> quads;
            mono_.layout(text, 0.0f, 0.0f, 32.0f, hui::gfx::Align::left, quads);
            ASSERT_EQ(quads.size(), 1u);
            digits_[digit] = quads.front();
        }
    }

    // Sends one input, then idles for `seconds`; returns every cue asked for.
    std::vector<CueEvent> run(const hui::InputFrame &input, float seconds)
    {
        std::vector<CueEvent> cues;
        feedback_.clear();
        design_->update(input, kFrame, feedback_);
        cues = feedback_.cues;
        for (int frame = 0, frames = static_cast<int>(seconds / (kFrame)); frame < frames; ++frame)
        {
            feedback_.clear();
            design_->update(idle(), kFrame, feedback_);
            cues.insert(cues.end(), feedback_.cues.begin(), feedback_.cues.end());
        }
        return cues;
    }

    static int count(const std::vector<CueEvent> &cues, Cue cue)
    {
        int found = 0;
        for (const CueEvent &event : cues)
            found += event.cue == cue ? 1 : 0;
        return found;
    }

    // Every number drawn in the mono face inside `region`, left to right.
    std::vector<Number> numbers(const Rect &region)
    {
        struct Glyph
        {
            float x, bottom, height;
            int digit;
        };
        frame_.reset();
        design_->draw(frame_);
        std::vector<Glyph> glyphs;
        for (const hui::gfx::DrawList *list : {&frame_.scene, &frame_.overlay})
        {
            for (const hui::gfx::Instance &instance : list->instances())
            {
                if (static_cast<int>(instance.params[3]) !=
                    static_cast<int>(hui::gfx::Shape::glyph))
                    continue;
                // Skip what is on its way in or out (a struck-through price is
                // drawn at half opacity when at rest, so the bar sits below it).
                if (instance.color_top[3] < 0.3f)
                    continue;
                const float cx = instance.rect[0] + instance.rect[2] * 0.5f;
                const float cy = instance.rect[1] + instance.rect[3] * 0.5f;
                if (cx < region.x || cx > region.x + region.w || cy < region.y ||
                    cy > region.y + region.h)
                    continue;
                for (int digit = 0; digit < 10; ++digit)
                {
                    const hui::gfx::GlyphQuad &q = digits_[digit];
                    if (std::fabs(instance.extra[0] - q.u0) < 1e-6f &&
                        std::fabs(instance.extra[1] - q.v0) < 1e-6f &&
                        std::fabs(instance.extra[2] - q.u1) < 1e-6f &&
                        std::fabs(instance.extra[3] - q.v1) < 1e-6f)
                        glyphs.push_back({instance.rect[0], instance.rect[1] + instance.rect[3],
                                          instance.rect[3], digit});
                }
            }
        }
        std::sort(glyphs.begin(), glyphs.end(),
                  [](const Glyph &a, const Glyph &b) { return a.x < b.x; });
        // Digits of one number sit on one baseline, are one size and follow
        // each other within two advances (a thousands separator in between).
        std::vector<Number> found;
        std::vector<Glyph> tails;
        for (const Glyph &glyph : glyphs)
        {
            std::size_t owner = tails.size();
            for (std::size_t i = 0; i < tails.size(); ++i)
            {
                const Glyph &tail = tails[i];
                if (std::fabs(glyph.bottom - tail.bottom) < 0.2f * glyph.height &&
                    std::fabs(glyph.height - tail.height) < 0.2f * glyph.height &&
                    glyph.x - tail.x < 2.0f * glyph.height)
                    owner = i;
            }
            if (owner == tails.size())
            {
                tails.push_back(glyph);
                found.push_back({glyph.digit, glyph.height});
            }
            else
            {
                tails[owner] = glyph;
                found[owner].value = found[owner].value * 10 + glyph.digit;
                found[owner].height = std::max(found[owner].height, glyph.height);
            }
        }
        return found;
    }

    // What the cart's badge says (no badge is an empty cart).
    int cart_count()
    {
        const std::vector<Number> shown = numbers(kCounter);
        return shown.empty() ? 0 : shown.front().value;
    }
    int subtotal()
    {
        const std::vector<Number> shown = numbers(kSubtotal);
        return shown.empty() ? -1 : shown.front().value;
    }
    // The product page's prices, the headline (largest) first.
    std::vector<Number> page_prices()
    {
        std::vector<Number> shown = numbers(kPriceBlock);
        std::sort(shown.begin(), shown.end(),
                  [](const Number &a, const Number &b) { return a.height > b.height; });
        return shown;
    }

    // Switches the filter with the triggers; the focus lands on its first card.
    void show(Shelf shelf)
    {
        for (int i = 0; i < kShelves; ++i)
            run(press(Action::jump_prev), 0.05f);
        for (int i = 0; i < shelf; ++i)
            run(press(Action::jump_next), 0.05f);
        run(idle(), 0.7f);
    }

    // How many cards the grid holds: walk down its first column, then along
    // its last row, until each refuses. Starts from the first card.
    int shelf_size()
    {
        int rows = 1;
        for (; rows < 30; ++rows)
        {
            send(*design_, nav(Direction::down), 0.05f);
            if (played(Cue::error))
                break;
        }
        int columns = 1;
        for (; columns < 30; ++columns)
        {
            send(*design_, nav(Direction::right), 0.05f);
            if (played(Cue::error))
                break;
        }
        return (rows - 1) * 5 + columns;
    }

    // From the first card of the shelf: step right, open the page, add the
    // title to the cart and come back. Returns the price the page showed.
    int add_title(int steps_right)
    {
        for (int i = 0; i < steps_right; ++i)
            send(*design_, nav(Direction::right), 0.05f);
        run(press(Action::confirm), 1.2f);
        const std::vector<Number> prices = page_prices();
        const std::vector<CueEvent> cues = run(press(Action::confirm), 1.2f);
        EXPECT_EQ(count(cues, Cue::select), 1);
        run(press(Action::back), 0.8f);
        for (int i = 0; i < steps_right; ++i)
            send(*design_, nav(Direction::left), 0.05f);
        return prices.empty() ? 0 : prices.front().value;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_store(context_);
    hui::gfx::GlyphQuad digits_[10] = {};
};

TEST_F(Store, TheChipsFilterTheGridAndTheirEndsRefuse)
{
    int newest = 0;
    int new_titles = 0;
    for (const hui::demo::Item &item : catalog_.items())
        newest = std::max(newest, item.year);
    for (const hui::demo::Item &item : catalog_.items())
        new_titles += item.year == newest ? 1 : 0;

    const int all = shelf_size();
    EXPECT_EQ(all, static_cast<int>(catalog_.size()));

    // L2 on the first chip is the end of the row: a refusal, not a wrap.
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::tab));
    const int on_sale = shelf_size();
    show(kNew);
    EXPECT_EQ(shelf_size(), new_titles);
    show(kFree);
    const int free = shelf_size();
    show(kOwned);
    const int owned = shelf_size();
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));

    // Each shelf holds something, and the three are different titles: a title
    // on sale or free is one the player does not own.
    EXPECT_GT(on_sale, 0);
    EXPECT_GT(free, 0);
    EXPECT_GT(owned, 0);
    EXPECT_LT(on_sale + free + owned, all);
    expect_drawable(*design_);
}

TEST_F(Store, ADiscountedPriceIsLowerThanTheBasePrice)
{
    show(kOnSale);
    const int on_sale = shelf_size();
    for (int card = 0; card < on_sale; ++card)
    {
        show(kOnSale);
        for (int i = 0; i < card / 5; ++i)
            send(*design_, nav(Direction::down), 0.05f);
        for (int i = 0; i < card % 5; ++i)
            send(*design_, nav(Direction::right), 0.05f);
        run(press(Action::confirm), 1.2f);
        // The buy box shows three numbers: the price in large digits, the
        // struck-through base price, and what the player saves.
        const std::vector<Number> prices = page_prices();
        ASSERT_EQ(prices.size(), 3u) << "card " << card;
        EXPECT_GT(prices[0].value, 0);
        EXPECT_LT(prices[0].value, prices[1].value) << "card " << card;
        EXPECT_EQ(prices[1].value - prices[0].value, prices[2].value) << "card " << card;
        run(press(Action::back), 0.8f);
    }
}

TEST_F(Store, AddToCartCountsATitleOnceAndTheCounterWaitsForTheFlight)
{
    send(*design_, press(Action::confirm), 1.0f);
    EXPECT_TRUE(played(Cue::open));
    EXPECT_EQ(cart_count(), 0);

    // The press answers at once; the counter changes when the cover lands.
    const std::vector<CueEvent> flight = run(press(Action::confirm), 0.2f);
    EXPECT_EQ(count(flight, Cue::select), 1);
    EXPECT_EQ(count(flight, Cue::notify), 0);
    EXPECT_EQ(cart_count(), 0);
    const std::vector<CueEvent> landing = run(idle(), 1.0f);
    EXPECT_EQ(count(landing, Cue::notify), 1);
    EXPECT_EQ(cart_count(), 1);

    // A title already in the cart refuses, and the count stays.
    const std::vector<CueEvent> again = run(press(Action::confirm), 1.0f);
    EXPECT_EQ(count(again, Cue::error), 1);
    EXPECT_EQ(count(again, Cue::select), 0);
    EXPECT_EQ(cart_count(), 1);

    // So does a title the player owns.
    run(press(Action::back), 0.8f);
    show(kOwned);
    run(press(Action::confirm), 1.0f);
    const std::vector<CueEvent> owned = run(press(Action::confirm), 1.0f);
    EXPECT_EQ(count(owned, Cue::error), 1);
    EXPECT_EQ(count(owned, Cue::select), 0);
    EXPECT_EQ(cart_count(), 1);
    expect_drawable(*design_);
}

TEST_F(Store, TriangleRemovesALineAndTheSubtotalCountsDown)
{
    const int first = add_title(0);
    const int second = add_title(1);
    EXPECT_GT(first, 0);
    EXPECT_GT(second, 0);
    EXPECT_EQ(cart_count(), 2);

    send(*design_, press(Action::west), 1.2f);
    EXPECT_TRUE(played(Cue::open));
    EXPECT_EQ(subtotal(), first + second);

    // The drawer opens on Checkout; one step up is the last line.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    run(press(Action::north), 0.2f);
    // Mid-count the subtotal is between the two sums ...
    EXPECT_LT(subtotal(), first + second);
    EXPECT_GT(subtotal(), first);
    run(idle(), 1.0f);
    // ... and it arrives exactly.
    EXPECT_EQ(subtotal(), first);
    EXPECT_EQ(cart_count(), 1);

    // The focus fell back on the button, where there is nothing to remove.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_EQ(cart_count(), 1);
    expect_drawable(*design_);
}

TEST_F(Store, TheCheckoutAsksFirstWithTheFocusOnCancel)
{
    add_title(0);
    send(*design_, press(Action::west), 1.0f);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    // Cross straight away is Cancel: nothing is bought.
    const std::vector<CueEvent> cancelled = run(press(Action::confirm), 1.5f);
    EXPECT_EQ(count(cancelled, Cue::modal_close), 1);
    EXPECT_EQ(count(cancelled, Cue::saved), 0);
    EXPECT_EQ(cart_count(), 1);

    // Circle cancels too, from either button.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::back), 1.0f);
    EXPECT_TRUE(played(Cue::modal_close));
    EXPECT_EQ(cart_count(), 1);
    expect_drawable(*design_);
}

TEST_F(Store, BuyingMarksTheTitlesOwnedAndEmptiesTheCart)
{
    show(kOwned);
    const int owned_before = shelf_size();
    show(kAll);
    add_title(0);
    add_title(1);
    ASSERT_EQ(cart_count(), 2);

    send(*design_, press(Action::west), 1.0f);
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::right));
    const std::vector<CueEvent> bought = run(press(Action::confirm), 2.5f);
    EXPECT_EQ(count(bought, Cue::saved), 1);
    EXPECT_EQ(count(bought, Cue::notify), 1); // the "Purchase complete" toast
    EXPECT_EQ(cart_count(), 0);
    EXPECT_EQ(subtotal(), 0);
    expect_drawable(*design_);

    // An empty cart has nothing to check out.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));

    // Both titles moved to the Owned shelf, and cannot be bought again.
    show(kOwned);
    EXPECT_EQ(shelf_size(), owned_before + 2);
    show(kAll);
    run(press(Action::confirm), 1.0f);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_EQ(cart_count(), 0);
}

TEST_F(Store, EdgesRefuseSoftlyAndTheFocusClimbsToTheBanner)
{
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());

    // Up from the top row: the chips, then the banner, then nothing.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    // The banner is a loop: left and right always turn its page.
    for (int i = 0; i < 7; ++i)
    {
        send(*design_, nav(i < 2 ? Direction::left : Direction::right), 0.1f);
        EXPECT_TRUE(played(Cue::slider));
    }
    send(*design_, press(Action::confirm), 1.0f);
    EXPECT_TRUE(played(Cue::open));
    send(*design_, press(Action::back), 1.0f);
    EXPECT_TRUE(played(Cue::back));
    // On the chips, left and right are the filter.
    send(*design_, nav(Direction::down));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, nav(Direction::down));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Store, ThePageFocusWalksTheThumbnailsAndTheButtons)
{
    send(*design_, press(Action::confirm), 1.0f);
    // The focus opens on "Add to cart", the upper of two buttons.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    // Left enters the thumbnails at the first one; they end on both sides.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    for (int i = 0; i < 3; ++i)
    {
        send(*design_, nav(Direction::right));
        EXPECT_TRUE(played(Cue::focus)) << "step " << i;
    }
    // ... which brought the focus back to the button; Wishlist is below it.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::confirm));
    ASSERT_TRUE(played(Cue::toggle));
    const float on = last_cues_.front().pitch;
    send(*design_, press(Action::confirm));
    ASSERT_TRUE(played(Cue::toggle));
    EXPECT_LT(last_cues_.front().pitch, on); // off sounds lower than on
    EXPECT_EQ(cart_count(), 0);              // a wish is not a purchase
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    expect_drawable(*design_);
}

} // namespace
