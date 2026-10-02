// ps5-homebrew-ui - Component Library page: Carousel, GridView and the Card they draw.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A shelf across the top and a grid below it, both filled with the invented
// catalogue. Square swaps the whole layout: every variant is the same two
// components with other style values (mode, sizes, text placement, focus
// treatment), and one of them hands the drawing of an item to a slot. The
// page only decides which of the two has the focus: the shelf ignores up and
// down, and the grid reports when the focus leaves through its top edge.

#include "concepts/components/page.hpp"

#include "ui/components/carousel.hpp"
#include "ui/components/grid.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kCaption = 262.0f; // baseline of the shelf's caption
constexpr float kShelfTop = 284.0f;
constexpr float kCaptionGap = 36.0f; // from the shelf to the grid's caption
constexpr float kGridGap = 16.0f;    // from that caption to the grid
constexpr float kBottom = 948.0f;
constexpr float kThumbRoom = 16.0f; // the grid's scroll thumb sits outside its bounds
// The part of a sample cover without lettering, as a canvas uv (top row last).
constexpr float kCoverBandTop = 0.13f;
constexpr float kCoverBandHeight = 0.6f;
constexpr Rect kCoverBand{0.0f, 1.0f - kCoverBandTop, 1.0f, -kCoverBandHeight};

struct Variant
{
    const char *name;
    const char *shelf;
    const char *grid;
};
constexpr Variant kVariants[] = {
    {"Shelf + poster grid", "Carousel: leading, the row scrolls under the focus",
     "GridView: ten columns of posters, text over the art"},
    {"Centered wheel + wide tiles", "Carousel: centered, wraps around, lit in the item's colour",
     "GridView: 16:9 tiles, text below, reading-order wrap"},
    {"Paged + compact squares", "Carousel: paged, four to a page, text over the art",
     "GridView: compact squares, dimmed neighbours, confirm selects"},
    {"Hero pager + plated cards", "Carousel: one item to a page, drawn by a slot",
     "GridView: cards on a themed plate, rows wrap around"},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::square, "Layout"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kNext = action_bit(Action::west);
constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.7f, kConfirm, Direction::none, "collections"},
    {0.35f, 0, Direction::down},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::down},
    {0.7f, kNext, Direction::none, "collections-posters"},
    {0.5f, 0, Direction::up},
    {0.2f, 0, Direction::up},
    {0.25f, 0, Direction::up},
    {0.3f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.7f, kNext, Direction::none, "collections-wheel"},
    {0.5f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.4f, 0, Direction::down},
    {0.3f, kConfirm},
    {0.25f, 0, Direction::right},
    {0.3f, kConfirm},
    {0.7f, kNext, Direction::none, "collections-paged"},
    {0.6f, 0, Direction::down},
    {0.7f, kNext, Direction::none, "collections-hero"},
};

class CollectionsPage final : public Page
{
  public:
    explicit CollectionsPage(app::Context &context) : context_(context)
    {
        std::vector<ui::CardItem> items;
        for (std::size_t i = 0; i < context_.catalog.size(); ++i)
        {
            const demo::Item &title = context_.catalog[i];
            ui::CardItem item;
            item.title = title.title;
            item.subtitle = title.genre;
            item.texture = title.cover;
            // The sample covers carry their own lettering; the cards show the
            // band between the name of the studio and the title.
            item.uv = kCoverBand;
            item.image_aspect = 1.0f / kCoverBandHeight;
            item.top = title.mid;
            item.bottom = title.dark;
            item.accent = title.accent;
            item.tag = static_cast<int>(i);
            items.push_back(item);
        }
        shelf_.set_items(items);
        for (ui::CardItem &item : items)
        {
            if (context_.catalog[static_cast<std::size_t>(item.tag)].year >= 2026)
                item.badge = "NEW";
        }
        grid_.set_items(std::move(items));
        apply(false);
    }

    const char *title() const override
    {
        return "Collections";
    }
    const char *summary() const override
    {
        return "ui::Carousel, ui::GridView and the ui::Card they draw: modes, sizes, text, focus";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply(false);
    }

    void enter() override
    {
        shelf_.enter();
        grid_.enter();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply(true);
            ui::play_cue(feedback, shelf_.style, shelf_.style.sounds.change);
        }
        else if (on_grid_)
        {
            report("GridView", grid_.handle(input, feedback), grid_.items(), grid_.focus());
            if (grid_.exit() == Direction::up)
            {
                focus_grid(false);
                ui::play_cue(feedback, shelf_.style, shelf_.style.sounds.move,
                             shelf_.item_rect(shelf_.focus()).cx(), 1.05f);
            }
        }
        else if (input.nav == Direction::down)
        {
            focus_grid(true);
            ui::play_cue(feedback, grid_.style, grid_.style.sounds.move,
                         grid_.cell_rect(grid_.focus()).cx(), 0.95f);
        }
        else if (input.nav == Direction::up)
        {
            // Nothing above the shelf: the page answers for it.
            ui::refuse(feedback, shelf_.style, input, edge_, shelf_.bounds().cx());
        }
        else
        {
            report("Carousel", shelf_.handle(input, feedback), shelf_.items(), shelf_.focus());
        }
        shelf_.update(dt);
        grid_.update(dt);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);
        const Variant &now = kVariants[variant_];
        const float grid_caption = grid_.bounds().y - kGridGap;
        paint.label(now.shelf, kPageArea.x + nudge, kCaption, 20.0f,
                    on_grid_ ? paint.page_text_muted() : paint.page_text());
        paint.label(now.grid, kPageArea.x, grid_caption, 20.0f,
                    on_grid_ ? paint.page_text() : paint.page_text_muted());
        if (!status_.empty())
            paint.body(status_, kPageArea.x + kPageArea.w, grid_caption, 20.0f,
                       paint.page_text_muted(), gfx::Align::right);
        shelf_.draw(canvas);
        grid_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    void focus_grid(bool on_grid)
    {
        on_grid_ = on_grid;
        shelf_.set_active(!on_grid_);
        grid_.set_active(on_grid_);
    }

    void report(const char *who, ui::Event event, const std::vector<ui::CardItem> &items, int focus)
    {
        const char *what = nullptr;
        switch (event)
        {
        case ui::Event::moved:
            what = "moved";
            break;
        case ui::Event::changed:
            what = "changed";
            break;
        case ui::Event::activated:
            what = "activated";
            break;
        case ui::Event::cancelled:
            what = "cancelled";
            break;
        case ui::Event::refused:
            what = "refused";
            break;
        default:
            return;
        }
        status_ = std::string(who) + "  \xC2\xB7  Event::" + what;
        if (event != ui::Event::cancelled && !items.empty())
            status_ += "  \xC2\xB7  " + items[static_cast<std::size_t>(focus)].title;
    }

    // Every variant starts from the components' defaults, so each block below
    // is the complete recipe of its look.
    void apply(bool replay)
    {
        ui::CarouselStyle shelf;
        ui::GridStyle grid;
        shelf.theme = grid.theme = theme_;
        shelf.reduced_motion = grid.reduced_motion = reduced_;
        grid.exits.up = true; // the shelf is above: let the focus go there
        shelf_.title.clear();
        shelf_.content = nullptr;
        bool progress = false;

        switch (variant_)
        {
        case 0: // a streaming shelf over a wall of posters
            shelf_.title = "Continue playing";
            shelf.mode = ui::CarouselMode::leading;
            shelf.item_width = 248.0f;
            shelf.card.art_aspect = 16.0f / 9.0f;
            shelf.card.focus_scale = 1.08f;
            progress = true;
            grid.columns = 10;
            grid.gap_x = grid.gap_y = 18.0f;
            grid.card.art_aspect = 3.0f / 4.0f;
            grid.card.text = ui::CardText::over;
            grid.card.title_size = 20.0f;
            grid.card.subtitle_size = 0.0f;
            grid.card.text_inset = 12.0f;
            grid.card.badge_corner = ui::CardCorner::top_right;
            break;
        case 1: // a cover wheel over wide tiles
            shelf.mode = ui::CarouselMode::centered;
            shelf.item_width = 184.0f;
            shelf.gap = 24.0f;
            shelf.wrap = true;
            shelf.neighbour_fade = 0.2f;
            shelf.card.focus_scale = 1.3f;
            shelf.card.lift = 0.0f;
            shelf.card.align = gfx::Align::center;
            shelf.card.title_size = 22.0f;
            shelf.card.subtitle_size = 0.0f;
            shelf.card.glow = true;
            grid.columns = 6;
            grid.wrap = ui::GridWrap::flow;
            grid.card.art_aspect = 16.0f / 9.0f;
            grid.card.title_size = 22.0f;
            grid.card.subtitle_size = 19.0f;
            break;
        case 2: // pages of wide tiles over a dense wall you can tick
            shelf_.title = "New this week";
            shelf.mode = ui::CarouselMode::paged;
            shelf.per_page = 4;
            shelf.peek = 44.0f;
            shelf.gap = 20.0f;
            shelf.card.art_aspect = 2.2f;
            shelf.card.text = ui::CardText::over;
            shelf.card.focus_scale = 1.04f;
            shelf.card.lift = 4.0f;
            grid.columns = 10;
            grid.gap_x = grid.gap_y = 16.0f;
            grid.select_on_confirm = true;
            grid.card.text = ui::CardText::over;
            grid.card.title_size = 20.0f;
            grid.card.subtitle_size = 0.0f;
            grid.card.text_inset = 12.0f;
            grid.card.focus_scale = 1.04f;
            grid.card.dim = 0.35f;
            grid.card.badge_corner = ui::CardCorner::top_left;
            break;
        default: // one hero at a time over cards that sit on a plate
            shelf.mode = ui::CarouselMode::paged;
            shelf.per_page = 1;
            shelf.peek = 0.0f;
            shelf.item_height = 190.0f;
            shelf.card.focus_scale = 1.0f;
            shelf.card.lift = 0.0f;
            shelf_.content = [this](ui::Canvas &canvas, const Rect &r, const ui::CardItem &item,
                                    int, float) { draw_hero(canvas, r, item); };
            grid.columns = 8;
            grid.gap_x = grid.gap_y = 18.0f;
            grid.wrap = ui::GridWrap::rows;
            grid.card.plate = true;
            grid.card.align = gfx::Align::center;
            grid.card.title_size = 20.0f;
            grid.card.subtitle_size = 0.0f;
            grid.card.focus_scale = 1.05f;
            break;
        }
        shelf_.style = shelf;
        grid_.style = grid;
        for (std::size_t i = 0; i < shelf_.items().size(); ++i)
        {
            const float played = context_.catalog[i].progress;
            shelf_.item(static_cast<int>(i)).progress =
                progress && played > 0.0f && played < 1.0f ? played : -1.0f;
        }

        // The shelf says how tall it is in this look; the grid takes the rest.
        shelf_.set_bounds({kPageArea.x, kShelfTop, kPageArea.w, 100.0f});
        const float shelf_h = std::ceil(shelf_.preferred_height());
        shelf_.set_bounds({kPageArea.x, kShelfTop, kPageArea.w, shelf_h});
        const float grid_top = kShelfTop + shelf_h + kCaptionGap + kGridGap;
        grid_.set_bounds({kPageArea.x, grid_top, kPageArea.w - kThumbRoom, kBottom - grid_top});
        focus_grid(on_grid_);
        if (replay)
        {
            shelf_.enter();
            grid_.enter();
        }
    }

    // The slot of the fourth variant: a banner per title, built from the
    // theme's own surface, heading, text and progress bar.
    void draw_hero(ui::Canvas &canvas, const Rect &r, const ui::CardItem &item) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const demo::Item &title = context_.catalog[static_cast<std::size_t>(item.tag)];
        const float radius = std::min(theme_.radius_card, r.h * 0.22f);
        const Rect body = paint.surface(r, radius, theme_.surface, theme_.outline, 1.0f);
        const Rect art = Rect{body.x, body.y, body.h, body.h}.inset(18.0f);
        const float corner =
            theme_.corner == ui::Corner::round ? std::min(theme_.radius, 14.0f) : 0.0f;
        if (item.texture != 0)
            list.image(item.texture, art, gfx::kCanvasUv, Color::rgb(0xffffff), corner);
        else
            list.gradient_rect(art, corner, title.mid, title.dark);

        const float x = art.x + art.w + 30.0f;
        const float right = body.x + body.w - 36.0f;
        char line[96];
        std::snprintf(line, sizeof(line), "%s  \xC2\xB7  %s  \xC2\xB7  %d", title.genre,
                      title.studio, title.year);
        paint.label(line, x, body.y + 48.0f, 19.0f, theme_.text_muted);
        paint.heading(title.title, x, body.y + 100.0f, 44.0f, theme_.text);
        paint.body(ui::fit_body(paint, title.blurb, 22.0f, right - x - 240.0f), x, body.y + 138.0f,
                   22.0f, theme_.text_muted);

        // What the right end says: how far the story is, and the hours behind it.
        std::snprintf(line, sizeof(line), "%d h played", title.hours);
        paint.body(line, right, body.y + 100.0f, 22.0f, theme_.text, gfx::Align::right);
        // Languages that fill a bar with blocks need a taller one to hold them.
        const float bar = 12.0f + 2.0f * theme_.border;
        paint.progress({right - 200.0f, body.y + 124.0f - bar * 0.5f, 200.0f, bar}, title.progress);
        std::snprintf(line, sizeof(line), "%d%% of the story",
                      static_cast<int>(title.progress * 100.0f + 0.5f));
        paint.body(line, right, body.y + 160.0f, 20.0f, theme_.text_muted, gfx::Align::right);
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::Carousel shelf_;
    ui::GridView grid_;
    bool on_grid_ = false;
    int variant_ = 0;
    float clock_ = 0.0f;
    ui::Pulse edge_;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_collections_page(app::Context &context)
{
    return std::make_unique<CollectionsPage>(context);
}

} // namespace hui::concepts::gallery
