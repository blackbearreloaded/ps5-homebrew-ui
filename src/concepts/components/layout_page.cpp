// ps5-homebrew-ui - Component Library page: the layout pieces, as one master-detail screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This page is built only from the layout group, so it doubles as its
// tutorial. Nothing here has a hand-written "if left then ..." in it:
//
//   - every rectangle comes from a layout helper (Row, Column, GridLayout,
//     Wrap, split, inset) and passes through a ui::SpringLayout, so pressing
//     "Re-flow" animates the whole arrangement;
//   - every focusable thing (three buttons, six panels of different sizes,
//     the cards) is one item of a single ui::FocusGroup, which decides where
//     the D-pad leads, owns the gliding ring and remembers the last item of
//     each pane;
//   - a ui::SplitView places the two panes and dims the one without the
//     focus; a ui::ScrollArea scrolls the cards and follows the focus;
//   - ui::Panel, ui::Divider and ui::SectionHeader are the furniture, and a
//     ui::Transition pushes one collection's cards out as the next comes in.
//
// The modal (draw_modal) pushes a focus scope: the D-pad stays inside it and
// the focus returns to the button that opened it. Square changes the knobs.

#include "concepts/components/page.hpp"

#include "ui/components/card.hpp"
#include "ui/components/focus_group.hpp"
#include "ui/components/layout.hpp"
#include "ui/components/overlay.hpp"
#include "ui/components/scroll_area.hpp"
#include "ui/components/split_view.hpp"
#include "ui/components/surface.hpp"
#include "ui/components/transition.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the page's own grid ------------------------------------------------------

constexpr Rect kBar{96.0f, 244.0f, 1728.0f, 56.0f};
constexpr Rect kSplit{96.0f, 324.0f, 1728.0f, 618.0f};
constexpr float kButtonWidth = 196.0f;
constexpr float kGap = 14.0f;       // between the master's panels
constexpr float kHeader = 44.0f;    // a pane's section header
constexpr float kThumbRoom = 16.0f; // beside the scroll view, for its thumb
constexpr Rect kScreen{0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight};
constexpr Rect kModal{620.0f, 330.0f, 680.0f, 392.0f};
// The part of a sample cover without lettering, as a canvas uv (top row last).
constexpr float kCoverBandTop = 0.13f;
constexpr float kCoverBandHeight = 0.6f;
constexpr Rect kCoverBand{0.0f, 1.0f - kCoverBandTop, 1.0f, -kCoverBandHeight};

// Focus scopes: one per region, so the group remembers a place in each.
enum Scope : int
{
    scope_bar,
    scope_master,
    scope_detail,
    scope_modal,
};

// Focus ids.
constexpr int kRatioButton = 0;
constexpr int kReflowButton = 1;
constexpr int kModalButton = 2;
constexpr int kFirstPanel = 10;
constexpr int kFirstCard = 100;
constexpr int kResetButton = 200;
constexpr int kDoneButton = 201;

struct Collection
{
    const char *name;
    const char *note;
    int first; // index into the catalogue
    int count;
};
constexpr Collection kCollections[] = {
    {"Continue", "Where you left off", 0, 8}, {"Favourites", "Starred by you", 3, 11},
    {"Co-op", "Two players or more", 7, 6},   {"Short", "A weekend each", 12, 9},
    {"New", "Added this month", 16, 5},       {"All", "The whole shelf", 0, 14},
};
constexpr int kCollectionCount = static_cast<int>(std::size(kCollections));

struct Variant
{
    const char *name;
    ui::SplitAxis axis;
    ui::DividerLine line;
    ui::PanelKind item_kind;
    ui::HighlightKind highlight;
    bool guides;
    bool panels;
    float third;
    float ratio;
};
constexpr Variant kVariants[] = {
    {"Master-detail", ui::SplitAxis::horizontal, ui::DividerLine::solid, ui::PanelKind::plain,
     ui::HighlightKind::ring, false, false, 0.0f, 0.36f},
    {"Stacked, dashed, wells", ui::SplitAxis::vertical, ui::DividerLine::dashed,
     ui::PanelKind::well, ui::HighlightKind::tint, false, false, 0.0f, 0.34f},
    {"Layout guides", ui::SplitAxis::horizontal, ui::DividerLine::inset, ui::PanelKind::plain,
     ui::HighlightKind::ring, true, false, 0.0f, 0.36f},
    {"Three panes on panels", ui::SplitAxis::horizontal, ui::DividerLine::solid,
     ui::PanelKind::well, ui::HighlightKind::ring, false, true, 0.25f, 0.3f},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr const char *kRatioNames[] = {"regular", "balanced", "collapsed"};
constexpr int kRatioCount = static_cast<int>(std::size(kRatioNames));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::right_stick, "Scroll"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kModalHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kWest = action_bit(Action::west);

constexpr app::TourStep kTour[] = {
    {0.7f, 0, Direction::down},  // Favourites: the cards are pushed out
    {0.5f, 0, Direction::right}, // Co-op, by geometry
    {0.5f, 0, Direction::right}, // across the split, onto the feature card
    {0.4f, 0, Direction::right},
    {0.4f, 0, Direction::down},
    {0.4f, 0, Direction::down}, // off the beam, down to the last card: the area scrolls
    {0.9f, 0, Direction::left, "layout"},
    {0.4f, 0, Direction::up}, // back in the master, where it was; then up
    {0.4f, 0, Direction::up}, // the buttons
    {0.4f, kConfirm},         // ratio: balanced
    {0.6f, 0, Direction::right},
    {0.4f, kConfirm}, // re-flow: every rectangle glides
    {0.6f, 0, Direction::right},
    {0.4f, kConfirm}, // the modal pushes a focus scope
    {0.9f, 0, Direction::left, "layout-modal"},
    {0.4f, kBack},
    {0.5f, kWest},
    {0.9f, kWest, Direction::none, "layout-stacked"},
    {0.9f, kWest, Direction::none, "layout-guides"},
    {0.9f, kWest, Direction::none, "layout-panes"},
    // Put the arrangement back (a variant starts from the regular ratio).
    {0.5f, 0, Direction::left},
    {0.4f, kConfirm},
    {0.5f, 0, Direction::down},
};

class LayoutPage final : public Page
{
  public:
    explicit LayoutPage(app::Context &context) : context_(context)
    {
        focus_.add({kRatioButton, {}, scope_bar});
        focus_.add({kReflowButton, {}, scope_bar});
        focus_.add({kModalButton, {}, scope_bar});
        for (int i = 0; i < kCollectionCount; ++i)
            focus_.add({kFirstPanel + i, {}, scope_master});

        // The first panel is a titled one: its count sits in the title bar's
        // slot, and the page draws a few covers in its content rectangle.
        hero_.header_right = [this](ui::Canvas &canvas, const Rect &area)
        {
            ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
            char text[16];
            std::snprintf(text, sizeof(text), "%d", kCollections[0].count);
            paint.label(text, area.x + area.w, area.cy() + 9.0f, 26.0f, theme_.text_muted,
                        gfx::Align::right);
        };
        modal_panel_.footer = [this](ui::Canvas &canvas, const Rect &)
        {
            ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
            ui::Look look;
            look.press = focus_.focus() == kResetButton ? tween::clamp01(press_.value) : 0.0f;
            paint.button(modal_buttons_[0], "Reset", ui::ButtonKind::secondary, look);
            look.press = focus_.focus() == kDoneButton ? tween::clamp01(press_.value) : 0.0f;
            paint.button(modal_buttons_[1], "Done", ui::ButtonKind::primary, look);
        };
        inspector_.footer = [this](ui::Canvas &canvas, const Rect &area)
        {
            if (cards_.empty())
                return;
            ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
            const demo::Item &title = item_of(current_, inspected_);
            char text[48];
            std::snprintf(text, sizeof(text), "%d h played", title.hours);
            paint.body(text, area.x, area.cy() + 8.0f, 21.0f, theme_.text_muted);
            std::snprintf(text, sizeof(text), "%d", title.year);
            paint.label(text, area.x + area.w, area.cy() + 8.0f, 21.0f, theme_.text,
                        gfx::Align::right);
        };

        restyle(ui::default_theme(), false);
        show_collection(0, Direction::none);
        swap_.show();
        modal_.hide(); // closed until its button opens it
        focus_.set_focus(kFirstPanel);
        place(0.0f, true);
    }

    const char *title() const override
    {
        return "Layout";
    }
    const char *summary() const override
    {
        return "FocusGroup, layout helpers, SplitView, ScrollArea, Panel, Transition";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant(false);
    }

    void enter() override
    {
        if (modal_open_)
            close_modal(true);
        modal_.hide();
        enter_.start(ui::TransitionKind::slide, Direction::up);
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        if (modal_open_)
            update_modal(input, feedback);
        else
            update_screen(input, feedback);

        place(dt, false);
        focus_.update(dt);
        split_.update(dt);
        master_header_.update(dt);
        detail_header_.update(dt);
        enter_.update(dt);
        swap_.update(dt);
        modal_.update(dt);
        press_.update(dt, 8.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const Variant &knobs = kVariants[variant_];

        draw_bar(paint);

        enter_.begin(canvas, kSplit);
        split_.draw(canvas);

        if (!split_.collapsed())
        {
            split_.begin_pane(canvas, 0);
            master_header_.draw(canvas);
            for (int i = 0; i < kCollectionCount; ++i)
                draw_collection(canvas, paint, i, master_springs_.rect(i));
            split_.end_pane(canvas, 0);
        }

        split_.begin_pane(canvas, 1);
        draw_detail(canvas);
        split_.end_pane(canvas, 1);

        if (knobs.third > 0.0f)
            draw_inspector(canvas, paint);
        if (knobs.guides)
            draw_guides(canvas, paint);
        enter_.end(canvas);

        // One highlight for everything on the page. A ring goes on top.
        if (!modal_open_)
            focus_.draw(canvas);
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        if (!modal_.visible())
            return;
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const float shown = modal_.opacity();
        list.rounded_rect(kScreen, 0.0f, Color::rgb(0x000000, 0.5f * shown));

        modal_.begin(canvas, kModal);
        ui::draw_overlay_panel(canvas, theme_, kModal, theme_.style == ui::SurfaceStyle::glass);
        modal_panel_.draw(canvas, kModal);
        const Rect body = modal_panel_.content_rect(kModal);
        float y = body.y + 22.0f;
        for (const std::string &line :
             ui::wrap_body(paint,
                           "push_scope() keeps the D-pad among these two buttons. Closing pops the "
                           "scope, and the focus glides back to the button that opened this.",
                           22.0f, body.w, 4))
        {
            paint.body(line, body.x, y, 22.0f, theme_.text);
            y += 32.0f;
        }
        modal_.end(canvas);
        if (modal_open_)
            focus_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (modal_open_)
            return kModalHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    struct Guide
    {
        Rect rect;
        const char *label;
    };

    bool stacked() const
    {
        return kVariants[variant_].axis == ui::SplitAxis::vertical;
    }

    const demo::Item &item_of(int collection, int index) const
    {
        const Collection &group = kCollections[collection];
        return context_.catalog[static_cast<std::size_t>(group.first + index)];
    }

    // ---- style ---------------------------------------------------------------

    void apply_variant(bool restart)
    {
        const Variant &knobs = kVariants[variant_];
        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(style_);
        themed(focus_.style);
        themed(split_.style);
        themed(scroll_.style);
        themed(hero_.style);
        themed(item_panel_.style);
        themed(inspector_.style);
        themed(modal_panel_.style);
        themed(master_header_.style);
        themed(detail_header_.style);
        themed(guide_.style);
        themed(enter_.style);
        themed(swap_.style);
        themed(modal_.style);

        focus_.style.highlight.kind = knobs.highlight;
        // A tint is a plate: a little larger than the item, its edge shows
        // around a picture too.
        focus_.style.highlight.grow = knobs.highlight == ui::HighlightKind::tint ? 6.0f : 0.0f;

        split_.style.axis = knobs.axis;
        split_.style.line = knobs.line;
        split_.style.panels = knobs.panels;
        split_.style.third = knobs.third;
        split_.style.regular = knobs.ratio;
        split_.style.gap = knobs.panels ? 20.0f : 32.0f;
        split_.style.dim = 0.35f;
        split_.set_bounds(kSplit);

        scroll_.style.axes = stacked() ? ui::ScrollAxes::horizontal : ui::ScrollAxes::vertical;
        scroll_.style.on_panel = knobs.panels;

        hero_.style.kind = ui::PanelKind::titled;
        hero_.style.padding_x = 18.0f;
        hero_.style.padding_y = 14.0f;
        hero_.style.title_size = 24.0f;
        hero_.style.subtitle_size = 18.0f;
        hero_.style.header_slot = 60.0f;
        hero_.style.rule = knobs.line;
        hero_.title = kCollections[0].name;
        hero_.subtitle = kCollections[0].note;
        item_panel_.style.kind = knobs.item_kind;
        item_panel_.style.padding_x = 18.0f;
        item_panel_.style.padding_y = 14.0f;

        // The third pane's panel is the split view's: this one adds only the
        // title bar and the footer.
        inspector_.style.kind = ui::PanelKind::titled;
        inspector_.style.surface = false;
        inspector_.style.padding_x = 0.0f;
        inspector_.style.padding_y = 16.0f;
        inspector_.style.footer_height = 48.0f;
        inspector_.style.accent_bar = true;

        modal_panel_.style.kind = ui::PanelKind::titled;
        modal_panel_.style.surface = false;
        modal_panel_.style.padding_x = 36.0f;
        modal_panel_.style.padding_y = 26.0f;
        modal_panel_.style.title_size = 30.0f;
        modal_panel_.style.subtitle_size = 20.0f;
        modal_panel_.style.footer_height = 104.0f;
        modal_panel_.title = "A focus scope";
        modal_panel_.subtitle = "Drawn in draw_modal(), above the page";

        master_header_.title = "Collections";
        master_header_.set_count(kCollectionCount);
        master_header_.style.on_panel = knobs.panels;
        master_header_.style.line = knobs.line;
        detail_header_.style.on_panel = knobs.panels;
        detail_header_.style.line = knobs.line;
        detail_header_.style.count_kind = ui::Status::primary;
        detail_header_.action = "Scroll";
        detail_header_.action_button = ui::Button::right_stick;

        guide_.style.line = ui::DividerLine::dashed;
        guide_.style.thickness = 2.0f;
        guide_.style.dash = 8.0f;
        guide_.style.dash_gap = 6.0f;
        // In the page's own ink: the one colour every theme makes readable there.
        guide_.style.color =
            ui::Painter(scratch_, context_.fonts, theme_).page_text().with_alpha(0.55f);

        swap_.style.clip = true;
        modal_.style.scale = 0.94f;

        cards_look_ = ui::CardLook{};
        cards_look_.art_aspect = 0.0f;
        cards_look_.text = ui::CardText::over;
        cards_look_.title_size = 22.0f;
        cards_look_.subtitle_size = 18.0f;
        cards_look_.focus_scale = 1.0f; // the focus group's ring is the indicator
        cards_look_.lift = 0.0f;
        cards_look_.ring = false;
        cards_look_.on_panel = knobs.panels;

        if (restart)
        {
            // Another axis is another screen: nothing glides between the two.
            ratio_ = 0;
            split_.set_preset(ui::SplitPreset::regular, true);
            enter_.start(ui::TransitionKind::fade);
            scroll_.scroll_to(0.0f, 0.0f, true);
            place(0.0f, true);
        }
    }

    // ---- input ---------------------------------------------------------------

    void update_screen(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant(true);
            ui::play_cue(feedback, style_, style_.sounds.change);
            status_ = "Square  \xC2\xB7  the same pieces, other knobs";
            return;
        }
        // The right stick scrolls the cards whatever has the focus.
        scroll_.handle(input, feedback);

        const ui::Event event = focus_.handle(input, feedback);
        const int id = focus_.focus();
        if (event == ui::Event::moved)
        {
            on_moved(input.nav);
        }
        else if (event == ui::Event::refused)
        {
            status_ = "Event::refused  \xC2\xB7  nothing lies that way";
        }
        else if (event == ui::Event::activated)
        {
            press_.trigger();
            if (id == kRatioButton)
            {
                ratio_ = (ratio_ + 1) % kRatioCount;
                const ui::SplitPreset presets[kRatioCount] = {ui::SplitPreset::regular,
                                                              ui::SplitPreset::balanced,
                                                              ui::SplitPreset::collapsed};
                split_.set_preset(presets[ratio_]);
                reveal_ = true;
                status_ = std::string("SplitView::set_preset  \xC2\xB7  ") + kRatioNames[ratio_];
            }
            else if (id == kReflowButton)
            {
                reflow_ = !reflow_;
                reveal_ = true;
                status_ = "SpringLayout  \xC2\xB7  every rectangle has a new target";
            }
            else if (id == kModalButton)
            {
                // A layer's items exist while it is open: the group would
                // otherwise find them by geometry like any others.
                modal_open_ = true;
                modal_.start(ui::TransitionKind::scale);
                focus_.add({kResetButton, modal_buttons_[0], scope_modal});
                focus_.add({kDoneButton, modal_buttons_[1], scope_modal});
                focus_.push_scope(scope_modal, kDoneButton);
                ui::play_cue(feedback, style_, style_.sounds.open);
                status_ = "FocusGroup::push_scope  \xC2\xB7  the dialog owns the focus";
            }
            else if (id >= kFirstCard)
            {
                status_ = std::string("Event::activated  \xC2\xB7  ") +
                          item_of(current_, id - kFirstCard).title;
            }
            else
            {
                status_ = std::string("Event::activated  \xC2\xB7  ") +
                          kCollections[id - kFirstPanel].name;
            }
        }
    }

    void on_moved(Direction nav)
    {
        const int id = focus_.focus();
        const int scope = focus_.focus_scope();
        if (scope == scope_master)
        {
            split_.set_focus(0);
            if (id - kFirstPanel != current_)
                show_collection(id - kFirstPanel, nav);
            status_ = std::string("Event::moved  \xC2\xB7  ") + kCollections[current_].name;
        }
        else if (scope == scope_detail)
        {
            split_.set_focus(1);
            inspected_ = id - kFirstCard;
            reveal_ = true;
            status_ = std::string("Event::moved  \xC2\xB7  ") + item_of(current_, inspected_).title;
        }
        else
        {
            // The focus left the split view: neither pane is "the other one".
            split_.set_focus(-1);
            status_ = "Event::moved  \xC2\xB7  the buttons";
        }
    }

    void update_modal(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = focus_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            press_.trigger();
            if (focus_.focus() == kResetButton)
            {
                reflow_ = false;
                ratio_ = 0;
                split_.set_preset(ui::SplitPreset::regular);
                reveal_ = true;
            }
        }
        if (event != ui::Event::activated && event != ui::Event::cancelled)
            return;
        modal_.leave(ui::TransitionKind::scale);
        close_modal(false);
        if (event == ui::Event::activated)
            ui::play_cue(feedback, style_, style_.sounds.close);
        status_ = "FocusGroup::pop_scope  \xC2\xB7  the focus is back where it was";
    }

    void close_modal(bool snap)
    {
        modal_open_ = false;
        focus_.pop_scope(snap);
        focus_.remove(kResetButton);
        focus_.remove(kDoneButton);
    }

    // Another collection: its cards replace the others, pushed along the way
    // the focus moved.
    void show_collection(int index, Direction nav)
    {
        old_cards_ = std::move(cards_);
        old_scroll_x_ = scroll_.offset_x();
        old_scroll_y_ = scroll_.offset_y();
        for (std::size_t i = 0; i < old_cards_.size(); ++i)
            focus_.remove(kFirstCard + static_cast<int>(i));

        previous_ = current_;
        current_ = index;
        inspected_ = 0;
        const Collection &group = kCollections[index];
        cards_.clear();
        for (int i = 0; i < group.count; ++i)
        {
            const demo::Item &title = item_of(index, i);
            ui::CardItem card;
            card.title = title.title;
            card.subtitle = title.genre;
            card.texture = title.cover;
            // The sample covers carry their own lettering; the cards show the
            // band between the name of the studio and the title.
            card.uv = kCoverBand;
            card.image_aspect = 1.0f / kCoverBandHeight;
            card.top = title.mid;
            card.bottom = title.dark;
            card.accent = title.accent;
            cards_.push_back(card);
            ui::FocusItem item{kFirstCard + i, {}, scope_detail};
            item.picture = true;
            focus_.add(item);
        }
        focus_.forget(scope_detail);
        scroll_.scroll_to(0.0f, 0.0f, true);
        snap_cards_ = true;

        detail_header_.title = group.name;
        detail_header_.set_count(group.count);
        // Down a list is up: the content travels against the focus.
        Direction travel = Direction::left;
        if (nav == Direction::down)
            travel = Direction::up;
        else if (nav == Direction::up)
            travel = Direction::down;
        else if (nav == Direction::left)
            travel = Direction::right;
        swap_.start(ui::TransitionKind::push, travel);
    }

    // ---- layout --------------------------------------------------------------

    // Six panels of different sizes. Each arrangement is a few stacks; the
    // focus group needs to know nothing about either.
    std::vector<Rect> master_layout(const Rect &area)
    {
        std::vector<Rect> out(static_cast<std::size_t>(kCollectionCount));
        using Child = ui::LayoutChild;
        const ui::Row row{kGap};
        const ui::Column column{kGap};
        if (!stacked() && !reflow_)
        {
            const Child rows[] = {Child::flexible(1.3f), Child::flexible(), Child::flexible()};
            const Child two[] = {Child::flexible(1.6f), Child::flexible()};
            const Child three[] = {Child::flexible(), Child::flexible(), Child::flexible(1.2f)};
            const std::vector<Rect> lines = column.layout(area, rows);
            out[0] = lines[0];
            row.layout(lines[1], two, std::span<Rect>(out).subspan(1, 2));
            row.layout(lines[2], three, std::span<Rect>(out).subspan(3, 3));
            for (const Rect &line : lines)
                guides_.push_back({line, "Column"});
        }
        else if (!stacked())
        {
            const Child sides[] = {Child::flexible(), Child::flexible(1.15f)};
            const Child left[] = {Child::flexible(1.7f), Child::flexible()};
            const std::vector<Rect> columns = row.layout(area, sides);
            const std::vector<Rect> first = column.layout(columns[0], left);
            const std::vector<Rect> second = column.layout(columns[1], 3);
            const std::vector<Rect> pair = row.layout(second[2], 2);
            out[0] = first[0];
            out[3] = first[1];
            out[1] = second[0];
            out[2] = second[1];
            out[4] = pair[0];
            out[5] = pair[1];
            for (const Rect &side : columns)
                guides_.push_back({side, "Row"});
        }
        else if (!reflow_)
        {
            const Child weights[] = {Child::flexible(1.5f), Child::flexible(1.25f),
                                     Child::flexible(),     Child::flexible(),
                                     Child::flexible(0.9f), Child::flexible(0.8f)};
            row.layout(area, weights, out);
            guides_.push_back({area, "Row"});
        }
        else
        {
            const Child groups[] = {Child::flexible(1.5f), Child::flexible(), Child::flexible(1.2f),
                                    Child::flexible()};
            const std::vector<Rect> cells = row.layout(area, groups);
            const std::vector<Rect> first = column.layout(cells[1], 2);
            const std::vector<Rect> second = column.layout(cells[3], 2);
            out[0] = cells[0];
            out[1] = first[0];
            out[2] = first[1];
            out[3] = cells[2];
            out[4] = second[0];
            out[5] = second[1];
            for (const Rect &cell : cells)
                guides_.push_back({cell, "Row"});
        }
        return out;
    }

    // The cards of a collection, in the scroll area's content coordinates.
    std::vector<Rect> card_layout(const Rect &view, int count) const
    {
        const std::size_t n = static_cast<std::size_t>(count);
        if (!stacked() && !reflow_)
        {
            // A grid whose first card is a feature, two cells each way.
            ui::GridLayout grid;
            grid.columns = std::clamp(static_cast<int>((view.w + 16.0f) / 236.0f), 2, 6);
            grid.cell_aspect = 1.2f;
            std::vector<ui::GridSpan> spans(n);
            if (grid.columns >= 3 && n > 0)
                spans[0] = {2, 2};
            return grid.layout({0.0f, 0.0f, view.w, 0.0f}, spans);
        }
        if (!stacked())
        {
            // A flow of tiles of different widths; every line is filled.
            ui::Wrap wrap;
            wrap.gap_x = kGap;
            wrap.gap_y = kGap;
            wrap.line = ui::LayoutAlign::stretch;
            std::vector<ui::LayoutSize> sizes(n);
            for (std::size_t i = 0; i < n; ++i)
                sizes[i] = {190.0f + 46.0f * static_cast<float>((i * 5 + 1) % 4), 138.0f};
            return wrap.layout({0.0f, 0.0f, view.w, 0.0f}, sizes);
        }
        const ui::Row row{16.0f};
        if (!reflow_)
        {
            // One shelf, wider than the view: the area scrolls sideways.
            const std::vector<ui::LayoutChild> shelf(n, ui::LayoutChild::fixed(view.h * 0.86f));
            return row.layout({0.0f, 0.0f, row.content_size(shelf), view.h}, shelf);
        }
        // Two shelves of tiles of different widths.
        std::vector<ui::LayoutChild> upper;
        std::vector<ui::LayoutChild> lower;
        for (std::size_t i = 0; i < n; ++i)
        {
            const float width = 340.0f + 70.0f * static_cast<float>((i * 2 + 1) % 3);
            (i % 2 == 0 ? upper : lower).push_back(ui::LayoutChild::fixed(width));
        }
        const float width = std::max(row.content_size(upper), row.content_size(lower));
        const std::vector<Rect> lines = ui::Column{kGap}.layout({0.0f, 0.0f, width, view.h}, 2);
        const std::vector<Rect> top = row.layout(lines[0], upper);
        const std::vector<Rect> bottom = row.layout(lines[1], lower);
        std::vector<Rect> out(n);
        for (std::size_t i = 0; i < n; ++i)
            out[i] = (i % 2 == 0 ? top : bottom)[i / 2];
        return out;
    }

    // Where the cards' view is inside the detail pane.
    Rect detail_view(const Rect &pane) const
    {
        const Rect body = ui::split(pane, ui::Side::top, kHeader, 16.0f).rest;
        return stacked() ? ui::inset(body, ui::Edges{0.0f, 0.0f, 0.0f, kThumbRoom})
                         : ui::inset(body, ui::Edges{0.0f, 0.0f, kThumbRoom, 0.0f});
    }

    // Computes every rectangle of the page from its state, lets the springs
    // chase them and tells the focus group where its items are now.
    void place(float dt, bool snap)
    {
        guides_.clear();

        // The buttons: three fixed children, then a spacer that takes the
        // rest (the status line is drawn in it).
        const ui::LayoutChild bar[] = {ui::LayoutChild::fixed(kButtonWidth),
                                       ui::LayoutChild::fixed(kButtonWidth),
                                       ui::LayoutChild::fixed(kButtonWidth), ui::Spacer::flex()};
        ui::Row{kGap}.layout(kBar, bar, bar_rects_);
        for (int i = 0; i < 3; ++i)
            focus_.set_rect(kRatioButton + i, bar_rects_[i]);
        guides_.push_back({kBar, "Row"});

        // The master pane: a header strip, then the panels.
        const bool hidden = split_.collapsed();
        const ui::SplitRects master = ui::split(split_.pane_rect(0), ui::Side::top, kHeader, 12.0f);
        master_header_.set_bounds(master.strip);
        const std::vector<Rect> panels = master_layout(master.rest);
        master_springs_.stagger = reduced_ ? 0.0f : 0.02f;
        if (snap)
            master_springs_.snap(panels);
        else
            master_springs_.target(panels);
        master_springs_.update(dt, style_);
        for (int i = 0; i < kCollectionCount; ++i)
        {
            const Rect r = master_springs_.rect(i);
            focus_.set_rect(kFirstPanel + i, r);
            focus_.set_enabled(kFirstPanel + i, !hidden);
            // The ring takes the panel's corner, not a control's.
            focus_.set_radius(kFirstPanel + i,
                              std::min(theme_.radius_card, std::min(r.w, r.h) * 0.5f));
        }
        guides_.push_back({split_.pane_rect(0), "pane_rect(0)"});

        // The detail pane: a header strip, then the scroll area.
        const Rect pane = split_.pane_rect(1);
        detail_header_.set_bounds(ui::split(pane, ui::Side::top, kHeader).strip);
        const Rect view = detail_view(pane);
        scroll_.set_bounds(view);
        const std::vector<Rect> cards = card_layout(view, static_cast<int>(cards_.size()));
        const Rect content = ui::bounds_of(cards);
        scroll_.set_content_size(content.x + content.w, content.y + content.h);
        if (snap || snap_cards_)
            card_springs_.snap(cards);
        else
            card_springs_.target(cards);
        snap_cards_ = false;
        card_springs_.update(dt, style_);
        // Follow the focused card when it or the layout moved; the stick is
        // free to scroll away from it otherwise.
        const int focused = focus_.focus() - kFirstCard;
        if ((reveal_ || split_.moving()) && focused >= 0 && focused < card_springs_.count())
            scroll_.reveal(card_springs_.target_rect(focused));
        reveal_ = false;
        scroll_.update(dt);
        for (int i = 0; i < card_springs_.count(); ++i)
        {
            const Rect on_screen = scroll_.to_screen(card_springs_.rect(i));
            focus_.set_rect(kFirstCard + i, on_screen);
            focus_.set_clip(kFirstCard + i, view);
            focus_.set_radius(kFirstCard + i, ui::card_radius(style_, cards_look_, on_screen));
        }
        guides_.push_back({pane, "pane_rect(1)"});
        guides_.push_back({view, "ScrollArea"});

        // The modal's two buttons: a row pushed to the right by a spacer.
        const ui::LayoutChild buttons[] = {ui::Spacer::flex(), ui::LayoutChild::fixed(190.0f),
                                           ui::LayoutChild::fixed(190.0f)};
        Rect cells[3];
        ui::Row{16.0f}.layout(ui::inset(modal_panel_.footer_rect(kModal), 0.0f, 22.0f), buttons,
                              cells);
        modal_buttons_[0] = cells[1];
        modal_buttons_[1] = cells[2];
        focus_.set_rect(kResetButton, modal_buttons_[0]);
        focus_.set_rect(kDoneButton, modal_buttons_[1]);
    }

    // ---- drawing -------------------------------------------------------------

    void cover(ui::Canvas &canvas, const demo::Item &title, const Rect &art, float radius) const
    {
        if (title.cover != 0)
            canvas.list.image(title.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
        else
            canvas.list.gradient_rect(art, radius, title.mid, title.dark);
    }

    void draw_bar(ui::Painter &paint) const
    {
        constexpr const char *kLabels[3] = {"Ratio", "Re-flow", "Modal"};
        for (int i = 0; i < 3; ++i)
        {
            ui::Look look;
            if (focus_.focus() == kRatioButton + i)
                look.press = tween::clamp01(press_.value);
            paint.button(bar_rects_[i], kLabels[i], ui::ButtonKind::secondary, look);
        }
        // The spacer's room: what the last input did.
        const Rect room = ui::inset(bar_rects_[3], 12.0f, 0.0f);
        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, room.w), room.x + room.w,
                       room.cy() + 8.0f, 22.0f, paint.page_text_muted(), gfx::Align::right);
    }

    void draw_collection(ui::Canvas &canvas, ui::Painter &paint, int index, const Rect &r) const
    {
        const Collection &group = kCollections[index];
        const bool well = item_panel_.style.kind == ui::PanelKind::well;
        // The collection on show keeps a bar in the accent colour, so the
        // master still says what the detail is while the focus is elsewhere.
        const auto marker = [&]()
        {
            if (index == current_)
                paint.fill({r.x + 7.0f, r.y + 14.0f, 4.0f, std::max(r.h - 28.0f, 0.0f)},
                           theme_.corner == ui::Corner::round ? 2.0f : 0.0f, theme_.accent);
        };
        if (index == 0 && !well && r.h >= 150.0f)
        {
            hero_.draw(canvas, r);
            marker();
            // Its covers flow through the content rectangle: one line where
            // the panel is wide, several where it is narrow and tall.
            const Rect room = hero_.content_rect(r);
            const float size = std::min(room.h, 84.0f);
            if (size < 28.0f)
                return;
            ui::Wrap flow;
            flow.gap_x = 10.0f;
            flow.gap_y = 10.0f;
            const std::vector<ui::LayoutSize> sizes(static_cast<std::size_t>(group.count),
                                                    ui::LayoutSize{size, size});
            const std::vector<Rect> covers = flow.layout(room, sizes);
            for (std::size_t i = 0; i < covers.size(); ++i)
            {
                if (covers[i].y + covers[i].h > room.y + room.h + 0.5f)
                    break; // the lines that do not fit are left out
                cover(canvas, item_of(index, static_cast<int>(i)), covers[i],
                      std::min(theme_.radius, 10.0f));
            }
            return;
        }
        item_panel_.draw(canvas, r);
        marker();
        const Rect room = item_panel_.content_rect(r);
        if (room.w < 30.0f || room.h < 20.0f)
            return;
        const bool tall = room.h >= 76.0f;
        const float baseline = tall ? room.y + 24.0f : room.cy() + 9.0f;
        paint.label(ui::fit_label(paint, group.name, 24.0f, room.w), room.x, baseline, 24.0f,
                    theme_.text);
        if (tall && room.w >= 150.0f)
            paint.body(ui::fit_body(paint, group.note, 19.0f, room.w), room.x, baseline + 28.0f,
                       19.0f, theme_.text_muted);
        if (room.h >= 104.0f)
        {
            char text[24];
            std::snprintf(text, sizeof(text), room.w >= 170.0f ? "%d titles" : "%d", group.count);
            paint.label(text, room.x + room.w, room.y + room.h - 2.0f, 19.0f, theme_.text_muted,
                        gfx::Align::right);
        }
    }

    void draw_cards(ui::Canvas &canvas, const std::vector<ui::CardItem> &cards,
                    const std::vector<Rect> *still, bool live) const
    {
        for (std::size_t i = 0; i < cards.size(); ++i)
        {
            const int index = static_cast<int>(i);
            const Rect r = still != nullptr ? (*still)[i] : card_springs_.rect(index);
            const float visible = scroll_.visibility(r);
            if (visible <= 0.01f)
                continue;
            ui::CardLook look = cards_look_;
            // A small tile has room for a title and nothing else.
            if (r.h < 150.0f || r.w < 200.0f)
                look.subtitle_size = 0.0f;
            ui::CardState state;
            state.focus = live ? focus_.focus_amount(kFirstCard + index) : 0.0f;
            state.marks = false;
            canvas.list.push_opacity(visible);
            ui::draw_card(canvas, style_, look, r, cards[i], state);
            canvas.list.pop_opacity();
        }
    }

    void draw_detail(ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const Rect head = detail_header_.bounds();
        const Rect view = scroll_.bounds();
        const Rect window = scroll_.visible();

        // The header and the cards change together, pushed the same way.
        if (swap_.running())
        {
            ui::SectionHeader old = detail_header_;
            old.title = kCollections[previous_].name;
            old.set_count(kCollections[previous_].count);
            swap_.begin(canvas, head, ui::TransitionPhase::outgoing);
            old.draw(canvas);
            swap_.end(canvas);

            // The old cards keep the scroll position they were left at.
            const std::vector<Rect> rects = card_layout(view, static_cast<int>(old_cards_.size()));
            list.push_clip(view.inset(-10.0f));
            list.push_transform(1.0f, 0.0f, 0.0f, view.x - old_scroll_x_, view.y - old_scroll_y_);
            swap_.begin(canvas, {old_scroll_x_, old_scroll_y_, view.w, view.h},
                        ui::TransitionPhase::outgoing);
            draw_cards(canvas, old_cards_, &rects, false);
            swap_.end(canvas);
            list.pop_transform();
            list.pop_clip();
        }
        swap_.begin(canvas, head);
        detail_header_.draw(canvas);
        swap_.end(canvas);

        scroll_.begin(canvas);
        swap_.begin(canvas, window);
        draw_cards(canvas, cards_, nullptr, true);
        if (kVariants[variant_].guides)
        {
            for (int i = 0; i < card_springs_.count(); ++i)
                outline(canvas, card_springs_.target_rect(i).inset(-4.0f));
        }
        swap_.end(canvas);
        scroll_.end(canvas);
        scroll_.draw(canvas);
    }

    // The third pane: what the focused card is, on the split view's own panel.
    void draw_inspector(ui::Canvas &canvas, ui::Painter &paint) const
    {
        const Rect pane = split_.pane_rect(2);
        if (pane.w < 120.0f || cards_.empty())
            return;
        const demo::Item &title = item_of(current_, inspected_);
        ui::Panel panel = inspector_;
        panel.title = title.title;
        panel.subtitle = title.studio;
        panel.draw(canvas, pane);

        const Rect room = panel.content_rect(pane);
        const Rect art = ui::aspect_fit(ui::split(room, ui::Side::top, room.h * 0.44f).strip, 1.0f);
        cover(canvas, title, {room.x, art.y, art.w, art.h}, std::min(theme_.radius_card, 14.0f));
        float y = art.y + art.h + 36.0f;
        paint.label(ui::fit_label(paint, title.genre, 22.0f, room.w), room.x, y, 22.0f,
                    theme_.text);
        y += 34.0f;
        for (const std::string &line : ui::wrap_body(paint, title.blurb, 20.0f, room.w, 5))
        {
            if (y > room.y + room.h)
                break;
            paint.body(line, room.x, y, 20.0f, theme_.text_muted);
            y += 28.0f;
        }
    }

    // A dashed rectangle from four dividers.
    void outline(ui::Canvas &canvas, const Rect &r) const
    {
        ui::DividerStyle across = guide_.style;
        across.vertical = false;
        ui::DividerStyle down = guide_.style;
        down.vertical = true;
        ui::draw_divider(canvas, across, {r.x, r.y - 1.0f, r.w, 2.0f});
        ui::draw_divider(canvas, across, {r.x, r.y + r.h - 1.0f, r.w, 2.0f});
        ui::draw_divider(canvas, down, {r.x - 1.0f, r.y, 2.0f, r.h});
        ui::draw_divider(canvas, down, {r.x + r.w - 1.0f, r.y, 2.0f, r.h});
    }

    void draw_guides(ui::Canvas &canvas, ui::Painter &paint) const
    {
        for (const Guide &guide : guides_)
        {
            if (guide.rect.w < 24.0f || guide.rect.h < 24.0f)
                continue;
            const Rect r = guide.rect.inset(-6.0f);
            outline(canvas, r);
            // The helper's name on a plate of the page colour, on the line.
            const float width = paint.label_width(guide.label, 16.0f) + 16.0f;
            const Rect plate{r.x + 14.0f, r.y - 11.0f, width, 22.0f};
            canvas.list.rounded_rect(plate, 0.0f,
                                     Color{theme_.page.r, theme_.page.g, theme_.page.b, 1.0f});
            paint.label(guide.label, plate.x + 8.0f, plate.y + 16.0f, 16.0f, paint.page_text());
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the page's own cues and the cards

    ui::FocusGroup focus_;
    ui::SplitView split_;
    ui::ScrollArea scroll_;
    ui::SpringLayout master_springs_;
    ui::SpringLayout card_springs_;
    ui::Panel hero_;
    ui::Panel item_panel_;
    ui::Panel inspector_;
    ui::Panel modal_panel_;
    ui::SectionHeader master_header_;
    ui::SectionHeader detail_header_;
    ui::Divider guide_;
    ui::Transition enter_;
    ui::Transition swap_;
    ui::Transition modal_;
    ui::CardLook cards_look_;

    std::vector<ui::CardItem> cards_;
    std::vector<ui::CardItem> old_cards_;
    std::vector<Guide> guides_;
    Rect bar_rects_[4]{};
    Rect modal_buttons_[2]{};
    float old_scroll_x_ = 0.0f;
    float old_scroll_y_ = 0.0f;

    int variant_ = 0;
    int ratio_ = 0;
    int current_ = 0;  // the collection on show
    int previous_ = 0; // ... and the one it replaced
    int inspected_ = 0;
    bool reflow_ = false;
    bool modal_open_ = false;
    bool reveal_ = false;
    bool snap_cards_ = false;
    float clock_ = 0.0f;
    ui::Pulse press_;
    std::string status_;
    gfx::DrawList scratch_; // a painter needs one, even to ask a colour
};

} // namespace

std::unique_ptr<Page> make_layout_page(app::Context &context)
{
    return std::make_unique<LayoutPage>(context);
}

} // namespace hui::concepts::gallery
