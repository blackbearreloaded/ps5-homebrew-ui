// ps5-homebrew-ui - Design "Editorial": a magazine's weekly selection.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A light, typographic screen: eight numbered features on warm paper, set on
// a strict baseline grid, with a reading view behind each one. It shows that
// a console UI does not have to be dark glass. What makes it feel finished:
//
//   - the focus has no box: the focused row turns full ink while the others
//     rest at a third, an accent bar springs out of the margin, the row steps
//     aside for it and a thin underline wipes in under the title;
//   - the right-hand column (cover, caption, rating, pull quote and a huge
//     faint numeral) cross-fades with a small slide in the direction of travel;
//   - Cross opens the article: the list slides away under a colour plate that
//     wipes in from the edge, the cover travels to its place on the plate and
//     the reading page arrives from the right (one shared element, no cut);
//   - the article is real typesetting: a drop cap, text poured into pairs of
//     columns once at start-up, scrolling a pair at a time so the text always
//     rests on the grid, an end mark, and a hairline that doubles as a scroll
//     bar;
//   - L2 and R2 turn the page with a cross-slide while the plate's colour
//     eases to the next cover; Triangle drops a bookmark ribbon with overshoot;
//   - motion is slow and confident (springs near omega 10, 420 ms wipes) and
//     there is exactly one shadow on the whole screen: a soft one under the
//     cover.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- palette: paper, ink and one accent ------------------------------------
const Color kPaperTop = Color::rgb(0xf3efe6);
const Color kPaperBottom = Color::rgb(0xe9e3d6);
const Color kPaperLight = Color::rgb(0x161410); // added to the paper, so it stays subtle
const Color kInk = Color::rgb(0x15161a);
const Color kAccent = Color::rgb(0xe5432d);   // vermilion
const Color kPlateInk = Color::rgb(0xfbf7ee); // type on the colour plate
const Color kWhite = Color::rgb(0xffffff);

constexpr float kRestAlpha = 0.35f; // rows that are not focused
constexpr float kQuietAlpha = 0.6f; // captions, bylines
constexpr float kRuleAlpha = 0.16f; // hairlines

// ---- the page grid ---------------------------------------------------------
constexpr float kMargin = 96.0f;
constexpr float kRight = gfx::kVirtualWidth - kMargin;
constexpr float kHeavyRule = 60.0f;     // the masthead's thick rule
constexpr float kHeadBaseline = 100.0f; // issue line, running head
constexpr float kHeadRule = 124.0f;     // hairline under it; everything hangs from here
constexpr float kCapsSize = 16.0f;      // small tracked capitals
constexpr float kCapsTracking = 3.5f;

// ---- the list (left column, 55 % of the screen) ----------------------------
constexpr int kFeatures = 8;
constexpr int kVisibleRows = 6;
constexpr float kColumnRight = 1056.0f;
constexpr float kRowPitch = 136.0f;
constexpr float kListHeight = kRowPitch * kVisibleRows;
constexpr float kRowCapTop = 34.0f; // top of the numeral and the title's capitals
constexpr float kRowTitle = 66.0f;  // title baseline, from the row's hairline
constexpr float kRowUnderline = 82.0f;
constexpr float kRowBase = 104.0f; // numeral and small caps share this baseline
constexpr float kNumeralSize = 100.0f;
constexpr float kTitleSize = 48.0f;
constexpr float kTitleX = kMargin + 168.0f;
constexpr float kFocusShift = 20.0f; // the focused row steps aside for the bar
constexpr float kBarWidth = 6.0f;
constexpr float kFootnote = 1016.0f; // small caps on the hint row's centre line
constexpr float kRibbonWidth = 22.0f;
constexpr float kRibbonHeight = 48.0f;
constexpr float kRibbonNotch = 10.0f;

// ---- the spread (right column) ---------------------------------------------
constexpr float kSpreadX = 1152.0f;
constexpr Rect kCoverList{kSpreadX, 156.0f, 480.0f, 480.0f};
constexpr float kGhostSize = 360.0f; // the faint numeral behind the cover
// Height of a display numeral per unit of size: between the flat-topped
// figures (0.70) and the round ones, which overshoot (0.72).
constexpr float kFigureHeight = 0.711f;
constexpr float kQuoteSize = 40.0f;
constexpr float kQuoteLine = 52.0f;

// ---- the article -----------------------------------------------------------
constexpr float kPlateWidth = 656.0f;
constexpr float kPlateNumeralSize = 170.0f;
constexpr float kSafeBottom = 1020.0f;
constexpr Rect kCoverArticle{kMargin, 156.0f, 464.0f, 464.0f};
constexpr float kPageX = 736.0f;
constexpr float kPageWidth = kRight - kPageX;
constexpr float kColumnGap = 64.0f;
constexpr float kColumnWidth = (kPageWidth - kColumnGap) * 0.5f;
constexpr float kHeadline = 224.0f;
constexpr float kHeadlineSize = 88.0f;
constexpr float kStandfirst = 286.0f;
constexpr float kStandfirstSize = 32.0f;
constexpr float kStandfirstLine = 44.0f;
constexpr float kByline = 388.0f;
constexpr float kBodyRule = 412.0f;
constexpr float kBodyTop = 428.0f;
constexpr float kBodySize = 25.0f;
constexpr float kBodyLine = 38.0f;
constexpr float kBodyFirst = 30.0f; // first baseline below kBodyTop
constexpr int kBodyLines = 11;      // lines per column
constexpr float kBodyHeight = kBodyLine * kBodyLines + 8.0f;
// One press scrolls a pair of columns (a leaf). Leaves are stacked two blank
// lines apart, so text in motion shows where one ends and the next begins.
constexpr float kLeafPitch = kBodyLine * (kBodyLines + 2);
constexpr int kDropLines = 3; // lines the drop cap spans
constexpr float kDropSize = 134.0f;
constexpr float kDropGap = 14.0f;
constexpr float kFootRule = 880.0f;
constexpr float kFootBaseline = 930.0f;

// ---- motion: slow and confident --------------------------------------------
constexpr float kFocusOmega = 11.0f;
constexpr float kViewOmega = 10.0f;
constexpr float kCalmOmega = 40.0f; // "Reduce motion": springs become quick fades
constexpr float kWipeSeconds = 0.42f;
constexpr float kChangeSeconds = 0.45f;
constexpr float kTurnSeconds = 0.55f;
constexpr float kSpreadSlide = 30.0f; // vertical travel of the right column
constexpr float kTurnSlide = 150.0f;  // horizontal travel of a page turn
constexpr float kListTravel = 420.0f; // how far the list leaves
constexpr float kPageTravel = 360.0f; // how far the reading page arrives from

// The weekly selection: catalogue indices, chosen for a varied run of colours.
constexpr int kSelection[kFeatures] = {1, 7, 16, 4, 21, 14, 8, 2};

// Placeholder prose. %s is the title, the studio, then the genre and title.
constexpr const char *kOpening =
    "%s is the kind of game that explains itself slowly. The first hour asks very little: a "
    "short walk, a few simple tools and one clear goal on the horizon. By the second evening "
    "the same places feel different, because the game has quietly taught a new way of looking "
    "at them. Nothing is announced. The rules arrive as small surprises, and each one stays "
    "useful until the credits. It is a patient opening, and it earns the patience it asks for.";
constexpr const char *kMiddle =
    "The team at %s has built everything around a steady rhythm. Sessions run about forty "
    "minutes and tend to end on a question rather than a cliffhanger. The sound is sparse and "
    "warm, the screen is rarely busy, and the controls would fit on a single page of a manual. "
    "Even the menus are quiet: one list, one sound, and nothing that blinks for attention. It "
    "is confident work that trusts the player to notice things without being told twice.";
constexpr const char *kClosing =
    "Not every idea lands. A late chapter repeats one trick too often, and the map could say "
    "more about where you have already been. The ending arrives a little early, which is "
    "better than the alternative. These are small complaints about a generous game, and none "
    "of them survive the walk back to the title screen. If %s is a genre you usually skip, "
    "this is a good place to start. If it is one you love, %s will feel like a letter from a "
    "friend who knows you well.";

constexpr const char *kTechniques[] = {
    "A focus without a box: ink weight, a spring-grown accent bar and an eased underline wipe",
    "Strict baseline grid on a light paper backdrop, with one accent and hairline rules",
    "Shared-element transition: the cover travels from the list into the article's colour plate",
    "Typeset article: drop cap, text poured into column pairs once, grid-locked spring scroll",
    "Page turns that cross-slide the text while the plate colour eases (ui::SpringColor)",
    "Bookmark ribbon dropped with an underdamped spring (tween::Bounce) and the mark cue",
};

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::down},
    {0.35f, 0, Direction::down},
    {0.6f, action_bit(Action::north)},
    {1.0f, action_bit(Action::confirm), Direction::none, "bookmark"},
    {1.3f, 0, Direction::down, "article"},
    {1.0f, action_bit(Action::jump_next), Direction::none, "continued"},
    {0.3f, 0, Direction::none, "page-turn"},
    {0.9f, action_bit(Action::back)},
    {0.7f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {1.0f, 0, Direction::none, "list-end"},
};

// One entry of the selection, with every string the screen sets prepared
// once: wrapping text allocates, so it is done at start-up, not per frame.
struct Feature
{
    int item = 0; // catalogue index
    char numeral[4] = "01";
    char drop_cap[2] = "A";
    std::string title;             // fitted to the list's column
    std::string headline;          // fitted to the article's page
    std::string meta;              // GENRE · STUDIO
    std::string caption;           // GENRE · STUDIO · YEAR
    std::string kicker;            // FEATURE 01 · GENRE
    std::string facts;             // 41 HOURS PLAYED · 1 PLAYER
    std::vector<std::string> body; // wrapped lines; an empty one separates paragraphs
    int lead_lines = 0;            // lines indented beside the drop cap
    // The lines poured into columns, in reading order: two columns make a
    // leaf, and the leaves are stacked below each other for scrolling.
    struct Column
    {
        int begin = 0;
        int end = 0;
    };
    std::vector<Column> columns;
    int leaves = 1;
};

// The alpha and the travel (-1..1 of a distance) of one half of a cross-fade.
struct Blend
{
    float alpha = 1.0f;
    float slide = 0.0f;
};

int count_words(std::string_view line)
{
    int words = 0;
    bool inside = false;
    for (char c : line)
    {
        if (c != ' ' && !inside)
            ++words;
        inside = c != ' ';
    }
    return words;
}

std::string_view skip_words(std::string_view text, int words)
{
    std::size_t at = 0;
    for (int i = 0; i < words; ++i)
    {
        while (at < text.size() && text[at] == ' ')
            ++at;
        while (at < text.size() && text[at] != ' ')
            ++at;
    }
    while (at < text.size() && text[at] == ' ')
        ++at;
    return text.substr(at);
}

std::string lower(std::string_view value)
{
    std::string result(value);
    for (char &c : result)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return result;
}

Rect mix_rect(const Rect &a, const Rect &b, float t)
{
    return {tween::lerp(a.x, b.x, t), tween::lerp(a.y, b.y, t), tween::lerp(a.w, b.w, t),
            tween::lerp(a.h, b.h, t)};
}

class Editorial final : public app::Concept
{
  public:
    explicit Editorial(app::Context &context) : context_(context)
    {
        for (int i = 0; i < kFeatures; ++i)
            build_feature(i);
        wipe_.fill(0.0f);
        wipe_[0] = 1.0f;
        lit_[0].snap(1.0f);
        apply_plate(true);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "editorial",
            "Editorial",
            "A magazine's weekly selection: big type on paper, and an article behind every row",
            "src/concepts/editorial.cpp",
            audio::SoundSet::paper,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        // Coming back shows the list assembling again, on the same feature.
        age_ = 0.0f;
        reading_ = false;
        view_.snap(0.0f);
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (reading_)
            update_article(input, feedback);
        else
            update_list(input, feedback);
        if (input.is_pressed(Action::north))
            toggle_bookmark(feedback);

        // ---- animation state ----
        const bool calm = context_.settings.reduced_motion;
        for (int i = 0; i < kFeatures; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            lit_[at].target = i == focus_ ? 1.0f : 0.0f;
            lit_[at].update(dt, calm ? kCalmOmega : kFocusOmega);
            ribbon_[at].target = bookmarked_[at] ? 1.0f : 0.0f;
            // Underdamped on the way down so the ribbon overshoots and settles.
            ribbon_[at].update(dt, calm ? 30.0f : 15.0f, calm ? 1.0f : 0.45f);
        }
        float &wipe = wipe_[static_cast<std::size_t>(focus_)];
        wipe = calm ? 1.0f : std::min(1.0f, wipe + dt / kWipeSeconds);

        const float start = static_cast<float>(focus_) * kRowPitch;
        scroll_.reveal(start, start + kRowPitch, kListHeight, 0.0f,
                       kRowPitch * static_cast<float>(kFeatures));
        scroll_.update(dt, calm ? kCalmOmega : kViewOmega);
        change_.update(dt);
        view_.target = reading_ ? 1.0f : 0.0f;
        view_.update(dt, calm ? kCalmOmega : kViewOmega);
        body_scroll_.update(dt, calm ? kCalmOmega : 12.0f);
        nudge_.update(dt, 7.0f);
        for (ui::SpringColor &colour : plate_)
            colour.update(dt, 6.0f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::paper;
        frame.backdrop.colors[0] = kPaperTop;
        frame.backdrop.colors[1] = kPaperBottom;
        frame.backdrop.colors[2] = kPaperLight;
        frame.backdrop.time = 0.0f; // paper does not move

        gfx::DrawList &list = frame.scene;
        draw_list_screen(list);
        draw_article_screen(list);
        draw_cover(list);
        draw_hints(list);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- content -----------------------------------------------------------

    const Feature &feature(int index) const
    {
        return features_[static_cast<std::size_t>(index)];
    }

    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(feature(index).item)];
    }

    void build_feature(int index)
    {
        Feature &f = features_[static_cast<std::size_t>(index)];
        f.item = kSelection[index];
        const demo::Item &it = context_.catalog[static_cast<std::size_t>(f.item)];
        char text[800];
        std::snprintf(f.numeral, sizeof(f.numeral), "%02d", index + 1);
        const gfx::Font &display = *context_.fonts.display.font;
        f.title = display.fit(it.title, kTitleSize, kColumnRight - kTitleX - 80.0f);
        f.headline = display.fit(it.title, kHeadlineSize, kPageWidth - 80.0f);
        const std::string genre = ui::upper(it.genre);
        const std::string studio = ui::upper(it.studio);
        f.meta = genre + "  \xC2\xB7  " + studio;
        f.caption = f.meta + "  \xC2\xB7  " + std::to_string(it.year);
        f.kicker = std::string("FEATURE ") + f.numeral + "  \xC2\xB7  " + genre;
        if (it.hours > 0)
            std::snprintf(text, sizeof(text), "%d HOURS PLAYED  \xC2\xB7  %s", it.hours,
                          it.players > 1 ? "MULTIPLAYER" : "SOLO");
        else
            std::snprintf(text, sizeof(text), "NOT STARTED");
        f.facts = text;

        // The article. The first paragraph loses its first letter to the drop
        // cap and its first lines are wrapped narrower to make room for it;
        // the rest of the paragraph continues at the full measure.
        const gfx::Font &font = *context_.fonts.regular.font;
        std::snprintf(text, sizeof(text), kOpening, it.title);
        f.drop_cap[0] = text[0];
        const std::string_view lead(text + 1);
        const float indent = context_.fonts.display.measure(f.drop_cap, kDropSize) + kDropGap;
        int words = 0;
        for (const std::string &line : font.wrap(lead, kBodySize, kColumnWidth - indent))
        {
            if (f.lead_lines == kDropLines)
                break;
            words += count_words(line);
            f.body.push_back(line);
            ++f.lead_lines;
        }
        append_paragraph(f, skip_words(lead, words), false);
        std::snprintf(text, sizeof(text), kMiddle, it.studio);
        append_paragraph(f, text, true);
        std::snprintf(text, sizeof(text), kClosing, lower(it.genre).c_str(), it.title);
        append_paragraph(f, text, true);

        // Pour the lines into columns. The text reads down the left column,
        // then down the right one, then continues on the next leaf: the
        // reader never has to scroll back up. A column never opens on a
        // blank line, and the last leaf is balanced.
        const int count = static_cast<int>(f.body.size());
        int at = 0;
        while (at < count)
        {
            if (f.body[static_cast<std::size_t>(at)].empty())
            {
                ++at;
                continue;
            }
            const int remaining = count - at;
            int take = kBodyLines;
            if (f.columns.size() % 2 == 0 && remaining <= 2 * kBodyLines)
                take = std::max((remaining + 1) / 2, f.columns.empty() ? f.lead_lines : 1);
            take = std::min({take, remaining, kBodyLines});
            // Widows and orphans: a paragraph's last line does not start a
            // column alone, and its first line does not end one alone. The
            // column is left a line short instead.
            const auto blank = [&](int line)
            { return line >= count || f.body[static_cast<std::size_t>(line)].empty(); };
            if (take > 2 && !blank(at + take) && blank(at + take + 1))
                --take;
            if (take > 2 && !blank(at + take) && blank(at + take - 2))
                take -= 2;
            f.columns.push_back({at, at + take});
            at += take;
        }
        f.leaves = std::max(1, (static_cast<int>(f.columns.size()) + 1) / 2);
    }

    void append_paragraph(Feature &f, std::string_view text, bool spaced) const
    {
        if (text.empty())
            return;
        if (spaced)
            f.body.emplace_back();
        for (std::string &line : context_.fonts.regular.font->wrap(text, kBodySize, kColumnWidth))
            f.body.push_back(std::move(line));
    }

    float max_scroll(int index) const
    {
        return static_cast<float>(feature(index).leaves - 1) * kLeafPitch;
    }

    // The plate takes the colours of the cover that sits on it.
    void apply_plate(bool snap)
    {
        const demo::Item &it = item(focus_);
        const Color targets[2] = {gfx::mix(it.mid, it.dark, 0.12f),
                                  gfx::mix(it.dark, it.mid, 0.22f)};
        for (int i = 0; i < 2; ++i)
        {
            if (snap)
                plate_[i].snap(targets[i]);
            else
                plate_[i].target(targets[i]);
        }
    }

    // ---- input -------------------------------------------------------------

    // Moves to another feature. `turn` is a page turn inside the article
    // (content crosses sideways); otherwise the list moved (it crosses
    // vertically). Both use one timer, so the two views can never disagree
    // about which feature is leaving.
    void go_to(int next, bool turn)
    {
        previous_ = focus_;
        direction_ = next > focus_ ? 1.0f : -1.0f;
        focus_ = next;
        turn_ = turn;
        wipe_[static_cast<std::size_t>(next)] = 0.0f;
        const bool calm = context_.settings.reduced_motion;
        change_.start(calm ? 0.12f : (turn ? kTurnSeconds : kChangeSeconds));
        leaving_scroll_ = body_scroll_.value;
        body_scroll_.snap(0.0f);
        apply_plate(false);
    }

    void refuse(app::Feedback &feedback, float x, float y)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_x_ = x;
        nudge_y_ = y;
    }

    void update_list(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int step = input.nav == Direction::down ? 1 : -1;
            const int next = focus_ + step;
            if (next >= 0 && next < kFeatures)
            {
                go_to(next, false);
                // Rows further down the page sound a little lower.
                feedback.play(audio::Cue::focus, 1.08f - 0.02f * static_cast<float>(next),
                              ui::pan_for_x(kTitleX));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, static_cast<float>(step));
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            reading_ = true;
            body_scroll_.snap(0.0f);
            feedback.play(audio::Cue::select);
        }
    }

    void update_article(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            // The text moves one leaf (a whole number of lines), so it always
            // comes to rest on the baseline grid with both columns full.
            const float step = input.nav == Direction::down ? 1.0f : -1.0f;
            const float limit = max_scroll(focus_);
            const float wanted = std::clamp(body_scroll_.target + step * kLeafPitch, 0.0f, limit);
            if (std::fabs(wanted - body_scroll_.target) > 0.5f)
            {
                body_scroll_.target = wanted;
                // Further down the article sounds a little lower.
                feedback.play(audio::Cue::slide, 1.05f - 0.1f * wanted / limit,
                              ui::pan_for_x(kPageX + kPageWidth * 0.5f));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, step);
            }
        }
        const bool earlier = input.is_pressed(Action::jump_prev);
        if (earlier || input.is_pressed(Action::jump_next))
        {
            const int step = earlier ? -1 : 1;
            const int next = focus_ + step;
            if (next >= 0 && next < kFeatures)
            {
                go_to(next, true);
                // The page is heard leaving toward the side it turns to.
                feedback.play(audio::Cue::flip, 1.0f, 0.4f * static_cast<float>(step));
            }
            else
            {
                refuse(feedback, static_cast<float>(step), 0.0f);
            }
        }
        if (input.is_pressed(Action::back))
        {
            reading_ = false;
            feedback.play(audio::Cue::back);
        }
    }

    void toggle_bookmark(app::Feedback &feedback)
    {
        const std::size_t at = static_cast<std::size_t>(focus_);
        bookmarked_[at] = !bookmarked_[at];
        const float pan = ui::pan_for_x(reading_ ? kRight : kColumnRight);
        feedback.play(bookmarked_[at] ? audio::Cue::mark : audio::Cue::erase, 1.0f, pan);
    }

    // ---- motion helpers ----------------------------------------------------

    // Distances collapse to zero under "Reduce motion": only the fades remain.
    float travel(float distance) const
    {
        return context_.settings.reduced_motion ? 0.0f : distance;
    }

    // The old content leaves quickly; the new one arrives a beat later.
    Blend leaving() const
    {
        if (!change_.running)
            return {0.0f, 0.0f};
        const float t = tween::clamp01(change_.progress() * 3.0f);
        return {1.0f - tween::smoothstep(t), -direction_ * tween::cubic_in(t)};
    }

    Blend arriving() const
    {
        if (!change_.running)
            return {1.0f, 0.0f};
        // The fade finishes before the slide does, so the last part of the
        // travel is seen at full strength: the content visibly settles.
        const float t = tween::clamp01((change_.progress() - 0.25f) / 0.75f);
        return {tween::smoothstep(t * 1.6f), direction_ * (1.0f - tween::cubic_out(t))};
    }

    // A refusal pushes the content a few pixels the way the player pressed
    // and lets it come back: out and home again as the pulse decays.
    float bump(float amplitude) const
    {
        return std::sin(nudge_.value * 3.14159265f) * travel(amplitude);
    }

    float view() const
    {
        return tween::clamp01(view_.value);
    }

    // ---- drawing: shared pieces --------------------------------------------

    void caps(gfx::DrawList &list, std::string_view value, float x, float baseline, Color color,
              gfx::Align align = gfx::Align::left, float size = kCapsSize) const
    {
        ui::text(list, context_.fonts.semibold, value, x, baseline, size, color, align,
                 kCapsTracking);
    }

    static void rule(gfx::DrawList &list, float x, float y, float width, Color color,
                     float thickness = 1.0f)
    {
        if (width > 0.0f)
            list.rounded_rect({x, y, width, thickness}, 0, color);
    }

    // Five stars with a fractional fill: the faint row is drawn whole and
    // the accent row is clipped to the rating.
    static void stars(gfx::DrawList &list, float x, float cy, float rating, Color on, Color off)
    {
        constexpr float kRadius = 9.0f;
        constexpr float kStep = 24.0f;
        for (int i = 0; i < 5; ++i)
            list.star(x + kRadius + static_cast<float>(i) * kStep, cy, kRadius, off);
        const float whole = std::floor(rating);
        const float filled = whole * kStep + (rating - whole) * kRadius * 2.0f;
        list.push_clip({x, cy - kRadius - 2.0f, filled, kRadius * 2.0f + 4.0f});
        for (int i = 0; i < 5; ++i)
            list.star(x + kRadius + static_cast<float>(i) * kStep, cy, kRadius, on);
        list.pop_clip();
    }

    // A bookmark hanging from a rule. `drop` is the spring's value: it grows
    // past 1 and settles, which is the whole animation.
    static void ribbon(gfx::DrawList &list, float x, float y, float drop, Color color)
    {
        const float height = kRibbonHeight * std::max(0.0f, drop);
        if (height < 1.0f)
            return;
        const float notch = std::min(kRibbonNotch, height * 0.5f);
        const float xy[] = {x,
                            y,
                            x + kRibbonWidth,
                            y,
                            x + kRibbonWidth,
                            y + height,
                            x + kRibbonWidth * 0.5f,
                            y + height - notch,
                            x,
                            y + height};
        list.polygon(xy, 5, color);
        // Polygon edges are not anti-aliased; the two slanted ones are
        // stroked with a thin line (which is) to hide the steps.
        list.line(xy[4] - 1.0f, xy[5] - 1.0f, xy[6], xy[7], 1.5f, color);
        list.line(xy[8] + 1.0f, xy[9] - 1.0f, xy[6], xy[7], 1.5f, color);
    }

    // ---- drawing: the list -------------------------------------------------

    void draw_list_screen(gfx::DrawList &list) const
    {
        const float t = view();
        if (t >= 0.996f)
            return;
        // Leaving for the article: the whole spread slides left and fades.
        list.push_opacity(1.0f - tween::smoothstep(t * 1.9f));
        list.push_transform(1.0f, 0, 0, -travel(kListTravel) * t, 0);
        draw_masthead(list);
        draw_rows(list);
        const float in = tween::stagger(age_, 6, 0.06f, 0.7f);
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, travel(24.0f) * (1.0f - in));
        draw_spread(list, previous_, leaving());
        draw_spread(list, focus_, arriving());
        list.pop_transform();
        list.pop_opacity();
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_masthead(gfx::DrawList &list) const
    {
        // The heavy rule draws itself across the page, then the type appears.
        const float drawn = tween::quint_out(tween::clamp01(age_ / 0.8f));
        rule(list, kMargin, kHeavyRule, (kRight - kMargin) * drawn, kInk, 4.0f);
        const float in = tween::stagger(age_, 1, 0.06f, 0.6f);
        list.push_opacity(in);
        caps(list, "ISSUE 14  \xC2\xB7  THE WEEKLY SELECTION", kMargin, kHeadBaseline, kInk,
             gfx::Align::left, 17.0f);
        // The folio counts with the focus; the two numbers cross-fade.
        char text[16];
        const Blend out = leaving();
        if (out.alpha > 0.01f)
        {
            std::snprintf(text, sizeof(text), "%s / %02d", feature(previous_).numeral, kFeatures);
            caps(list, text, kRight, kHeadBaseline, kInk.with_alpha(kQuietAlpha * out.alpha),
                 gfx::Align::right, 17.0f);
        }
        std::snprintf(text, sizeof(text), "%s / %02d", feature(focus_).numeral, kFeatures);
        caps(list, text, kRight, kHeadBaseline, kInk.with_alpha(kQuietAlpha * arriving().alpha),
             gfx::Align::right, 17.0f);
        list.pop_opacity();
    }

    void draw_rows(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float offset = scroll_.offset();
        const float bottom = kHeadRule + kListHeight;
        const float idle =
            context_.settings.reduced_motion ? 0.0f : 0.18f * ui::breathe(clock_, 3.2f);
        list.push_clip({0, kHeadRule, kColumnRight + 24.0f, kListHeight + 1.0f});
        for (int i = 0; i < kFeatures; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            const float top = kHeadRule + static_cast<float>(i) * kRowPitch - offset;
            if (top + kRowPitch <= kHeadRule || top >= bottom)
                continue;
            // Rows crossing the edge of the list fade instead of being cut.
            const float inside = std::min(top + kRowPitch - kHeadRule, bottom - top) / kRowPitch;
            const float in = tween::stagger(age_, 2 + i, 0.06f, 0.6f);
            const float lit = tween::clamp01(lit_[at].value);
            const float ink = tween::lerp(kRestAlpha, 1.0f, lit);
            const float y =
                top + travel(18.0f) * (1.0f - in) + (i == focus_ ? nudge_y_ * bump(10.0f) : 0.0f);
            const float shift = kFocusShift * lit;
            const Feature &f = feature(i);

            list.push_opacity(in * tween::smoothstep(inside));
            rule(list, kMargin, top, kColumnRight - kMargin, kInk.with_alpha(kRuleAlpha));
            // The accent bar grows out of the margin; the row steps aside.
            // It is the one thing on the page that is alive: it breathes.
            if (lit > 0.01f)
                list.rounded_rect({kMargin, y + kRowCapTop, kBarWidth * lit, kRowBase - kRowCapTop},
                                  0, kAccent.with_alpha(1.0f - idle));
            ui::text(list, fonts.display, f.numeral, kMargin - 4.0f + shift, y + kRowBase,
                     kNumeralSize, kInk.with_alpha(ink));
            const float width = ui::text(list, fonts.display, f.title, kTitleX + shift,
                                         y + kRowTitle, kTitleSize, kInk.with_alpha(ink));
            // The underline wipes in from the left on the focused row and
            // fades with the row's light when the focus moves on.
            const float wipe = tween::cubic_in_out(wipe_[at]);
            rule(list, kTitleX + shift, y + kRowUnderline, width * wipe, kAccent.with_alpha(lit),
                 2.0f);
            caps(list, f.meta, kTitleX + shift, y + kRowBase,
                 kInk.with_alpha(tween::lerp(kRestAlpha, kQuietAlpha, lit)));
            ribbon(list, kColumnRight - kRibbonWidth - 12.0f, top, ribbon_[at].value,
                   kAccent.with_alpha(tween::lerp(0.6f, 1.0f, lit)));
            list.pop_opacity();
        }
        list.pop_clip();

        // The closing rule, and where the list continues: two notes on the
        // hint row's line that fade with the scroll position.
        const float in = tween::stagger(age_, 8, 0.06f, 0.6f);
        const float limit = kRowPitch * static_cast<float>(kFeatures - kVisibleRows);
        list.push_opacity(in);
        rule(list, kMargin, bottom, kColumnRight - kMargin, kInk.with_alpha(kRuleAlpha));
        caps(list, "CONTINUES  \xE2\x86\x93", kMargin, kFootnote,
             kInk.with_alpha(kQuietAlpha * tween::clamp01((limit - offset) / kRowPitch)));
        caps(list, "\xE2\x86\x91  EARLIER", kColumnRight, kFootnote,
             kInk.with_alpha(kQuietAlpha * tween::clamp01(offset / kRowPitch)), gfx::Align::right);
        list.pop_opacity();
    }

    // The right column's type for one feature, at a cross-fade position.
    void draw_spread(gfx::DrawList &list, int index, Blend blend) const
    {
        if (blend.alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Feature &f = feature(index);
        const demo::Item &it = item(index);
        const float dy = turn_ ? 0.0f : blend.slide * travel(kSpreadSlide);
        const float below = kCoverList.y + kCoverList.h;
        char text[16];
        list.push_opacity(blend.alpha);

        // The faint numeral: a graphic element half hidden by the cover, its
        // top on the cover's top edge. It travels further than the text.
        ui::text(list, fonts.display, f.numeral, kRight + 14.0f,
                 kCoverList.y + kGhostSize * kFigureHeight + dy * 2.5f, kGhostSize,
                 kInk.with_alpha(0.07f), gfx::Align::right);

        caps(list, f.caption, kSpreadX, below + 40.0f + dy, kInk.with_alpha(kQuietAlpha));
        stars(list, kSpreadX, below + 70.0f + dy, it.rating, kAccent, kInk.with_alpha(0.16f));
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
        const float x = kSpreadX + 132.0f;
        const float w = ui::text(list, fonts.semibold, text, x, below + 77.0f + dy, 20, kInk);
        caps(list, f.facts, x + w + 20.0f, below + 76.0f + dy, kInk.with_alpha(kQuietAlpha));

        // The pull quote lands last and from a little further away.
        rule(list, kSpreadX, below + 112.0f + dy * 1.5f, 56.0f, kAccent, 4.0f);
        ui::paragraph(list, fonts.regular, it.blurb, kSpreadX, below + 172.0f + dy * 1.5f,
                      kQuoteSize, kRight - kSpreadX, kQuoteLine, kInk, 3);
        list.pop_opacity();
    }

    // ---- drawing: the cover, shared by both views ---------------------------

    // One cover, drawn once, on top of both views: its rectangle is blended
    // between its place in the list and its place on the plate, so opening
    // an article moves the picture instead of replacing it.
    void draw_cover(gfx::DrawList &list) const
    {
        const float in = tween::stagger(age_, 4, 0.06f, 0.7f);
        Rect rect = mix_rect(kCoverList, kCoverArticle, view());
        rect.y += travel(24.0f) * (1.0f - in);
        list.push_opacity(in);
        // The only shadow on the screen: wide, soft and faint.
        list.shadow({rect.x + 8.0f, rect.y + 26.0f, rect.w - 16.0f, rect.h - 8.0f}, 8, 48,
                    Color::rgb(0x2a2014, 0.26f));
        // The old cover stays opaque underneath while the new one fades in
        // over it: paper never shows through in the middle of the change.
        // The new one also slides, but inside the frame: the picture changes,
        // the frame does not move.
        if (change_.running)
        {
            const float t = change_.progress();
            const float slide =
                direction_ * (1.0f - tween::quint_out(t)) * travel(turn_ ? 72.0f : 40.0f);
            list.image(item(previous_).cover, rect, gfx::kCanvasUv, kWhite, 4);
            list.push_clip(rect);
            list.image(
                item(focus_).cover,
                {rect.x + (turn_ ? slide : 0.0f), rect.y + (turn_ ? 0.0f : slide), rect.w, rect.h},
                gfx::kCanvasUv, kWhite.with_alpha(tween::smoothstep(t * 1.5f)), 4);
            list.pop_clip();
        }
        else
        {
            list.image(item(focus_).cover, rect, gfx::kCanvasUv, kWhite, 4);
        }
        list.bordered_rect(rect, 4, Color::rgb(0x000000, 0.0f), 1, kInk.with_alpha(0.12f));
        list.pop_opacity();
    }

    // ---- drawing: the article ----------------------------------------------

    void draw_article_screen(gfx::DrawList &list) const
    {
        const float t = view();
        if (t <= 0.004f)
            return;
        // The plate wipes in from the screen edge, over the departing list.
        const float width = kPlateWidth * t;
        list.gradient_rect({0, 0, width, gfx::kVirtualHeight}, 0, plate_[0].value(),
                           plate_[1].value());
        list.push_clip({0, 0, width, gfx::kVirtualHeight});
        draw_plate_type(list, previous_, leaving().alpha * t);
        draw_plate_type(list, focus_, arriving().alpha * t);
        list.pop_clip();

        // The reading page arrives from the right, a beat behind the plate.
        list.push_opacity(tween::smoothstep((t - 0.4f) / 0.6f));
        list.push_transform(1.0f, 0, 0, travel(kPageTravel) * (1.0f - t), 0);
        rule(list, kPageX, kHeadRule, kPageWidth, kInk.with_alpha(kRuleAlpha));
        rule(list, kPageX, kBodyRule, kPageWidth, kInk.with_alpha(kRuleAlpha));
        rule(list, kPageX, kFootRule, kPageWidth, kInk.with_alpha(kRuleAlpha));
        if (change_.running)
            draw_page(list, previous_, leaving(), leaving_scroll_);
        draw_page(list, focus_, arriving(), body_scroll_.value);
        list.pop_transform();
        list.pop_opacity();
    }

    // What is printed on the colour plate under the cover.
    void draw_plate_type(gfx::DrawList &list, int index, float alpha) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Feature &f = feature(index);
        const demo::Item &it = item(index);
        const float left = kCoverArticle.x;
        const float right = kCoverArticle.x + kCoverArticle.w;
        const float below = kCoverArticle.y + kCoverArticle.h;
        char text[32];
        list.push_opacity(alpha);
        caps(list, f.caption, left, below + 44.0f, kPlateInk.with_alpha(0.8f), gfx::Align::left,
             15.0f);

        // A three-line fact table, ruled like a contents page.
        const char *labels[3] = {"RATING", "PLAYED", "PLAYERS"};
        for (int i = 0; i < 3; ++i)
        {
            const float y = below + 76.0f + static_cast<float>(i) * 52.0f;
            rule(list, left, y, right - left, kPlateInk.with_alpha(0.22f));
            caps(list, labels[i], left, y + 34.0f, kPlateInk.with_alpha(0.65f), gfx::Align::left,
                 15.0f);
            if (i == 0)
                std::snprintf(text, sizeof(text), "%.1f / 5", static_cast<double>(it.rating));
            else if (i == 1)
                std::snprintf(text, sizeof(text), "%d hours", it.hours);
            else if (it.players > 1)
                std::snprintf(text, sizeof(text), "1\xE2\x80\x93%d", it.players);
            else
                std::snprintf(text, sizeof(text), "1");
            ui::text(list, fonts.semibold, text, right, y + 35.0f, 24, kPlateInk,
                     gfx::Align::right);
        }
        rule(list, left, below + 76.0f + 156.0f, right - left, kPlateInk.with_alpha(0.22f));
        // The feature's number stands on the bottom of the safe area.
        ui::text(list, fonts.display, f.numeral, left - 8.0f, kSafeBottom, kPlateNumeralSize,
                 kPlateInk.with_alpha(0.2f));
        list.pop_opacity();
    }

    // One article's type at a cross-slide position.
    void draw_page(gfx::DrawList &list, int index, Blend blend, float scroll) const
    {
        if (blend.alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Feature &f = feature(index);
        const demo::Item &it = item(index);
        const bool current = index == focus_;
        const float x = kPageX + (turn_ ? blend.slide * travel(kTurnSlide) : 0.0f) +
                        (current ? nudge_x_ * bump(14.0f) : 0.0f);
        char text[64];
        list.push_opacity(blend.alpha);

        // Running head: kicker on the left, folio on the right.
        caps(list, f.kicker, x, kHeadBaseline, kAccent, gfx::Align::left, 17.0f);
        std::snprintf(text, sizeof(text), "%s / %02d", f.numeral, kFeatures);
        caps(list, text, x + kPageWidth, kHeadBaseline, kInk.with_alpha(kQuietAlpha),
             gfx::Align::right, 17.0f);
        ribbon(list, x + kPageWidth - kRibbonWidth, kHeadRule,
               ribbon_[static_cast<std::size_t>(index)].value, kAccent);

        ui::text(list, fonts.display, f.headline, x - 5.0f, kHeadline, kHeadlineSize, kInk);
        ui::paragraph(list, fonts.regular, it.blurb, x, kStandfirst, kStandfirstSize, 700.0f,
                      kStandfirstLine, kInk.with_alpha(0.8f), 2);
        std::snprintf(text, sizeof(text), "WORDS  THE WEEKLY DESK  \xC2\xB7  %d", it.year);
        caps(list, text, x, kByline, kInk.with_alpha(kQuietAlpha), gfx::Align::left, 15.0f);

        draw_body(list, f, x, current ? scroll + nudge_y_ * bump(10.0f) : scroll);

        // The foot rule is also the scroll bar: an accent run that shows
        // which part of the columns is on the page.
        const float limit = max_scroll(index);
        if (limit > 0.0f)
        {
            const float shown = 1.0f / static_cast<float>(f.leaves);
            const float at = tween::clamp01(scroll / limit) * (1.0f - shown);
            rule(list, x + kPageWidth * at, kFootRule - 1.0f, kPageWidth * shown, kAccent, 3.0f);
        }
        if (index + 1 < kFeatures)
        {
            caps(list, "NEXT FEATURE", x, kFootBaseline, kAccent);
            ui::text(list, fonts.display, item(index + 1).title, x + 196.0f, kFootBaseline + 2.0f,
                     30, kInk);
            ui::text(list, fonts.display, feature(index + 1).numeral, x + kPageWidth,
                     kFootBaseline + 2.0f, 30, kInk.with_alpha(kRestAlpha), gfx::Align::right);
        }
        else
        {
            caps(list, "END OF ISSUE 14", x, kFootBaseline, kAccent);
            ui::text(list, fonts.display, "Back to the selection", x + 228.0f, kFootBaseline + 2.0f,
                     30, kInk);
        }
        list.pop_opacity();
    }

    // Two columns of body text behind one clip rectangle. Lines near the
    // edges fade, so a scroll never shows a line cut in half.
    void draw_body(gfx::DrawList &list, const Feature &f, float x, float scroll) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float first = kBodyTop + kBodyFirst;
        const float last = first + kBodyLine * static_cast<float>(kBodyLines - 1);
        const auto fade = [&](float baseline)
        {
            return tween::clamp01((baseline - kBodyTop) / kBodyFirst) *
                   tween::clamp01((last + kBodyLine - baseline) / kBodyLine);
        };
        const float indent = fonts.display.measure(f.drop_cap, kDropSize) + kDropGap;
        list.push_clip({x - 12.0f, kBodyTop, kPageWidth + 24.0f, kBodyHeight});
        const int columns = static_cast<int>(f.columns.size());
        for (int c = 0; c < columns; ++c)
        {
            const Feature::Column &column = f.columns[static_cast<std::size_t>(c)];
            const float cx = x + static_cast<float>(c % 2) * (kColumnWidth + kColumnGap);
            const float top = first + static_cast<float>(c / 2) * kLeafPitch - scroll;
            for (int i = column.begin; i < column.end; ++i)
            {
                const std::string &line = f.body[static_cast<std::size_t>(i)];
                const float baseline = top + static_cast<float>(i - column.begin) * kBodyLine;
                const float alpha = fade(baseline);
                if (line.empty() || alpha <= 0.01f)
                    continue;
                const float lx = cx + (i < f.lead_lines ? indent : 0.0f);
                const float width = ui::text(list, fonts.regular, line, lx, baseline, kBodySize,
                                             kInk.with_alpha(0.88f * alpha));
                // The end mark: a small accent square after the last word.
                if (c + 1 == columns && i + 1 == column.end)
                    list.rounded_rect({lx + width + 12.0f, baseline - 13.0f, 12.0f, 12.0f}, 0,
                                      kAccent.with_alpha(alpha));
            }
        }
        // The drop cap stands on the third baseline and reaches the top of
        // the first line's capitals.
        const float cap = first + kBodyLine * static_cast<float>(kDropLines - 1) - scroll;
        ui::text(list, fonts.display, f.drop_cap, x - 6.0f, cap, kDropSize,
                 kAccent.with_alpha(fade(first - scroll)));
        list.pop_clip();
    }

    // ---- drawing: hints ----------------------------------------------------

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::light();
        const bool swapped = context_.settings.swap_confirm;
        const ui::Button confirm = swapped ? ui::Button::circle : ui::Button::cross;
        const ui::Button back = swapped ? ui::Button::cross : ui::Button::circle;
        const bool marked = bookmarked_[static_cast<std::size_t>(focus_)];
        const char *bookmark = marked ? "Remove bookmark" : "Bookmark";
        // The two rows cross-fade as the view changes.
        const float t = view();
        const float browsing =
            tween::stagger(age_, 10, 0.06f, 0.6f) * (1.0f - tween::smoothstep(t * 2.0f));
        if (browsing > 0.01f)
        {
            list.push_opacity(browsing);
            const ui::Hint hints[] = {{confirm, "Read"}, {ui::Button::triangle, bookmark}};
            ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            list.pop_opacity();
        }
        const float reading = tween::smoothstep(t * 2.0f - 1.0f);
        if (reading > 0.01f)
        {
            list.push_opacity(reading);
            const ui::Hint hints[] = {{ui::Button::l2, "Turn page", ui::Button::r2},
                                      {ui::Button::dpad, "Scroll"},
                                      {ui::Button::triangle, bookmark},
                                      {back, "Back"}};
            ui::draw_hints(list, fonts, style, hints, 4, kRight, true);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    std::array<Feature, kFeatures> features_;
    std::array<bool, kFeatures> bookmarked_{};
    float age_ = 0.0f;                            // seconds since enter(): drives the entrance
    float clock_ = 0.0f;                          // free-running time for idle motion
    int focus_ = 0;                               // the focused row, and the article being read
    int previous_ = 0;                            // the feature that is fading out
    float direction_ = 1.0f;                      // +1 toward later features: where content travels
    bool turn_ = false;                           // the last change was a page turn (sideways)
    tween::Timer change_;                         // the cross-fade between previous_ and focus_
    std::array<tween::Spring, kFeatures> lit_;    // 0..1 focus light of each row
    std::array<float, kFeatures> wipe_{};         // 0..1 underline progress
    std::array<tween::Bounce, kFeatures> ribbon_; // 0..1 bookmark drop
    ui::Scroller scroll_;                         // the list
    bool reading_ = false;
    tween::Spring view_;          // 0 the list, 1 the article
    tween::Spring body_scroll_;   // pixels, always a whole number of lines at rest
    float leaving_scroll_ = 0.0f; // where the departing article was scrolled to
    ui::SpringColor plate_[2];
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
};

} // namespace

std::unique_ptr<app::Concept> make_editorial(app::Context &context)
{
    return std::make_unique<Editorial>(context);
}

} // namespace hui::concepts
