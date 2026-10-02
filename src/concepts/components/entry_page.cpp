// ps5-homebrew-ui - Component Library page: the keyboard and the other entry components.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Everything a screen needs to take text and buttons from a controller. On
// the left a TextField, the Keyboard and a PinEntry; in the middle a
// SearchField over the catalogue and the button that opens an InputPrompt; on
// the right a KeyBinder.
//
// The one design problem of such a screen is that three fields want one
// keyboard, and the keyboard wants the D-pad the fields are reached with. The
// rule here is the one TextField was built for: the D-pad moves between the
// fields; Cross on a field hands the focus to the keyboard, which then types
// into that field; Circle or the Done key hands it back. The field keeps its
// caret meanwhile, so the player sees where the text goes. Triangle on a
// field lets the page type for the player through Keyboard::tap(), which is
// also how the tour types.

#include "concepts/components/page.hpp"

#include "ui/components/input_prompt.hpp"
#include "ui/components/key_binder.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/components/pin_entry.hpp"
#include "ui/components/search_field.hpp"
#include "ui/components/text_field.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout -----------------------------------------------------------------

constexpr Rect kName{96.0f, 290.0f, 800.0f, 92.0f};
constexpr Rect kKeys{96.0f, 398.0f, 800.0f, 330.0f};
constexpr Rect kPin{96.0f, 746.0f, 800.0f, 160.0f};
constexpr Rect kSearch{936.0f, 290.0f, 440.0f, 380.0f};
constexpr Rect kButton{936.0f, 752.0f, 300.0f, 56.0f};
constexpr Rect kBinder{1412.0f, 290.0f, 412.0f, 470.0f};
constexpr float kCaption = 268.0f;
constexpr float kStatus = 930.0f;
constexpr float kPadKey = 100.0f; // one key of the numeric pad

// ---- content ----------------------------------------------------------------

enum Stop
{
    kStopName,
    kStopPin,
    kStopSearch,
    kStopPrompt,
    kStopBinder,
};

enum BindId
{
    kJump = 1,
    kDodge,
    kAttack,
    kInteract,
    kMap,
    kPhoto,
};

constexpr int kNameLimit = 14;
constexpr int kSearchLimit = 24;
constexpr float kTypeEvery = 0.13f; // the page's own typing, one key per tick
constexpr const char *kDemoName = "Marlowe";
constexpr const char *kDemoQuery = "ar";
constexpr const char *kFocusRule =
    "Cross on a field gives the keyboard the focus; Circle gives it back";

constexpr const char *kVariants[] = {
    "Surface keys, plate",
    "Numeric pad, masked PIN",
    "Flat keys, underline",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));
constexpr int kPadVariant = 1; // the PIN is typed on a numeric pad

constexpr ui::Hint kHintsField[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Type"},
    {ui::Button::triangle, "Type for me"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsTyping[] = {
    {ui::Button::dpad, "Keys"},      {ui::Button::cross, "Press"},
    {ui::Button::triangle, "Space"}, {ui::Button::circle, "To the field"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsStop[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsPinEdit[] = {
    {ui::Button::dpad, "Digit, box"},
    {ui::Button::cross, "Next"},
    {ui::Button::circle, "Leave"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsList[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Pick"},
    {ui::Button::circle, "To the field"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsListening[] = {
    {ui::Button::circle, "Keep the old button"},
};
constexpr ui::Hint kHintsPrompt[] = {
    {ui::Button::dpad, "Keys"},
    {ui::Button::cross, "Press"},
    {ui::Button::triangle, "Space"},
    {ui::Button::circle, "Cancel"},
};

constexpr app::TourStep kTour[] = {
    // The page types a name, then a query; a suggestion is picked.
    {0.6f, action_bit(Action::north)},
    {1.3f, 0, Direction::right},
    {0.4f, action_bit(Action::north)},
    {0.9f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.6f, action_bit(Action::confirm), Direction::none, "entry"},
    // The code, spun with the D-pad.
    {0.4f, 0, Direction::left},
    {0.25f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)},
    {0.25f, 0, Direction::up},
    {0.25f, 0, Direction::up},
    {0.6f, action_bit(Action::back), Direction::none, "entry-pin"},
    // The prompt: one letter, then cancel.
    {0.3f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {0.8f, action_bit(Action::confirm)},
    {0.7f, action_bit(Action::back), Direction::none, "entry-prompt"},
    // The numeric pad types the code and it is accepted.
    {0.5f, 0, Direction::left},
    {0.3f, action_bit(Action::west)},
    {0.6f, action_bit(Action::north)},
    {1.6f, action_bit(Action::west), Direction::none, "entry-numeric"},
    // Flat keys; a row of the binder listens and takes Triangle.
    {0.5f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.3f, action_bit(Action::confirm)},
    {1.0f, action_bit(Action::north), Direction::none, "entry-flat"},
    {0.8f, action_bit(Action::west)},
};

const char *button_name(Action action)
{
    switch (action)
    {
    case Action::confirm:
        return "Cross";
    case Action::back:
        return "Circle";
    case Action::north:
        return "Triangle";
    case Action::west:
        return "Square";
    case Action::up:
        return "D-pad up";
    case Action::down:
        return "D-pad down";
    case Action::left:
        return "D-pad left";
    case Action::right:
        return "D-pad right";
    case Action::l3:
        return "L3";
    case Action::r3:
        return "R3";
    default:
        return "nothing";
    }
}

class EntryPage final : public Page
{
  public:
    explicit EntryPage(app::Context &context) : context_(context)
    {
        name_.set_label("Profile name");
        name_.set_placeholder("Cross to type, Triangle to watch");

        search_.set_placeholder("Search the library");
        search_.set_recent({"paper kites", "racing", "tide"});
        // The lambda holds a reference to the catalogue, which outlives the
        // page, and nothing of the page itself.
        const demo::Catalog &catalog = context_.catalog;
        search_.provider = [&catalog](std::string_view query, std::vector<ui::Suggestion> &out)
        {
            for (const demo::Item &item : catalog.items())
            {
                if (contains(item.title, query))
                    out.push_back({item.title, item.genre, 0});
            }
        };

        binder_.set_bindings({
            {kJump, "Jump", Action::confirm, Action::count, false},
            {kDodge, "Dodge", Action::back, Action::count, false},
            {kAttack, "Attack", Action::west, Action::count, false},
            {kInteract, "Interact", Action::north, Action::count, false},
            {kMap, "Open the map", Action::up, Action::count, false},
            {kPhoto, "Photo mode", Action::r3, Action::count, false},
        });

        prompt_.set_title("Name this save");
        prompt_.field.set_placeholder("Untitled");

        restyle(ui::default_theme(), false);
    }

    const char *title() const override
    {
        return "Entry";
    }
    const char *summary() const override
    {
        return "Typing with a pad: Keyboard, PinEntry, SearchField, KeyBinder, InputPrompt";
    }
    const char *variant() const override
    {
        return kVariants[variant_];
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        typing_ = false;
        pin_editing_ = false;
        demo_ = nullptr;
        prompt_.dismiss();
        binder_.stop_listening();
        keyboard_.enter();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        if (prompt_.is_open())
            update_prompt(input, feedback);
        else if (binder_.listening())
            update_binder(input, feedback); // it is waiting for any button
        else if (input.is_pressed(Action::west))
            next_variant(feedback);
        else if (typing_)
            update_typing(input, feedback);
        else
            update_stop(input, feedback);
        run_demo(dt, feedback);
        aim_keyboard();

        // One focus at a time: while the keys have it, the field they feed
        // keeps its caret and gives up its ring.
        const bool keys = typing_ || demo_ != nullptr;
        name_.style.theme.focus.a = keys ? 0.0f : theme_.focus.a;
        search_.style.focus_ring = !keys;
        name_.set_active(stop_ == kStopName);
        pin_.set_active(stop_ == kStopPin);
        search_.set_active(stop_ == kStopSearch);
        binder_.set_active(stop_ == kStopBinder);
        keyboard_.set_active(typing_ || demo_ != nullptr);
        name_.update(dt);
        keyboard_.update(dt);
        pin_.update(dt);
        search_.update(dt);
        binder_.update(dt);
        prompt_.update(dt);
        button_focus_.target = stop_ == kStopPrompt ? 1.0f : 0.0f;
        button_focus_.update(dt, 18.0f);
        button_press_.update(dt, 9.0f);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = reduced_ ? 0.0f : ui::shake(edge_.value, clock_, 10.0f);
        const int column = stop_ == kStopBinder ? 2 : (stop_ >= kStopSearch ? 1 : 0);
        const auto caption = [&](int index, const char *text, const Rect &area)
        {
            const bool active = index == column;
            paint.label(ui::fit_label(paint, text, 20.0f, area.w), area.x + (active ? nudge : 0.0f),
                        kCaption, 20.0f, active ? paint.page_text() : paint.page_text_muted());
        };
        caption(0, "One keyboard for every field", kName);
        caption(1, "Search with suggestions", kSearch);
        caption(2, "Remap: Cross, then a button", kBinder);

        name_.draw(canvas);
        keyboard_.draw(canvas);
        pin_.draw(canvas);
        binder_.draw(canvas);
        const float legend = binder_.bounds().y + binder_.bounds().h + 40.0f;
        paint.body(ui::fit_body(paint,
                                variant_ == kPadVariant ? "A taken button is refused"
                                                        : "A taken button swaps the two rows",
                                20.0f, kBinder.w),
                   kBinder.x, legend, 20.0f, paint.page_text_muted());
        paint.body(ui::fit_body(paint, "A row listens for four seconds", 20.0f, kBinder.w),
                   kBinder.x, legend + 30.0f, 20.0f, paint.page_text_muted());

        paint.label(ui::fit_label(paint, "The overlay most screens want", 20.0f, kSearch.w),
                    kButton.x, kButton.y - 18.0f, 20.0f,
                    stop_ == kStopPrompt ? paint.page_text() : paint.page_text_muted());
        ui::Look look;
        look.focus = button_focus_.value;
        look.press = tween::clamp01(button_press_.value);
        paint.button(kButton, ui::fit_label(paint, "Name a save", 24.0f, kButton.w - 28.0f),
                     ui::ButtonKind::secondary, look);
        if (!answer_.empty())
            paint.body(ui::fit_body(paint, answer_, 21.0f, kSearch.w), kButton.x,
                       kButton.y + kButton.h + 38.0f, 21.0f, paint.page_text_muted());
        // The search field is drawn last in its column: its list floats.
        search_.draw(canvas);

        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, kName.w), kPageArea.x, kStatus, 22.0f,
                       paint.page_text_muted());
        const float right = kPageArea.x + kPageArea.w;
        paint.body(ui::fit_body(paint, kFocusRule, 20.0f, right - kSearch.x), kSearch.x, kStatus,
                   20.0f, paint.page_text_muted());
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        prompt_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (prompt_.is_open())
            return kHintsPrompt;
        if (binder_.listening())
            return kHintsListening;
        if (typing_)
            return kHintsTyping;
        if (pin_editing_)
            return kHintsPinEdit;
        if (stop_ == kStopSearch && search_.in_list())
            return kHintsList;
        if (stop_ == kStopName || (stop_ == kStopSearch && !search_.in_list()) ||
            (stop_ == kStopPin && variant_ == kPadVariant))
            return kHintsField;
        return kHintsStop;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // True if `text` contains `query`, ignoring ASCII case.
    static bool contains(std::string_view text, std::string_view query)
    {
        if (query.empty() || query.size() > text.size())
            return false;
        const auto lower = [](char c)
        { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        for (std::size_t at = 0; at + query.size() <= text.size(); ++at)
        {
            std::size_t same = 0;
            while (same < query.size() && lower(text[at + same]) == lower(query[same]))
                ++same;
            if (same == query.size())
                return true;
        }
        return false;
    }

    const char *code() const
    {
        return variant_ == kPadVariant ? "258025" : "2580";
    }

    // Square: the same components with other knobs. Every style starts from
    // its defaults again, so a variant only names what it changes. Text,
    // digits, bindings and focus are not part of a style and survive.
    void apply_variant()
    {
        ui::TextFieldStyle field;
        ui::KeyboardStyle keys;
        ui::PinEntryStyle pin;
        ui::SearchFieldStyle search;
        ui::KeyBinderStyle binder;
        ui::InputPromptStyle prompt;
        field.max_length = kNameLimit;
        field.on_page = true;
        keys.bindings.space = Action::north; // Square is the page's variant button
        pin.on_page = true;
        search.on_page = true;
        search.keep_open = true;
        search.fill_on_pick = false;
        search.max_length = kSearchLimit;
        search.exits.left = true;
        search.exits.right = true;
        search.exits.down = true;
        binder.on_page = true;
        binder.exits.left = true;
        prompt.max_length = 16;
        ui::KeyboardStyle prompt_keys = keys;
        switch (variant_)
        {
        case kPadVariant:
            keys.highlight.kind = ui::HighlightKind::ring;
            pin.length = 6;
            pin.group = 3;
            pin.masked = true;
            pin.spin = false;
            // The pad that feeds it has the voice.
            pin.enter = audio::Cue::count;
            pin.erase = audio::Cue::count;
            search.pill = true;
            search.highlight.kind = ui::HighlightKind::bar;
            binder.conflict = ui::BindConflict::refuse;
            binder.panel = true;
            binder.dividers = false;
            binder.row_height = 56.0f;
            binder.highlight.kind = ui::HighlightKind::bar;
            prompt_keys.highlight.kind = ui::HighlightKind::ring;
            break;
        case 2:
            keys.surfaces = false;
            keys.panel = false;
            keys.highlight.kind = ui::HighlightKind::underline;
            keys.key_width = 66.0f;
            keys.key_height = 52.0f;
            keys.gap = 6.0f;
            keys.label_size = 27.0f;
            pin.box_width = 54.0f;
            pin.box_height = 64.0f;
            pin.digit_size = 31.0f;
            search.list_panel = false;
            search.row_icons = false;
            search.row_height = 46.0f;
            search.highlight.kind = ui::HighlightKind::underline;
            binder.highlight.kind = ui::HighlightKind::underline;
            binder.dividers = false;
            binder.glyph_size = 34.0f;
            binder.row_height = 54.0f;
            prompt_keys.surfaces = false;
            prompt_keys.highlight.kind = ui::HighlightKind::underline;
            prompt.buttons = false;
            break;
        default:
            break;
        }
        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(field);
        themed(keys);
        themed(pin);
        themed(search);
        themed(binder);
        themed(prompt);
        name_.style = field;
        keyboard_.style = keys;
        pin_.style = pin;
        search_.style = search;
        binder_.style = binder;
        prompt_.style = prompt;
        // The prompt gives its keyboard the theme and the sizes; the look of
        // the keys is ours to choose.
        prompt_.keyboard.style.surfaces = prompt_keys.surfaces;
        prompt_.keyboard.style.highlight = prompt_keys.highlight;
        prompt_.keyboard.style.bindings = prompt_keys.bindings;

        name_.set_bounds({kName.x, kName.y, kName.w, name_.preferred_height()});
        char label[48];
        std::snprintf(label, sizeof(label), "Parental code (it is %s)", code());
        pin_.set_label(label);
        pin_.set_bounds(kPin);
        search_.set_bounds(kSearch);
        binder_.set_bounds({kBinder.x, kBinder.y, kBinder.w, binder_.preferred_height()});
        aim_keyboard();
    }

    // The keyboard follows the field it serves: a numeric pad for the code
    // (in the pad variant), letters for everything else, and the limits of
    // the text it feeds.
    void aim_keyboard()
    {
        const bool pad = variant_ == kPadVariant && stop_ == kStopPin;
        if (pad != pad_)
        {
            pad_ = pad;
            if (pad)
                keyboard_.set_layouts({ui::KeyboardLayout::numeric()});
            else
                keyboard_.set_layouts(
                    {ui::KeyboardLayout::letters(), ui::KeyboardLayout::symbols()});
        }
        if (pad)
        {
            keyboard_.style.key_width = kPadKey;
            const float width = keyboard_.preferred_width();
            keyboard_.set_bounds({kKeys.cx() - width * 0.5f, kKeys.y, width, kKeys.h});
        }
        else
        {
            if (variant_ != 2)
                keyboard_.style.key_width = 0.0f;
            keyboard_.set_bounds(kKeys);
        }
        keyboard_.style.auto_capital = stop_ == kStopName;
        if (stop_ == kStopName)
        {
            keyboard_.style.max_length = kNameLimit;
            keyboard_.set_length(name_.length());
        }
        else if (stop_ == kStopSearch)
        {
            keyboard_.style.max_length = kSearchLimit;
            keyboard_.set_length(search_.length());
        }
        else
        {
            // The code keeps its own count.
            keyboard_.style.max_length = 0;
            keyboard_.set_length(-1);
        }
    }

    void next_variant(ui::Feedback &feedback)
    {
        variant_ = (variant_ + 1) % kVariantCount;
        // A code of another length is another code.
        pin_.reset();
        pin_editing_ = false;
        if (stop_ == kStopPin)
            typing_ = false;
        demo_ = nullptr;
        apply_variant();
        ui::play_cue(feedback, name_.style, name_.style.sounds.change);
    }

    void go(int stop, const Rect &at, ui::Feedback &feedback)
    {
        stop_ = stop;
        demo_ = nullptr;
        ui::play_cue(feedback, name_.style, name_.style.sounds.move, at.cx());
    }

    void edge(const InputFrame &input, const Rect &at, ui::Feedback &feedback)
    {
        ui::refuse(feedback, name_.style, input, edge_, at.cx());
    }

    // What the keyboard sent out goes into the field that has the focus.
    void feed(ui::Feedback &feedback)
    {
        const std::string &typed = keyboard_.typed();
        const int erased = keyboard_.erased();
        if (typed.empty() && erased == 0)
            return;
        if (stop_ == kStopName)
        {
            for (int i = 0; i < erased; ++i)
                name_.backspace();
            name_.insert(typed);
            status_ = "Keyboard  \xC2\xB7  Event::changed  \xC2\xB7  " + name_.text();
        }
        else if (stop_ == kStopSearch)
        {
            for (int i = 0; i < erased; ++i)
                search_.backspace();
            search_.insert(typed);
            status_ = "Keyboard  \xC2\xB7  Event::changed  \xC2\xB7  " + search_.text();
        }
        else if (stop_ == kStopPin)
        {
            for (int i = 0; i < erased; ++i)
                pin_.backspace(feedback);
            if (!typed.empty() && pin_.insert(typed, feedback) == ui::Event::activated)
                verify(feedback);
        }
    }

    void verify(ui::Feedback &feedback)
    {
        if (pin_.value() == code())
        {
            pin_.accept(feedback);
            status_ = "PinEntry  \xC2\xB7  Event::activated  \xC2\xB7  accepted";
            pin_editing_ = false;
            typing_ = false;
        }
        else
        {
            pin_.reject(feedback);
            status_ = "PinEntry  \xC2\xB7  Event::activated  \xC2\xB7  rejected, try again";
        }
    }

    void start_demo(ui::Feedback &feedback)
    {
        if (stop_ == kStopName)
        {
            name_.clear();
            demo_ = kDemoName;
        }
        else if (stop_ == kStopSearch)
        {
            search_.clear();
            demo_ = kDemoQuery;
        }
        else
        {
            pin_.reset();
            demo_ = code();
        }
        demo_timer_ = 0.0f;
        aim_keyboard();
        ui::play_cue(feedback, name_.style, name_.style.sounds.activate);
    }

    // The page as a player: one tap() per tick. The keyboard moves its
    // highlight to the key, dips it and plays the cue, as for a real press.
    void run_demo(float dt, ui::Feedback &feedback)
    {
        if (demo_ == nullptr)
            return;
        demo_timer_ += dt;
        while (demo_ != nullptr && demo_timer_ >= kTypeEvery)
        {
            demo_timer_ -= kTypeEvery;
            if (*demo_ == '\0')
            {
                demo_ = nullptr;
                break;
            }
            const char key[2] = {*demo_++, '\0'};
            aim_keyboard();
            keyboard_.tap(key, feedback);
            feed(feedback);
        }
    }

    void update_typing(const InputFrame &input, ui::Feedback &feedback)
    {
        aim_keyboard();
        const ui::Event event = keyboard_.handle(input, feedback);
        feed(feedback);
        if (event == ui::Event::activated || event == ui::Event::cancelled)
        {
            typing_ = false;
            status_ = event == ui::Event::activated ? "Keyboard  \xC2\xB7  Event::activated (Done)"
                                                    : "Keyboard  \xC2\xB7  Event::cancelled";
        }
        else if (event == ui::Event::refused)
        {
            status_ = "Keyboard  \xC2\xB7  Event::refused";
        }
    }

    void update_stop(const InputFrame &input, ui::Feedback &feedback)
    {
        switch (stop_)
        {
        case kStopName:
            update_name(input, feedback);
            break;
        case kStopPin:
            update_pin(input, feedback);
            break;
        case kStopSearch:
            update_search(input, feedback);
            break;
        case kStopPrompt:
            update_button(input, feedback);
            break;
        default:
            update_binder(input, feedback);
            break;
        }
    }

    void update_name(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.nav == Direction::down)
            go(kStopPin, kPin, feedback);
        else if (input.nav == Direction::right)
            go(kStopSearch, kSearch, feedback);
        else if (input.nav != Direction::none)
            edge(input, kName, feedback);
        else if (input.is_pressed(Action::north))
            start_demo(feedback);
        else if (name_.handle(input, feedback) == ui::Event::activated)
            typing_ = true;
    }

    void update_pin(const InputFrame &input, ui::Feedback &feedback)
    {
        if (pin_editing_)
        {
            // The component has the pad: up and down spin, left and right
            // change box, confirm moves on and submits.
            const ui::Event event = pin_.handle(input, feedback);
            if (event == ui::Event::activated)
                verify(feedback);
            else if (event == ui::Event::cancelled)
                pin_editing_ = false;
            else if (event == ui::Event::changed)
                status_ = "PinEntry  \xC2\xB7  Event::changed  \xC2\xB7  " + pin_.value();
            return;
        }
        if (input.nav == Direction::up)
            go(kStopName, kName, feedback);
        else if (input.nav == Direction::right)
            go(kStopPrompt, kButton, feedback);
        else if (input.nav != Direction::none)
            edge(input, kPin, feedback);
        else if (input.is_pressed(Action::north) && variant_ == kPadVariant)
            start_demo(feedback);
        else if (input.is_pressed(Action::confirm))
        {
            if (pin_.state() == ui::PinState::success)
                pin_.reset();
            if (variant_ == kPadVariant)
                typing_ = true;
            else
                pin_editing_ = true;
            ui::play_cue(feedback, pin_.style, pin_.style.sounds.activate, kPin.x + 140.0f);
        }
    }

    void update_search(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.is_pressed(Action::north) && !search_.in_list())
        {
            start_demo(feedback);
            return;
        }
        const ui::Event event = search_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            if (search_.has_pick())
                status_ = "SearchField  \xC2\xB7  Event::activated  \xC2\xB7  picked " +
                          search_.picked().text;
            else
                typing_ = true;
        }
        else if (event == ui::Event::changed)
        {
            status_ = "SearchField  \xC2\xB7  Event::changed  \xC2\xB7  cleared";
        }
        else if (event == ui::Event::none)
        {
            if (search_.exit() == Direction::left)
                go(kStopName, kName, feedback);
            else if (search_.exit() == Direction::right)
                go(kStopBinder, kBinder, feedback);
            else if (search_.exit() == Direction::down)
                go(kStopPrompt, kButton, feedback);
        }
    }

    void update_button(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.nav == Direction::up)
            go(kStopSearch, kSearch, feedback);
        else if (input.nav == Direction::left)
            go(kStopPin, kPin, feedback);
        else if (input.nav == Direction::right)
            go(kStopBinder, kBinder, feedback);
        else if (input.nav != Direction::none)
            edge(input, kButton, feedback);
        else if (input.is_pressed(Action::confirm))
        {
            button_press_.trigger();
            prompt_.open(feedback, saved_);
        }
    }

    void update_binder(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = binder_.handle(input, feedback);
        if (event == ui::Event::changed)
        {
            const int id = binder_.changed_id();
            if (id == ui::KeyBinder::kResetId)
            {
                status_ = "KeyBinder  \xC2\xB7  Event::changed  \xC2\xB7  defaults restored";
            }
            else
            {
                for (const ui::KeyBinding &row : binder_.bindings())
                {
                    if (row.id == id)
                        status_ = "KeyBinder  \xC2\xB7  Event::changed  \xC2\xB7  " + row.label +
                                  " is on " + button_name(row.action);
                }
            }
        }
        else if (event == ui::Event::activated)
        {
            status_ = "KeyBinder  \xC2\xB7  Event::activated  \xC2\xB7  listening";
        }
        else if (event == ui::Event::refused)
        {
            status_ = "KeyBinder  \xC2\xB7  Event::refused";
        }
        else if (event == ui::Event::none && binder_.exit() == Direction::left)
        {
            go(kStopSearch, kSearch, feedback);
        }
    }

    void update_prompt(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = prompt_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            saved_ = prompt_.text();
            answer_ = "Saved as \"" + saved_ + "\"";
            status_ = "InputPrompt  \xC2\xB7  Event::activated  \xC2\xB7  " + saved_;
        }
        else if (event == ui::Event::cancelled)
        {
            answer_ = "Cancelled: nothing changed";
            status_ = "InputPrompt  \xC2\xB7  Event::cancelled";
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::TextField name_;
    ui::Keyboard keyboard_;
    ui::PinEntry pin_;
    ui::SearchField search_;
    ui::KeyBinder binder_;
    ui::InputPrompt prompt_;
    int stop_ = kStopName;
    int variant_ = 0;
    bool typing_ = false;        // the keyboard has the focus and feeds the stop
    bool pin_editing_ = false;   // the code has the D-pad
    bool pad_ = false;           // the keyboard shows the numeric pad
    const char *demo_ = nullptr; // the rest of what the page is typing
    float demo_timer_ = 0.0f;
    float clock_ = 0.0f;
    tween::Spring button_focus_;
    ui::Pulse button_press_;
    ui::Pulse edge_;
    std::string status_;
    std::string answer_;
    std::string saved_;
};

} // namespace

std::unique_ptr<Page> make_entry_page(app::Context &context)
{
    return std::make_unique<EntryPage>(context);
}

} // namespace hui::concepts::gallery
