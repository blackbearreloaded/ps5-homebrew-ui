// ps5-homebrew-ui - Design "Phosphor": a monochrome CRT terminal.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A text-mode interface on an old picture tube: one colour, one typeface, a
// strict grid of 100 x 25 character cells, boxes drawn with lines. A menu of
// commands on the left, the page of the focused command on the right. What
// makes it feel finished is how far the conceit is carried:
//
//   - everything sits on the character grid: text is placed by column and
//     row, rules run through the centres of cells like box-drawing characters,
//     and even the controller glyphs take whole cells;
//   - every character glows: the text is drawn twice, a soft halo first and
//     the crisp glyphs over it, on a backdrop that is lit from the centre and
//     under a scanline overlay; the whole picture flickers by a few percent;
//   - the focus is an inverse-video bar that jumps from row to row, because a
//     terminal does not ease, and the row it left keeps a fading ghost of the
//     bar for a moment: phosphor persistence does the job a spring does in
//     the other designs;
//   - pages are not swapped, they are redrawn: a wipe reveals the new page
//     row by row, each row a little brighter while it is fresh, over the
//     afterglow of the old page;
//   - it boots: the lines type themselves with a block cursor and quiet key
//     sounds, and SHUTDOWN collapses the picture to a line and a dot before
//     the boot runs again;
//   - sounds are sparse and mechanical, placed left for the menu and right
//     for the page, and a refused move rings the visual bell: the bar blinks.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the character grid ----------------------------------------------------
constexpr int kCols = 100;
constexpr int kRows = 25;
constexpr float kText = 28.0f;     // the one type size on the screen
constexpr float kLine = 38.0f;     // height of a row
constexpr float kBaseline = 28.0f; // baseline below a row's top edge
constexpr float kRule = 2.0f;      // stroke of the box lines
constexpr float kBarInset = 2.0f;  // an inverse bar is this much shorter than its row
constexpr float kGlyph = 32.0f;    // controller glyphs: two cells wide

// ---- brightness: the only hierarchy a one-colour screen has -----------------
constexpr float kBright = 1.0f;
constexpr float kNormal = 0.8f;
constexpr float kDim = 0.5f;
constexpr float kFaint = 0.24f;
constexpr float kHalo = 0.2f;        // strength of the glow under each glyph
constexpr float kHaloShift = 1.7f;   // how far it spreads, in pixels
constexpr float kGlassGlow = 0.085f; // backdrop: phosphor light on the glass

// ---- layout, in cells ------------------------------------------------------
constexpr int kTitleRow = 0;
constexpr int kBoxTop = 2;
constexpr int kBoxBottom = 22;
constexpr int kStatusRow = 24;
constexpr int kMenuRight = 25; // left box: columns 0..25
constexpr int kPaneLeft = 27;  // right box: columns 27..99
constexpr int kMenuRow = 4;    // row of the first command
constexpr int kMenuStep = 2;
constexpr int kNoteRow = 19; // the three-line note under the menu
constexpr int kPaneCol = 29; // first text column inside the right box
constexpr int kPaneRow = 4;  // ... and its first text row
constexpr int kPaneCols = 69;

// ---- timing ----------------------------------------------------------------
constexpr float kAfterglow = 0.15f;           // how long a ghost of the bar persists
constexpr float kWipeRow = 0.022f;            // a page is drawn one row every 22 ms
constexpr float kFresh = 0.18f;               // a new row is brighter for this long
constexpr float kEntranceRow = 0.018f;        // the whole screen: 25 rows in 450 ms
constexpr float kTypeInterval = 1.0f / 12.0f; // at most twelve key sounds a second
constexpr float kBootLogo = 0.3f;
constexpr float kBootChar = 1.0f / 90.0f;
constexpr float kBootDot = 1.0f / 60.0f;
constexpr float kBootPause = 0.1f; // before a check prints its result
constexpr float kBootLineGap = 0.05f;
constexpr float kBootHold = 0.5f; // the finished boot text stays this long
constexpr float kPowerOff = 0.7f;
constexpr float kDark = 0.55f; // the tube stays dark before it boots again
constexpr float kPing = 0.9f;
constexpr float kDetailRate = 260.0f; // characters per second of typed details

constexpr const char *kTechniques[] = {
    "A strict 100 x 25 character grid: every glyph, rule and bar is placed in cells",
    "Phosphor glow: each line drawn as a soft halo plus crisp text, under scanlines",
    "Inverse-video focus that snaps, with an afterglow ghost instead of a spring",
    "Pages redrawn by a row-by-row wipe over the fading ghost of the old page",
    "Typed boot sequence with rate-limited key sounds; any button skips it",
    "CRT power-off: a clip rectangle collapses the picture to a line, then a dot",
    "Theme colours eased with ui::SpringColor; glyphs drawn with GlyphStyle::mono",
};

struct Theme
{
    const char *name;
    std::uint32_t colour;
};
constexpr Theme kThemes[] = {{"GREEN", 0x5dff9b}, {"AMBER", 0xffb648}, {"ICE", 0x9bdcff}};
constexpr int kThemeCount = 3;

enum Page : int
{
    kLibrary,
    kMessages,
    kSystem,
    kNetwork,
    kDiagnostics,
    kArchive,
    kShutdown,
    kCommands,
};

struct CommandInfo
{
    const char *name;
    const char *path; // shown in the prompt of the status line
    const char *note; // the short description under the menu
};
constexpr CommandInfo kCommandInfo[kCommands] = {
    {"LIBRARY", "library", "Every title on the disk. Mark the ones worth keeping."},
    {"MESSAGES", "messages", "Mail from the other machines in the lab."},
    {"SYSTEM", "system", "Live numbers from this very program."},
    {"NETWORK", "network", "Links, hosts and a ping to test them."},
    {"DIAGNOSTICS", "diagnostics", "Test patterns for the picture tube."},
    {"ARCHIVE", "archive", "Saved files, kept in a tree of folders."},
    {"SHUTDOWN", "shutdown", "Power the display off and boot again."},
};

// ---- boot text -------------------------------------------------------------
struct BootLine
{
    int row;
    const char *label;
    const char *result; // empty: a plain line with no check
};
constexpr BootLine kBoot[] = {
    {5, "HOMEBREW UI LAB  v1.0", ""},   {7, "memory check", "16384K ok"},
    {8, "video", "1920 x 1080, 60 Hz"}, {9, "controller", "connected"},
    {10, "sound bank", "paper, ok"},    {11, "loading designs", "14 found"},
    {12, "starting shell", "ready"},
};
constexpr int kBootLines = 7;
constexpr int kBootCol = 2;
constexpr int kBootDotsTo = 26; // labels are padded with dots to this width
constexpr int kBootPromptRow = 14;
constexpr int kLogoRow = 1;

// "PHOSPHOR" in a 5 x 5 pixel face; a pixel is one cell wide and half a row
// tall, the way half-block characters double a text screen's resolution.
constexpr std::uint8_t kLetters[5][5] = {
    {0b11110, 0b10001, 0b11110, 0b10000, 0b10000}, // P
    {0b10001, 0b10001, 0b11111, 0b10001, 0b10001}, // H
    {0b01110, 0b10001, 0b10001, 0b10001, 0b01110}, // O
    {0b01111, 0b10000, 0b01110, 0b00001, 0b11110}, // S
    {0b11110, 0b10001, 0b11110, 0b10010, 0b10001}, // R
};
constexpr int kLogo[] = {0, 1, 2, 3, 0, 1, 2, 4};
constexpr int kLogoLetters = 8;

// ---- invented page content -------------------------------------------------
constexpr int kLibraryRows = 11; // titles visible at once

struct Message
{
    const char *from;
    const char *subject;
    const char *date;
    const char *body;
};
constexpr Message kMail[] = {
    {"sysop", "Welcome to the lab", "01 OCT",
     "This terminal is one of fourteen interface designs built on the same small kit. "
     "Nothing here can break: open every command and lean on every button."},
    {"kiln-01", "Firing finished", "30 SEP",
     "The overnight firing ended at 04:12. Peak temperature held for forty minutes. Two "
     "bowls cracked; the blue glaze came out better than anyone hoped."},
    {"relay", "Night shift roster", "29 SEP",
     "The harbor relay needs one more runner for the midnight leg. Bring a lamp and "
     "comfortable shoes. Tea is provided at the second beacon."},
    {"orbit", "Rain schedule changed", "27 SEP",
     "The garden moon now spins four percent faster. Expect light rain on the terraces "
     "every ninth hour until further notice."},
    {"lighthouse", "Re: the radio", "24 SEP",
     "It answered again last night, on the old frequency. I wrote down what it said. Come "
     "and read it before the tide turns."},
};
constexpr int kMessageCount = 5;

struct Host
{
    const char *name;
    const char *address;
    int reply_ms; // 0: the host never answers
};
constexpr Host kHosts[] = {
    {"gateway", "10.0.4.1", 3},
    {"archive", "10.0.4.7", 11},
    {"relay", "10.0.4.19", 27},
    {"lighthouse", "10.0.4.250", 0},
};
constexpr int kHostCount = 4;
constexpr int kHostRow = 6;

struct Node
{
    int depth;
    bool folder;
    const char *name;
    const char *size;
    const char *date;
};
constexpr Node kNodes[] = {
    {0, true, "saves", "", ""},
    {1, false, "tidewater.sav", "412K", "28 SEP"},
    {1, false, "ember-and-ash.sav", "1.2M", "30 SEP"},
    {1, false, "kiln.sav", "96K", "12 AUG"},
    {0, true, "captures", "", ""},
    {1, true, "2026-09", "", ""},
    {2, false, "harbor-0001.img", "2.4M", "19 SEP"},
    {2, false, "harbor-0002.img", "2.6M", "19 SEP"},
    {1, false, "index.txt", "2K", "19 SEP"},
    {0, true, "logs", "", ""},
    {1, false, "boot.log", "8K", "01 OCT"},
    {1, false, "tour.log", "31K", "01 OCT"},
    {0, false, "readme.txt", "1K", "02 JAN"},
};
constexpr int kNodeCount = 13;

constexpr int kWindows[] = {60, 120, 240}; // frames shown by the SYSTEM graph
constexpr int kMaxFrequency = 8;

constexpr app::TourStep kTour[] = {
    // The first picture is the boot sequence, half typed. Skip the rest.
    {0.3f, action_bit(Action::confirm)},
    {1.2f, action_bit(Action::confirm)}, // open LIBRARY
    {0.25f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.25f, action_bit(Action::confirm)}, // mark a title
    {0.25f, 0, Direction::down},
    {0.9f, action_bit(Action::back), Direction::none, "library"},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.4f, action_bit(Action::west)}, // amber
    {0.3f, action_bit(Action::confirm)},
    {0.25f, 0, Direction::up},
    {1.3f, action_bit(Action::back), Direction::none, "diagnostics"},
    {0.3f, 0, Direction::down},
    {0.3f, action_bit(Action::west)},      // ice
    {1.0f, 0, Direction::down, "archive"}, // the menu has the input here
    {0.3f, action_bit(Action::west)},      // green again
    {0.5f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::left},
    {0.8f, action_bit(Action::confirm), Direction::none, "shutdown"},
    {0.22f, 0, Direction::none, "poweroff"},
    // The terminal boots again; skip that and end on the menu.
    {1.8f, action_bit(Action::confirm)},
};

// Number of character cells a UTF-8 string takes.
int cells(std::string_view text)
{
    int count = 0;
    for (const char c : text)
    {
        if ((static_cast<unsigned char>(c) & 0xc0) != 0x80)
            ++count;
    }
    return count;
}

// The first `count` characters of a UTF-8 string.
std::string_view prefix(std::string_view text, int count)
{
    std::size_t end = 0;
    while (end < text.size() && count > 0)
    {
        ++end;
        while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
            ++end;
        --count;
    }
    return text.substr(0, end);
}

// Breaks plain text into lines of at most `width` characters at spaces. On a
// character grid wrapping is counting, not measuring. Returns the line count.
int wrap_cells(std::string_view text, int width, std::string_view *lines, int capacity)
{
    int count = 0;
    while (!text.empty() && count < capacity)
    {
        if (static_cast<int>(text.size()) <= width)
        {
            lines[count++] = text;
            break;
        }
        std::size_t cut = text.rfind(' ', static_cast<std::size_t>(width));
        if (cut == std::string_view::npos || cut == 0)
            cut = static_cast<std::size_t>(width);
        lines[count++] = text.substr(0, cut);
        text.remove_prefix(cut);
        while (!text.empty() && text.front() == ' ')
            text.remove_prefix(1);
    }
    return count;
}

// The picture tube: draws in one colour on the character grid. Columns and
// rows are floats so that rules can run through the centres of cells
// (column 3.5 is the middle of cell 3), the way box-drawing characters do.
struct Screen
{
    gfx::DrawList &list;
    const ui::FontRef &font;
    float cell; // width of a character
    float left; // x of column 0
    float top;  // y of row 0
    Color ink;  // the phosphor colour
    Color dark; // the unlit glass, for inverse video

    float x(float col) const
    {
        return left + col * cell;
    }
    float y(float row) const
    {
        return top + row * kLine;
    }

    // Glowing text. The halo is the same line drawn twice at low alpha, a
    // little to the left and to the right: an electron beam smears along its
    // scan direction. Two extra glyph runs are far cheaper than a blur pass.
    void text(float col, float row, std::string_view value, float level = kNormal) const
    {
        if (level <= 0.01f || value.empty())
            return;
        const float px = x(col);
        const float baseline = y(row) + kBaseline;
        const Color halo = ink.with_alpha(level * kHalo);
        ui::text(list, font, value, px - kHaloShift, baseline, kText, halo);
        ui::text(list, font, value, px + kHaloShift, baseline, kText, halo);
        ui::text(list, font, value, px, baseline, kText, ink.with_alpha(level));
    }
    // The last character lands in column `col_end`.
    void text_right(float col_end, float row, std::string_view value, float level = kNormal) const
    {
        text(col_end + 1.0f - static_cast<float>(cells(value)), row, value, level);
    }
    // Text on an inverse bar: unlit glass, so no halo.
    void inverse(float col, float row, std::string_view value) const
    {
        ui::text(list, font, value, x(col), y(row) + kBaseline, kText, dark);
    }

    Rect span(float col, float row, float cols) const
    {
        return {x(col), y(row) + kBarInset, cols * cell, kLine - 2.0f * kBarInset};
    }
    // A lit block of cells: the focus bar, its ghost, a cursor.
    void bar(const Rect &r, float level) const
    {
        if (level <= 0.01f)
            return;
        list.glow(r, 2.0f, 14.0f, ink.with_alpha(0.3f * level));
        list.rounded_rect(r, 0.0f, ink.with_alpha(level));
    }

    void rule(float x1, float y1, float x2, float y2, float level) const
    {
        if (level > 0.7f)
            list.line(x1, y1, x2, y2, kRule + 5.0f, ink.with_alpha(0.12f * level));
        list.line(x1, y1, x2, y2, kRule, ink.with_alpha(level));
    }
    void hline(float row, float col1, float col2, float level) const
    {
        rule(x(col1), y(row), x(col2), y(row), level);
    }
    void vline(float col, float row1, float row2, float level) const
    {
        rule(x(col), y(row1), x(col), y(row2), level);
    }
};

// The right box while one page is being drawn. Columns and rows are relative
// to its text area. It knows how far the wipe has come and which row, if any,
// lies under the inverse bar.
struct Pane
{
    const Screen &s;
    float age;    // seconds since the page started to draw
    bool reduced; // reduced motion: the page fades in as a whole
    int inverse_row = -1;

    // Brightness factor of a row: 0 before the wipe reaches it, above 1
    // while its phosphor is fresh, then 1.
    float gain(int row) const
    {
        if (reduced)
            return tween::clamp01(age / 0.15f);
        const float since = age - static_cast<float>(row) * kWipeRow;
        if (since < 0.0f)
            return 0.0f;
        return 1.0f + 0.3f * (1.0f - tween::clamp01(since / kFresh));
    }
    Color ink(int row, float level) const
    {
        if (row == inverse_row)
            return s.dark.with_alpha(level);
        return s.ink.with_alpha(std::min(1.0f, level * gain(row)));
    }
    void text(int col, int row, std::string_view value, float level = kNormal) const
    {
        if (row == inverse_row)
            s.inverse(static_cast<float>(kPaneCol + col), static_cast<float>(kPaneRow + row),
                      value);
        else
            s.text(static_cast<float>(kPaneCol + col), static_cast<float>(kPaneRow + row), value,
                   std::min(1.0f, level * gain(row)));
    }
    void right(int col_end, int row, std::string_view value, float level = kNormal) const
    {
        text(col_end + 1 - cells(value), row, value, level);
    }
    void rule(int row, float level = kFaint) const
    {
        const float visible = std::min(1.0f, gain(row));
        if (visible > 0.01f)
            s.hline(static_cast<float>(kPaneRow + row) + 0.5f, static_cast<float>(kPaneCol),
                    static_cast<float>(kPaneCol + kPaneCols), level * visible);
    }
    float x(float col) const
    {
        return s.x(static_cast<float>(kPaneCol) + col);
    }
    float y(float row) const
    {
        return s.y(static_cast<float>(kPaneRow) + row);
    }
    // Shapes that span several rows (graphs, the scope) are revealed by the
    // same wipe through a clip rectangle that grows a row at a time.
    bool begin_graphics(int row1, int row2) const
    {
        int rows = row2 - row1 + 1;
        float alpha = 1.0f;
        if (reduced)
            alpha = tween::clamp01(age / 0.15f);
        else
            rows = std::min(rows, static_cast<int>(age / kWipeRow) + 1 - row1);
        if (rows <= 0 || alpha <= 0.01f)
            return false;
        s.list.push_clip({x(-1.5f), y(static_cast<float>(row1)),
                          static_cast<float>(kPaneCols + 3) * s.cell,
                          static_cast<float>(rows) * kLine});
        s.list.push_opacity(alpha);
        return true;
    }
    void end_graphics() const
    {
        s.list.pop_opacity();
        s.list.pop_clip();
    }
};

// Phosphor persistence: where the bar was, fading.
struct Afterglow
{
    Rect rect;
    float age = 1.0f;

    void leave(const Rect &r)
    {
        rect = r;
        age = 0.0f;
    }
    float level() const
    {
        const float t = 1.0f - tween::clamp01(age / kAfterglow);
        return 0.5f * t * t;
    }
};

class Terminal final : public app::Concept
{
  public:
    explicit Terminal(app::Context &context)
        : context_(context), mono_fonts_{context.fonts.mono, context.fonts.mono, context.fonts.mono,
                                         context.fonts.mono}
    {
        // The grid is derived from the font: one cell is the advance of a
        // glyph, and the 100 columns are centred on the canvas.
        cell_ = context.fonts.mono.measure("M", kText);
        left_ = (gfx::kVirtualWidth - static_cast<float>(kCols) * cell_) * 0.5f;
        top_ = (gfx::kVirtualHeight - static_cast<float>(kRows) * kLine) * 0.5f;
        marked_.assign(context.catalog.size(), false);
        phosphor_.snap(Color::rgb(kThemes[0].colour));
        for (int i = 0; i < kNodeCount; ++i)
            expanded_[i] = i != 9; // the logs folder starts closed
        flagged_[0] = flagged_[4] = true;
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "terminal",
            "Phosphor",
            "A monochrome CRT terminal: character grid, glow, typed text",
            "src/concepts/terminal.cpp",
            audio::SoundSet::paper,
            Color::rgb(kThemes[0].colour),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        start_boot();
        opened_ = false;
        answer_ = 1;
        ping_host_ = -1;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        type_cooldown_ -= dt;
        switch (mode_)
        {
        case Mode::boot:
            update_boot(input, feedback);
            break;
        case Mode::main:
            if (input.is_pressed(Action::west))
            {
                theme_ = (theme_ + 1) % kThemeCount;
                phosphor_.target(Color::rgb(kThemes[theme_].colour));
                feedback.play(audio::Cue::toggle, 1.0f + 0.06f * static_cast<float>(theme_));
            }
            if (opened_)
                update_pane(input, feedback);
            else
                update_menu(input, feedback);
            break;
        case Mode::power_off:
            power_.update(dt);
            if (!power_.running)
            {
                mode_ = Mode::dark;
                age_ = 0.0f;
            }
            break;
        case Mode::dark:
            if (age_ >= kDark)
            {
                start_boot();
                feedback.play(audio::Cue::restart, 1.0f, 0.0f, 0.7f);
            }
            break;
        }

        // ---- animation state ----
        phosphor_.update(dt, 5.0f);
        pane_age_ += dt;
        detail_age_ += dt;
        prompt_age_ += dt;
        page_ghost_ += dt;
        menu_ghost_.age += dt;
        pane_ghost_.age += dt;
        bell_.update(dt, 9.0f);
        if (!hold_)
            scope_time_ += dt;
        if (ping_host_ >= 0)
        {
            ping_age_ += dt;
            if (ping_age_ >= kPing)
            {
                // The answer arrives later than the press: it gets its own sound.
                const bool reply = kHosts[ping_host_].reply_ms > 0;
                ping_result_[ping_host_] = reply ? 1 : 2;
                if (mode_ == Mode::main)
                    feedback.play(reply ? audio::Cue::connect : audio::Cue::invalid, 1.0f,
                                  pane_pan(), 0.8f);
                ping_host_ = -1;
            }
        }
    }

    void draw(app::Frame &frame) const override
    {
        const bool reduced = context_.settings.reduced_motion;
        const Color ink = phosphor_.value();
        const Color dark = gfx::mix(Color::rgb(0x020303), ink, 0.03f);
        const float power = tube_power();

        frame.backdrop.mode = gfx::BackdropMode::phosphor;
        frame.backdrop.colors[0] = dark;
        frame.backdrop.colors[1] = {ink.r * kGlassGlow * power, ink.g * kGlassGlow * power,
                                    ink.b * kGlassGlow * power, 1.0f};
        frame.backdrop.time = reduced ? 0.0f : clock_;
        frame.post.mode = gfx::BackdropMode::scanlines;
        frame.post.params[0] = 0.22f;
        frame.post.params[1] = 0.5f;

        gfx::DrawList &list = frame.scene;
        const Screen s{list, context_.fonts.mono, cell_, left_, top_, ink, dark};

        // A tube never holds its brightness exactly: the whole picture
        // breathes by up to three percent on two slow, unrelated sines.
        float flicker = 1.0f;
        if (!reduced)
            flicker -= 0.03f * (0.5f + 0.5f * std::sin(clock_ * 5.3f) * std::sin(clock_ * 1.7f));
        list.push_opacity(flicker);
        // The raster: an energised tube is never quite black.
        list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f,
                          ink.with_alpha(0.014f * power));

        switch (mode_)
        {
        case Mode::boot:
            draw_boot(s, age_, true);
            draw_hints(s);
            break;
        case Mode::main:
            draw_main_entering(s);
            break;
        case Mode::power_off:
            draw_power_off(s);
            break;
        case Mode::dark:
        {
            // The last spot of light dies away after the picture is gone.
            const float t = 1.0f - tween::clamp01(age_ / 0.3f);
            list.glow({956, 536, 8, 8}, 4.0f, 30.0f, ink.with_alpha(reduced ? 0.0f : 0.5f * t * t));
            break;
        }
        }
        list.pop_opacity();
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    enum class Mode
    {
        boot,
        main,
        power_off,
        dark,
    };

    // What the boot text shows at a given moment: every line before `line`
    // is complete, and `line` itself has this much of its label, its dots
    // and its result.
    struct BootState
    {
        int line = 0;
        int label = 0;
        int dots = 0;
        bool result = false;
        bool done = false;
    };

    static int boot_dots(const BootLine &line)
    {
        if (line.result[0] == '\0')
            return 0;
        return kBootDotsTo - static_cast<int>(std::strlen(line.label)) - 1;
    }

    static BootState boot_state(float t)
    {
        BootState state;
        t -= kBootLogo;
        for (int i = 0; i < kBootLines; ++i)
        {
            state = {};
            state.line = i;
            const int length = static_cast<int>(std::strlen(kBoot[i].label));
            const int dots = boot_dots(kBoot[i]);
            if (t < static_cast<float>(length) * kBootChar)
            {
                state.label = std::max(0, static_cast<int>(t / kBootChar));
                return state;
            }
            state.label = length;
            t -= static_cast<float>(length) * kBootChar;
            if (dots > 0)
            {
                if (t < static_cast<float>(dots) * kBootDot)
                {
                    state.dots = static_cast<int>(t / kBootDot);
                    return state;
                }
                state.dots = dots;
                t -= static_cast<float>(dots) * kBootDot;
                if (t < kBootPause)
                    return state;
                state.result = true;
                t -= kBootPause;
            }
            if (t < kBootLineGap)
                return state;
            t -= kBootLineGap;
        }
        state = {};
        state.line = kBootLines;
        state.done = true;
        return state;
    }

    static float boot_duration()
    {
        float total = kBootLogo;
        for (const BootLine &line : kBoot)
        {
            total += static_cast<float>(std::strlen(line.label)) * kBootChar + kBootLineGap;
            if (boot_dots(line) > 0)
                total += static_cast<float>(boot_dots(line)) * kBootDot + kBootPause;
        }
        return total;
    }

    void start_boot()
    {
        mode_ = Mode::boot;
        age_ = 0.0f;
        boot_key_ = 0;
        type_cooldown_ = 0.0f;
    }

    void start_main()
    {
        boot_exit_age_ = age_;
        mode_ = Mode::main;
        age_ = 0.0f;
        pane_age_ = 0.0f;
        detail_age_ = 0.0f;
        previous_ = focus_;
        menu_ghost_.age = pane_ghost_.age = 1.0f;
    }

    // How bright the tube is as a whole: it warms up when the boot starts
    // and dies with the power-off.
    float tube_power() const
    {
        const bool reduced = context_.settings.reduced_motion;
        switch (mode_)
        {
        case Mode::boot:
            return reduced ? 1.0f : 0.25f + 0.75f * tween::smoothstep(age_ / 0.6f);
        case Mode::main:
            return 1.0f;
        case Mode::power_off:
            return 1.0f - tween::smoothstep(power_.progress() * 2.0f);
        case Mode::dark:
            return 0.0f;
        }
        return 1.0f;
    }

    // ---- geometry shared by update (ghosts) and draw ----
    Rect cell_span(float col, float row, float cols) const
    {
        return {left_ + col * cell_, top_ + row * kLine + kBarInset, cols * cell_,
                kLine - 2.0f * kBarInset};
    }
    Rect menu_bar(int index) const
    {
        return cell_span(1.0f, static_cast<float>(kMenuRow + index * kMenuStep),
                         static_cast<float>(kMenuRight - 1));
    }
    static bool has_rows(int page)
    {
        return page == kLibrary || page == kMessages || page == kNetwork || page == kArchive;
    }
    // The pane row under the cursor of a page that has one.
    int cursor_row(int page) const
    {
        switch (page)
        {
        case kLibrary:
            return 2 + cursor_[page] - library_top_;
        case kMessages:
            return 2 + cursor_[page];
        case kNetwork:
            return kHostRow + cursor_[page];
        default:
            return 1 + cursor_[page];
        }
    }
    Rect pane_bar(int page) const
    {
        return cell_span(static_cast<float>(kPaneLeft + 1),
                         static_cast<float>(kPaneRow + cursor_row(page)),
                         static_cast<float>(kCols - kPaneLeft - 2));
    }
    // YES and NO of the shutdown prompt.
    Rect answer_bar(int answer) const
    {
        return cell_span(static_cast<float>(kPaneCol + (answer == 0 ? 2 : 10)),
                         static_cast<float>(kPaneRow + 8), answer == 0 ? 5.0f : 4.0f);
    }
    float menu_pan() const
    {
        return ui::pan_for_x(left_ + 13.0f * cell_);
    }
    float pane_pan() const
    {
        return ui::pan_for_x(left_ + 63.0f * cell_);
    }

    int visible_nodes(int *out) const
    {
        int count = 0;
        int closed_depth = 99; // nodes deeper than this are inside a closed folder
        for (int i = 0; i < kNodeCount; ++i)
        {
            if (kNodes[i].depth > closed_depth)
                continue;
            closed_depth = 99;
            out[count++] = i;
            if (kNodes[i].folder && !expanded_[i])
                closed_depth = kNodes[i].depth;
        }
        return count;
    }

    // The visual bell. A terminal cannot nudge its cursor sideways, so a
    // refused move blinks the bar twice instead.
    bool bar_lit() const
    {
        if (context_.settings.reduced_motion)
            return true;
        const float v = bell_.value;
        return !(v > 0.6f || (v > 0.2f && v < 0.36f));
    }
    bool cursor_lit() const
    {
        return context_.settings.reduced_motion || std::fmod(clock_, 1.06f) < 0.6f;
    }

    void refuse(app::Feedback &feedback)
    {
        feedback.play(audio::Cue::error, 1.0f, opened_ ? pane_pan() : menu_pan(), 0.6f);
        feedback.rumble(0.25f, 0.05f);
        bell_.trigger();
    }

    // ---- input ----
    void update_boot(const InputFrame &input, app::Feedback &feedback)
    {
        const bool reduced = context_.settings.reduced_motion;
        if (input.pressed != 0 || input.nav != Direction::none)
        {
            feedback.play(audio::Cue::select, 1.0f, 0.0f, 0.8f);
            start_main();
            return;
        }
        const BootState state = boot_state(age_);
        if (!reduced)
        {
            // One key sound per burst of characters, not per character: the
            // cool-down keeps it to twelve a second however fast the text runs.
            const int key = state.line * 256 + state.label + state.dots;
            if (key > boot_key_ && !state.done && type_cooldown_ <= 0.0f)
            {
                const bool dots = state.dots > 0;
                feedback.play(
                    audio::Cue::type, dots ? 0.86f : 1.0f,
                    ui::pan_for_x(left_ +
                                  static_cast<float>(kBootCol + state.label + state.dots) * cell_),
                    0.45f);
                type_cooldown_ = kTypeInterval;
            }
            boot_key_ = key;
        }
        const float length = reduced ? 0.9f : boot_duration() + kBootHold;
        if (age_ >= length)
            start_main();
    }

    void update_menu(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::up:
        case Direction::down:
        {
            const int next = focus_ + (input.nav == Direction::down ? 1 : -1);
            if (next < 0 || next >= kCommands)
            {
                if (!input.nav_repeat)
                    refuse(feedback);
                break;
            }
            // The bar jumps; what eases is the light it leaves behind.
            menu_ghost_.leave(menu_bar(focus_));
            previous_ = focus_;
            focus_ = next;
            pane_age_ = 0.0f;
            page_ghost_ = 0.0f;
            detail_age_ = 0.0f;
            // Lower rows sound lower; the menu sits left of centre.
            feedback.play(audio::Cue::tick, 1.09f - 0.03f * static_cast<float>(focus_), menu_pan());
            break;
        }
        case Direction::right:
            open_pane(feedback);
            return;
        case Direction::left:
            if (!input.nav_repeat)
                refuse(feedback);
            break;
        case Direction::none:
            break;
        }
        if (input.is_pressed(Action::confirm))
            open_pane(feedback);
        else if (input.is_pressed(Action::back))
            refuse(feedback); // nothing above the menu
    }

    void open_pane(app::Feedback &feedback)
    {
        opened_ = true;
        pane_ghost_.age = 1.0f;
        detail_age_ = 0.0f;
        if (focus_ == kShutdown)
        {
            answer_ = 1; // the safe answer
            prompt_age_ = 0.0f;
            feedback.play(audio::Cue::modal_open, 1.0f, pane_pan());
        }
        else
        {
            feedback.play(audio::Cue::select, 1.0f, pane_pan());
        }
    }

    void close_pane(app::Feedback &feedback, audio::Cue cue)
    {
        opened_ = false;
        pane_ghost_.age = 1.0f;
        feedback.play(cue, 1.0f, menu_pan());
    }

    // Up and down on a page with a row cursor.
    void move_rows(const InputFrame &input, app::Feedback &feedback, int count)
    {
        if (input.nav != Direction::up && input.nav != Direction::down)
            return;
        const int next = cursor_[focus_] + (input.nav == Direction::down ? 1 : -1);
        if (next < 0 || next >= count)
        {
            if (!input.nav_repeat)
                refuse(feedback);
            return;
        }
        const Rect before = pane_bar(focus_);
        cursor_[focus_] = next;
        if (focus_ == kLibrary)
        {
            // The list scrolls a whole row at a time, keeping one title of
            // context beyond the cursor.
            if (next < library_top_ + 1)
                library_top_ = std::max(0, next - 1);
            if (next > library_top_ + kLibraryRows - 2)
                library_top_ = std::min(count - kLibraryRows, next - (kLibraryRows - 2));
            library_top_ = std::max(0, library_top_);
        }
        const Rect after = pane_bar(focus_);
        if (after.y != before.y)
            pane_ghost_.leave(before);
        detail_age_ = 0.0f;
        feedback.play(audio::Cue::tick,
                      std::max(0.8f, 1.1f - 0.018f * static_cast<float>(cursor_row(focus_))),
                      pane_pan());
    }

    // Up and down on a page that changes a value instead of moving a cursor.
    void step_value(const InputFrame &input, app::Feedback &feedback, int *value, int low, int high)
    {
        if (input.nav != Direction::up && input.nav != Direction::down)
            return;
        const int next = *value + (input.nav == Direction::up ? 1 : -1);
        if (next < low || next > high)
        {
            if (!input.nav_repeat)
                refuse(feedback);
            return;
        }
        *value = next;
        feedback.play(audio::Cue::slider,
                      0.85f +
                          0.5f * static_cast<float>(next - low) / static_cast<float>(high - low),
                      pane_pan());
    }

    void toggle_flag(bool now, app::Feedback &feedback)
    {
        feedback.play(now ? audio::Cue::mark : audio::Cue::erase, 1.0f, pane_pan());
    }

    void update_pane(const InputFrame &input, app::Feedback &feedback)
    {
        const bool prompt = focus_ == kShutdown;
        if (input.is_pressed(Action::back) || (!prompt && input.nav == Direction::left))
        {
            close_pane(feedback, prompt ? audio::Cue::modal_close : audio::Cue::back);
            return;
        }
        if (!prompt && input.nav == Direction::right && !input.nav_repeat)
            refuse(feedback);
        const bool confirm = input.is_pressed(Action::confirm);
        switch (focus_)
        {
        case kLibrary:
            move_rows(input, feedback, static_cast<int>(context_.catalog.size()));
            if (confirm)
            {
                const std::size_t index = static_cast<std::size_t>(cursor_[kLibrary]);
                marked_[index] = !marked_[index];
                toggle_flag(marked_[index], feedback);
            }
            break;
        case kMessages:
            move_rows(input, feedback, kMessageCount);
            if (confirm)
            {
                bool &flag = flagged_[cursor_[kMessages]];
                flag = !flag;
                toggle_flag(flag, feedback);
            }
            break;
        case kSystem:
            step_value(input, feedback, &window_, 0, 2);
            if (confirm)
                refuse(feedback);
            break;
        case kNetwork:
            move_rows(input, feedback, kHostCount);
            if (confirm)
            {
                ping_host_ = cursor_[kNetwork];
                ping_age_ = 0.0f;
                ping_result_[ping_host_] = 0;
                feedback.play(audio::Cue::select, 1.0f, pane_pan());
            }
            break;
        case kDiagnostics:
            step_value(input, feedback, &frequency_, 1, kMaxFrequency);
            if (confirm)
            {
                hold_ = !hold_;
                feedback.play(audio::Cue::toggle, hold_ ? 0.92f : 1.0f, pane_pan());
            }
            break;
        case kArchive:
        {
            int nodes[kNodeCount];
            const int count = visible_nodes(nodes);
            cursor_[kArchive] = std::min(cursor_[kArchive], count - 1);
            move_rows(input, feedback, count);
            if (confirm)
            {
                const int node = nodes[cursor_[kArchive]];
                if (kNodes[node].folder)
                {
                    expanded_[node] = !expanded_[node];
                    feedback.play(audio::Cue::flip, expanded_[node] ? 1.0f : 0.9f, pane_pan());
                }
                else
                {
                    tagged_[node] = !tagged_[node];
                    toggle_flag(tagged_[node], feedback);
                }
            }
            break;
        }
        default: // the shutdown prompt
            if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = input.nav == Direction::left ? 0 : 1;
                if (next != answer_)
                {
                    pane_ghost_.leave(answer_bar(answer_));
                    answer_ = next;
                    feedback.play(audio::Cue::tick, next == 0 ? 0.94f : 1.0f, pane_pan());
                }
                else if (!input.nav_repeat)
                {
                    refuse(feedback);
                }
            }
            else if (input.nav != Direction::none && !input.nav_repeat)
            {
                refuse(feedback);
            }
            if (confirm && answer_ == 0)
            {
                // One low thud for the relay, and the picture collapses.
                opened_ = false;
                mode_ = Mode::power_off;
                power_.start(context_.settings.reduced_motion ? 0.3f : kPowerOff);
                feedback.play(audio::Cue::drop, 0.78f);
                feedback.rumble(0.5f, 0.12f);
            }
            else if (confirm)
            {
                close_pane(feedback, audio::Cue::modal_close);
            }
            break;
        }
    }

    // ---- drawing: boot ----
    void draw_boot(const Screen &s, float age, bool cursor) const
    {
        const bool reduced = context_.settings.reduced_motion;

        // The banner appears a pixel row at a time. Each run of lit pixels is
        // one rectangle, and the rows keep a hairline between them so the
        // letters look scanned rather than printed.
        const int logo_rows =
            reduced ? 5 : std::clamp(static_cast<int>(age / kBootLogo * 5.0f) + 1, 0, 5);
        const float pixel = kLine * 0.5f;
        s.list.glow(
            {s.x(kBootCol), s.y(kLogoRow), 47.0f * s.cell, static_cast<float>(logo_rows) * pixel},
            4.0f, 26.0f, s.ink.with_alpha(0.07f));
        for (int row = 0; row < logo_rows; ++row)
        {
            for (int letter = 0; letter < kLogoLetters; ++letter)
            {
                const std::uint8_t bits = kLetters[kLogo[letter]][row];
                for (int col = 0; col < 5; ++col)
                {
                    if (((bits >> (4 - col)) & 1) == 0)
                        continue;
                    int run = 1;
                    while (col + run < 5 && ((bits >> (4 - col - run)) & 1) != 0)
                        ++run;
                    s.list.rounded_rect({s.x(static_cast<float>(kBootCol + letter * 6 + col)),
                                         s.y(kLogoRow) + static_cast<float>(row) * pixel,
                                         static_cast<float>(run) * s.cell, pixel - 3.0f},
                                        0.0f, s.ink);
                    col += run;
                }
            }
        }

        BootState state = boot_state(age);
        if (reduced)
        {
            state = {};
            state.line = kBootLines;
            state.done = true;
        }
        static constexpr std::string_view kDots = "..............................";
        float cursor_col = static_cast<float>(kBootCol + 2);
        float cursor_row = static_cast<float>(kBootPromptRow);
        for (int i = 0; i < kBootLines && i <= state.line; ++i)
        {
            const BootLine &line = kBoot[i];
            const bool complete = i < state.line;
            const int length = static_cast<int>(std::strlen(line.label));
            const int label = complete ? length : state.label;
            const int dots = complete ? boot_dots(line) : state.dots;
            const bool result = line.result[0] != '\0' && (complete || state.result);
            const float row = static_cast<float>(line.row);
            s.text(kBootCol, row, prefix(line.label, label), i == 0 ? kBright : kNormal);
            s.text(static_cast<float>(kBootCol + length + 1), row,
                   kDots.substr(0, static_cast<std::size_t>(dots)), kDim);
            if (result)
                s.text(kBootCol + kBootDotsTo + 1, row, line.result, kBright);
            if (!complete)
            {
                cursor_row = row;
                cursor_col = static_cast<float>(kBootCol + label);
                if (dots > 0 || (label == length && boot_dots(line) > 0))
                    cursor_col = static_cast<float>(kBootCol + length + 1 + dots);
                if (result)
                    cursor_col =
                        static_cast<float>(kBootCol + kBootDotsTo + 2 + cells(line.result));
            }
        }
        if (state.done)
            s.text(kBootCol, kBootPromptRow, ">", kBright);
        if (cursor && cursor_lit())
            s.bar(s.span(cursor_col, cursor_row, 1.0f), 1.0f);
    }

    // ---- drawing: the main view ----
    void draw_main_entering(const Screen &s) const
    {
        const bool reduced = context_.settings.reduced_motion;
        // The screen is painted from the top, a row at a time, like a slow
        // terminal receiving a page. A clip rectangle that grows in whole
        // rows does it for every shape at once.
        const int rows = static_cast<int>(age_ / kEntranceRow) + 1;
        const bool entering = !reduced && rows < kRows;
        if (entering)
            s.list.push_clip({0, 0, gfx::kVirtualWidth, s.y(static_cast<float>(rows))});
        s.list.push_opacity(reduced ? tween::clamp01(age_ / 0.2f) : 1.0f);
        draw_main(s);
        s.list.pop_opacity();
        if (entering)
            s.list.pop_clip();

        // The boot text does not vanish: its light decays under the new page.
        const float ghost = 1.0f - tween::clamp01(age_ / kAfterglow);
        if (ghost > 0.01f && boot_exit_age_ >= 0.0f)
        {
            s.list.push_opacity(0.45f * ghost * ghost);
            draw_boot(s, boot_exit_age_, false);
            s.list.pop_opacity();
        }
    }

    void draw_main(const Screen &s) const
    {
        draw_title(s);
        draw_frame(s, 0, kMenuRight, "COMMANDS", !opened_);
        draw_frame(s, kPaneLeft, kCols - 1, kCommandInfo[focus_].name, opened_);
        draw_menu(s);
        draw_pane(s);
        draw_status(s);
        draw_hints(s);
    }

    void draw_title(const Screen &s) const
    {
        // A half-intensity bar: the "dim" attribute of a text screen. Full
        // inverse video is kept for the focus alone.
        s.list.rounded_rect(s.span(0, kTitleRow, kCols), 0.0f, s.ink.with_alpha(0.15f));
        s.text(1, kTitleRow, "HOMEBREW UI LAB  v1.0", kBright);
        s.text_right(kCols - 2, kTitleRow, "PHOSPHOR TERMINAL  TTY1", kNormal);
    }

    // A box from column col1 to col2 with its title set into the top rule.
    // The box that holds the input has a double rule and an inverse title,
    // as the active window of a text-mode program does.
    void draw_frame(const Screen &s, int col1, int col2, std::string_view title, bool active) const
    {
        const float x1 = static_cast<float>(col1) + 0.5f;
        const float x2 = static_cast<float>(col2) + 0.5f;
        const float y1 = static_cast<float>(kBoxTop) + 0.5f;
        const float y2 = static_cast<float>(kBoxBottom) + 0.5f;
        const float level = active ? kBright : 0.42f;
        const float gap1 = static_cast<float>(col1 + 2);
        const float gap2 = gap1 + static_cast<float>(cells(title)) + 2.0f;
        const int passes = active ? 2 : 1;
        for (int pass = 0; pass < passes; ++pass)
        {
            // The two rules of a double border sit 3 px either side of the
            // cell centre; dx and dy are that offset in columns and rows.
            const float offset = active ? (pass == 0 ? -3.0f : 3.0f) : 0.0f;
            const float dx = offset / s.cell;
            const float dy = offset / kLine;
            s.hline(y1 + dy, x1 + dx, gap1, level);
            s.hline(y1 + dy, gap2, x2 - dx, level);
            s.hline(y2 - dy, x1 + dx, x2 - dx, level);
            s.vline(x1 + dx, y1 + dy, y2 - dy, level);
            s.vline(x2 - dx, y1 + dy, y2 - dy, level);
        }
        if (active && bar_lit())
        {
            s.bar(s.span(gap1, kBoxTop, gap2 - gap1), 1.0f);
            s.inverse(gap1 + 1.0f, kBoxTop, title);
        }
        else
        {
            s.text(gap1 + 1.0f, kBoxTop, title, active ? kBright : kNormal);
        }
    }

    void draw_menu(const Screen &s) const
    {
        const float ghost = menu_ghost_.level();
        if (ghost > 0.0f)
            s.bar(menu_ghost_.rect, ghost);
        char tag[16];
        for (int i = 0; i < kCommands; ++i)
        {
            const float row = static_cast<float>(kMenuRow + i * kMenuStep);
            const bool focused = i == focus_;
            switch (i)
            {
            case kLibrary:
                std::snprintf(tag, sizeof(tag), "%d", static_cast<int>(context_.catalog.size()));
                break;
            case kMessages:
                std::snprintf(tag, sizeof(tag), "%d", kMessageCount);
                break;
            case kSystem:
                std::snprintf(tag, sizeof(tag), "OK");
                break;
            case kNetwork:
                std::snprintf(tag, sizeof(tag), "UP");
                break;
            case kDiagnostics:
                std::snprintf(tag, sizeof(tag), "%s", hold_ ? "HOLD" : "RUN");
                break;
            case kArchive:
                std::snprintf(tag, sizeof(tag), "%d", kNodeCount);
                break;
            default:
                tag[0] = '\0';
                break;
            }
            if (focused && !opened_ && bar_lit())
            {
                // The focus: inverse video. It is drawn at the focused row,
                // not at an eased position: on a character display a cursor
                // is in one cell or in the next and never in between, so a
                // gliding bar would break the illusion. The afterglow above
                // gives the eye the trail a spring would have given it.
                s.bar(menu_bar(i), 1.0f);
                s.inverse(2, row, ">");
                s.inverse(4, row, kCommandInfo[i].name);
                s.inverse(static_cast<float>(kMenuRight - 1 - cells(tag)), row, tag);
                continue;
            }
            if (focused && opened_)
                s.list.rounded_rect(menu_bar(i), 0.0f, s.ink.with_alpha(0.15f));
            if (focused)
                s.text(2, row, ">", kBright);
            s.text(4, row, kCommandInfo[i].name, focused ? kBright : kNormal);
            s.text_right(kMenuRight - 2, row, tag, kDim);
        }

        // The note under the menu is redrawn with the page it describes.
        const float level = opened_ ? 0.42f : kFaint;
        s.hline(static_cast<float>(kNoteRow) - 0.5f, 0.5f + (opened_ ? 0.0f : 3.0f / s.cell),
                static_cast<float>(kMenuRight) + 0.5f - (opened_ ? 0.0f : 3.0f / s.cell), level);
        std::string_view lines[3];
        const int count = wrap_cells(kCommandInfo[focus_].note, kMenuRight - 3, lines, 3);
        const Pane wipe{s, pane_age_, context_.settings.reduced_motion};
        for (int i = 0; i < count; ++i)
            s.text(2, static_cast<float>(kNoteRow + i), lines[i],
                   std::min(1.0f, kDim * wipe.gain(i * 2)));
    }

    void draw_pane(const Screen &s) const
    {
        const bool reduced = context_.settings.reduced_motion;
        // The old page is not erased, it decays: for a moment both are on
        // the glass, the old one fading under the wipe of the new one.
        const float ghost = 1.0f - tween::clamp01(page_ghost_ / kAfterglow);
        if (ghost > 0.01f && previous_ != focus_)
        {
            s.list.push_opacity(0.4f * ghost * ghost);
            const Pane old{s, 99.0f, reduced};
            draw_page(old, previous_, false);
            s.list.pop_opacity();
        }

        Pane pane{s, pane_age_, reduced};
        if (opened_)
        {
            const float glow = pane_ghost_.level();
            if (glow > 0.0f)
                s.bar(pane_ghost_.rect, glow);
            if (has_rows(focus_) && bar_lit())
            {
                s.bar(pane_bar(focus_), 1.0f);
                pane.inverse_row = cursor_row(focus_);
            }
        }
        draw_page(pane, focus_, opened_);
    }

    void draw_page(const Pane &p, int page, bool live) const
    {
        switch (page)
        {
        case kLibrary:
            draw_library(p);
            break;
        case kMessages:
            draw_messages(p);
            break;
        case kSystem:
            draw_system(p);
            break;
        case kNetwork:
            draw_network(p);
            break;
        case kDiagnostics:
            draw_diagnostics(p);
            break;
        case kArchive:
            draw_archive(p);
            break;
        default:
            draw_shutdown(p, live);
            break;
        }
    }

    // A paragraph that types itself: `budget` characters are visible.
    void draw_typed(const Pane &p, int col, int row, std::string_view text, int width,
                    int max_lines, float level) const
    {
        std::string_view lines[8];
        const int count = wrap_cells(text, width, lines, std::min(max_lines, 8));
        int budget = 100000;
        if (p.age < 50.0f && !p.reduced)
            budget = static_cast<int>(detail_age_ * kDetailRate);
        for (int i = 0; i < count && budget > 0; ++i)
        {
            p.text(col, row + i, prefix(lines[i], budget), level);
            budget -= static_cast<int>(lines[i].size());
        }
    }

    void draw_library(const Pane &p) const
    {
        const ui::FontRef &mono = context_.fonts.mono;
        const int count = static_cast<int>(context_.catalog.size());
        char text[96];
        p.text(2, 0, "TITLE", kDim);
        p.text(25, 0, "GENRE", kDim);
        p.right(45, 0, "HOURS", kDim);
        p.text(48, 0, "PROGRESS", kDim);
        p.rule(1);
        for (int i = 0; i < kLibraryRows && library_top_ + i < count; ++i)
        {
            const int index = library_top_ + i;
            const int row = 2 + i;
            const demo::Item &item = context_.catalog[static_cast<std::size_t>(index)];
            if (marked_[static_cast<std::size_t>(index)])
                p.text(0, row, "*", kBright);
            p.text(2, row, prefix(item.title, 22));
            p.text(25, row, prefix(item.genre, 15));
            std::snprintf(text, sizeof(text), "%d", item.hours);
            p.right(45, row, text);
            // The progress bar is fourteen cells, each a block character
            // drawn as a rectangle so that lit and unlit cells line up.
            const int lit = static_cast<int>(item.progress * 14.0f + 0.5f);
            for (int c = 0; c < 14; ++c)
                p.s.list.rounded_rect({p.x(static_cast<float>(48 + c)) + 1.5f,
                                       p.y(static_cast<float>(row)) + 12.0f, p.s.cell - 3.0f,
                                       kLine - 24.0f},
                                      0.0f, p.ink(row, c < lit ? kNormal : 0.18f));
            std::snprintf(text, sizeof(text), "%d%%",
                          static_cast<int>(item.progress * 100.0f + 0.5f));
            p.right(66, row, text);
        }
        // More above, more below.
        if (library_top_ > 0 && mono.font->has_glyph(0x25b2))
            p.text(68, 2, "\xE2\x96\xB2", kDim);
        if (library_top_ + kLibraryRows < count && mono.font->has_glyph(0x25bc))
            p.text(68, 1 + kLibraryRows, "\xE2\x96\xBC", kDim);
        p.rule(13);

        const int cursor = cursor_[kLibrary];
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(cursor)];
        std::snprintf(text, sizeof(text), "%s  -  %s, %d", item.title, item.studio, item.year);
        p.text(0, 14, prefix(text, kPaneCols), kBright);
        draw_typed(p, 0, 15, item.blurb, kPaneCols, 2, kNormal);
        int marks = 0;
        for (const bool mark : marked_)
            marks += mark ? 1 : 0;
        std::snprintf(text, sizeof(text), "%02d-%02d OF %d", library_top_ + 1,
                      std::min(count, library_top_ + kLibraryRows), count);
        p.text(0, 17, text, kDim);
        std::snprintf(text, sizeof(text), "%d MARKED", marks);
        p.right(68, 17, text, kDim);
    }

    void draw_messages(const Pane &p) const
    {
        const ui::FontRef &mono = context_.fonts.mono;
        char text[32];
        p.text(2, 0, "FROM", kDim);
        p.text(16, 0, "SUBJECT", kDim);
        p.right(68, 0, "DATE", kDim);
        p.rule(1);
        int flags = 0;
        for (int i = 0; i < kMessageCount; ++i)
        {
            const Message &message = kMail[i];
            const int row = 2 + i;
            if (flagged_[i])
            {
                p.text(0, row, mono.font->has_glyph(0x25cf) ? "\xE2\x97\x8F" : "*", kBright);
                ++flags;
            }
            p.text(2, row, message.from);
            p.text(16, row, message.subject);
            p.right(68, row, message.date, kDim);
        }
        p.rule(7);
        const Message &message = kMail[cursor_[kMessages]];
        p.text(0, 8, message.subject, kBright);
        draw_typed(p, 0, 10, message.body, 60, 6, kNormal);
        std::snprintf(text, sizeof(text), "%d FLAGGED", flags);
        p.text(0, 17, text, kDim);
    }

    void draw_system(const Pane &p) const
    {
        const app::Telemetry &telemetry = context_.telemetry;
        char text[64];
        const auto field = [&](int col, int row, const char *label)
        {
            p.text(col, row, label, kDim);
            p.right(col + 28, row, text, kBright);
        };
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(telemetry.fps));
        field(0, 0, "FRAMES PER SECOND");
        std::snprintf(text, sizeof(text), "%.2f ms", static_cast<double>(telemetry.average_ms));
        field(0, 1, "FRAME TIME");
        std::snprintf(text, sizeof(text), "%d", telemetry.voices);
        field(0, 2, "VOICES");
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(telemetry.draw_calls));
        field(40, 0, "DRAW CALLS");
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(telemetry.instances));
        field(40, 1, "SHAPES");
        format_uptime(text, sizeof(text));
        field(40, 2, "UPTIME");
        p.rule(3);

        const int window = kWindows[window_];
        std::snprintf(text, sizeof(text), "FRAME TIME, LAST %d FRAMES", window);
        p.text(0, 4, text, kDim);
        p.right(68, 4, "BUDGET 16.67 MS", kDim);
        p.right(3, 5, "33", kDim);
        p.right(3, 10, "16", kDim);
        p.right(3, 15, "0", kDim);

        // The graph: one rectangle per frame, newest on the right, against a
        // dashed line at the 60 Hz budget.
        if (p.begin_graphics(5, 16))
        {
            const float x1 = p.x(5.0f);
            const float width = 64.0f * p.s.cell;
            const float base = p.y(16.0f) - 14.0f;
            const float height = base - (p.y(5.0f) + 14.0f);
            p.s.vline(static_cast<float>(kPaneCol) + 4.5f, static_cast<float>(kPaneRow) + 5.2f,
                      static_cast<float>(kPaneRow) + 15.8f, kDim);
            p.s.list.line(x1 - p.s.cell * 0.5f, base + 4.0f, x1 + width, base + 4.0f, kRule,
                          p.s.ink.with_alpha(kDim));
            const float pitch = width / static_cast<float>(window);
            const float bar = std::max(1.5f, pitch - (pitch > 6.0f ? 3.0f : 1.5f));
            for (int i = 0; i < window; ++i)
            {
                const float ms = telemetry.sample(static_cast<std::size_t>(window - 1 - i));
                const float h = std::max(2.0f, tween::clamp01(ms / 33.33f) * height);
                p.s.list.rounded_rect({x1 + static_cast<float>(i) * pitch, base - h, bar, h}, 0.0f,
                                      p.s.ink.with_alpha(ms > 17.5f ? kBright : 0.62f));
            }
            const float budget = base - height * 0.5f;
            for (int dash = 0; dash < 32; ++dash)
                p.s.list.line(x1 + static_cast<float>(dash) * 2.0f * p.s.cell, budget,
                              x1 + (static_cast<float>(dash) * 2.0f + 1.0f) * p.s.cell, budget,
                              kRule, p.s.ink.with_alpha(kDim));
            p.end_graphics();
        }
        p.text(5, 16, "OLDER", kDim);
        p.right(68, 16, "NOW", kDim);
        std::snprintf(text, sizeof(text), "WINDOW %d FRAMES", window);
        p.text(0, 17, text, kDim);
    }

    void draw_network(const Pane &p) const
    {
        char text[48];
        const auto field = [&](int col, int row, const char *label, const char *value)
        {
            p.text(col, row, label, kDim);
            p.text(col + 12, row, value, kBright);
        };
        field(0, 0, "INTERFACE", "lan0");
        field(0, 1, "ADDRESS", "10.0.4.42");
        field(0, 2, "RECEIVED", "48.6 MB");
        field(36, 0, "LINK", "UP, 1000 MBIT");
        field(36, 1, "GATEWAY", "10.0.4.1");
        field(36, 2, "SENT", "1.2 MB");
        p.rule(3);
        p.text(2, 4, "HOST", kDim);
        p.text(22, 4, "ADDRESS", kDim);
        p.text(42, 4, "ECHO", kDim);
        p.rule(5);
        for (int i = 0; i < kHostCount; ++i)
        {
            const Host &host = kHosts[i];
            const int row = kHostRow + i;
            p.text(2, row, host.name);
            p.text(22, row, host.address);
            if (i == ping_host_)
            {
                // Four requests go out; a dot is printed for each.
                static constexpr std::string_view kWaiting = "waiting ....";
                const int dots = std::min(4, static_cast<int>(ping_age_ / kPing * 5.0f));
                p.text(42, row, kWaiting.substr(0, static_cast<std::size_t>(8 + dots)));
            }
            else if (ping_result_[i] == 1)
            {
                std::snprintf(text, sizeof(text), "%d ms, 4 of 4", host.reply_ms);
                p.text(42, row, text, kBright);
            }
            else if (ping_result_[i] == 2)
            {
                p.text(42, row, "no reply", kBright);
            }
            else
            {
                p.text(42, row, "--", kDim);
            }
        }
        p.rule(11);
        p.text(0, 12, "LAST EVENTS", kDim);
        p.text(0, 13, "21:40:02  link up, carrier at 1000 mbit");
        p.text(0, 14, "21:40:03  address 10.0.4.42 leased for 12 h");
        p.text(0, 15, "21:47:55  archive: 3 files synchronised");
        p.text(0, 17, "CARRIER DETECTED", kDim);
    }

    void draw_diagnostics(const Pane &p) const
    {
        const Screen &s = p.s;
        char text[32];
        p.text(0, 0, "SCOPE  CH1", kDim);
        p.text(44, 0, "SWEEP", kDim);
        if (p.begin_graphics(1, 11))
        {
            // Oscilloscope: a graticule of faint rules and a sine trace made
            // of short segments, each drawn wide and faint, then thin and
            // bright, like the glow under the text.
            const Rect scope{p.x(0.0f), p.y(1.0f) + 8.0f, 40.0f * s.cell, 11.0f * kLine - 16.0f};
            s.list.bordered_rect(scope, 0.0f, s.ink.with_alpha(0.03f), kRule,
                                 s.ink.with_alpha(kDim));
            for (int i = 1; i < 8; ++i)
            {
                const float gx = scope.x + scope.w * static_cast<float>(i) / 8.0f;
                s.list.line(gx, scope.y + 4, gx, scope.y + scope.h - 4, 1.5f,
                            s.ink.with_alpha(i == 4 ? 0.3f : 0.13f));
            }
            for (int i = 1; i < 4; ++i)
            {
                const float gy = scope.y + scope.h * static_cast<float>(i) / 4.0f;
                s.list.line(scope.x + 4, gy, scope.x + scope.w - 4, gy, 1.5f,
                            s.ink.with_alpha(i == 2 ? 0.3f : 0.13f));
            }
            constexpr int kSegments = 72;
            const float cycles = 0.5f + 0.5f * static_cast<float>(frequency_);
            float last_x = 0.0f;
            float last_y = 0.0f;
            for (int i = 0; i <= kSegments; ++i)
            {
                const float u = static_cast<float>(i) / static_cast<float>(kSegments);
                const float tx = scope.x + 10.0f + (scope.w - 20.0f) * u;
                const float ty =
                    scope.cy() -
                    scope.h * 0.36f * std::sin(6.2831853f * (cycles * u - scope_time_ * 0.7f));
                if (i > 0)
                {
                    s.list.line(last_x, last_y, tx, ty, 9.0f, s.ink.with_alpha(0.13f));
                    s.list.line(last_x, last_y, tx, ty, 3.0f, s.ink);
                }
                last_x = tx;
                last_y = ty;
            }

            // Radar sweep: the beam is a fan of thin sectors that fade away
            // from the leading edge, and the blips light as it passes them.
            const float radius = scope.h * 0.5f;
            const float cx = p.x(56.5f);
            const float cy = scope.cy();
            for (int i = 1; i <= 3; ++i)
                s.list.ring(cx, cy, radius * static_cast<float>(i) / 3.0f, kRule,
                            s.ink.with_alpha(i == 3 ? kDim : 0.2f));
            s.list.line(cx - radius, cy, cx + radius, cy, 1.5f, s.ink.with_alpha(0.2f));
            s.list.line(cx, cy - radius, cx, cy + radius, 1.5f, s.ink.with_alpha(0.2f));
            const float beam = std::fmod(scope_time_ * 1.5f, 6.2831853f);
            constexpr int kFan = 10;
            constexpr float kSlice = 0.11f;
            for (int i = 0; i < kFan; ++i)
            {
                const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(kFan);
                s.list.arc(cx, cy, radius - 2.0f, radius - 2.0f,
                           beam - static_cast<float>(i + 1) * kSlice, kSlice + 0.01f,
                           s.ink.with_alpha(0.32f * fade * fade), false);
            }
            s.list.line(cx, cy, cx + std::sin(beam) * (radius - 3.0f),
                        cy - std::cos(beam) * (radius - 3.0f), 2.5f, s.ink);
            constexpr float kBlips[][2] = {
                {0.7f, 0.55f}, {2.2f, 0.8f}, {3.6f, 0.38f}, {5.1f, 0.7f}};
            for (const auto &blip : kBlips)
            {
                float behind = beam - blip[0];
                if (behind < 0.0f)
                    behind += 6.2831853f;
                const float lit = std::exp(-behind * 0.8f);
                const float bx = cx + std::sin(blip[0]) * radius * blip[1];
                const float by = cy - std::cos(blip[0]) * radius * blip[1];
                s.list.glow({bx - 4, by - 4, 8, 8}, 4.0f, 12.0f, s.ink.with_alpha(0.5f * lit));
                s.list.circle(bx, by, 4.5f, s.ink.with_alpha(0.15f + 0.85f * lit));
            }
            p.end_graphics();
        }

        // Sixteen steps of brightness: the classic grey-scale check.
        p.text(0, 13, "RAMP", kDim);
        for (int i = 0; i < 16; ++i)
            s.list.rounded_rect({p.x(static_cast<float>(8 + i * 3)), p.y(13.0f) + 6.0f,
                                 3.0f * s.cell - 2.0f, kLine - 12.0f},
                                0.0f, p.ink(13, static_cast<float>(i + 1) / 16.0f));
        p.right(68, 13, "16 LEVELS", kDim);

        p.text(0, 15, "RATE", kDim);
        for (int i = 0; i < kMaxFrequency; ++i)
            s.list.rounded_rect({p.x(static_cast<float>(8 + i * 3)), p.y(15.0f) + 12.0f,
                                 3.0f * s.cell - 4.0f, kLine - 24.0f},
                                0.0f, p.ink(15, i < frequency_ ? kNormal : 0.18f));
        std::snprintf(text, sizeof(text), "%.1f CYCLES", 0.5 + 0.5 * frequency_);
        p.text(34, 15, text, kBright);
        p.right(68, 15, hold_ ? "HOLD" : "RUNNING", hold_ ? kBright : kDim);
        p.text(0, 17, "TRACE AND SWEEP NOMINAL", kDim);
    }

    void draw_archive(const Pane &p) const
    {
        const Screen &s = p.s;
        char text[32];
        int nodes[kNodeCount];
        const int count = visible_nodes(nodes);
        int tags = 0;
        for (const bool tag : tagged_)
            tags += tag ? 1 : 0;
        p.text(0, 0, "/archive", kBright);
        p.right(68, 0, "SIZE     DATE", kDim);
        for (int k = 0; k < count; ++k)
        {
            const Node &node = kNodes[nodes[k]];
            const int row = 1 + k;
            const float visible = std::min(1.0f, p.gain(row));
            if (visible <= 0.01f)
                continue;
            // The branches are rules through the centres of cells. A trunk
            // continues below this row at level l when a later row is a
            // sibling at that level, before the tree climbs above it.
            const Color branch = row == p.inverse_row ? s.dark : s.ink.with_alpha(kDim * visible);
            const float y1 = p.y(static_cast<float>(row));
            for (int level = 0; level <= node.depth; ++level)
            {
                bool continues = false;
                for (int j = k + 1; j < count; ++j)
                {
                    const int depth = kNodes[nodes[j]].depth;
                    if (depth <= level)
                    {
                        continues = depth == level;
                        break;
                    }
                }
                const float tx = p.x(static_cast<float>(level * 4) + 1.5f);
                const bool elbow = level == node.depth;
                if (continues)
                    s.list.line(tx, y1, tx, y1 + kLine, kRule, branch);
                else if (elbow)
                    s.list.line(tx, y1, tx, y1 + kLine * 0.5f, kRule, branch);
                if (elbow)
                    s.list.line(tx, y1 + kLine * 0.5f, tx + 1.8f * s.cell, y1 + kLine * 0.5f, kRule,
                                branch);
            }
            const int col = node.depth * 4 + 4;
            if (node.folder)
            {
                std::snprintf(text, sizeof(text), "%s %s/", expanded_[nodes[k]] ? "-" : "+",
                              node.name);
                p.text(col, row, text, kBright);
            }
            else
            {
                p.text(col, row, node.name);
                if (tagged_[nodes[k]])
                    p.text(col + cells(node.name) + 1, row, "*", kBright);
                p.right(59, row, node.size);
                p.right(68, row, node.date, kDim);
            }
        }
        std::snprintf(text, sizeof(text), "%d ITEMS, %d TAGGED", kNodeCount, tags);
        p.text(0, 17, text, kDim);
    }

    void draw_shutdown(const Pane &p, bool live) const
    {
        const Screen &s = p.s;
        char text[32];
        p.text(0, 0, "Power off the display and restart the lab.");
        p.text(0, 1, "Your place in every command is kept.");
        p.text(0, 3, "SESSION", kDim);
        format_uptime(text, sizeof(text));
        p.text(12, 3, text, kBright);

        // The power symbol: an open ring and a bar, two primitives.
        if (p.begin_graphics(4, 13))
        {
            const float cx = p.x(54.0f);
            const float cy = p.y(9.0f);
            const float level = live ? kBright : kDim;
            s.list.arc(cx, cy, 104.0f, 22.0f, 0.62f, 6.2831853f - 1.24f,
                       s.ink.with_alpha(0.1f * level));
            s.list.arc(cx, cy, 96.0f, 6.0f, 0.62f, 6.2831853f - 1.24f, s.ink.with_alpha(level));
            s.list.line(cx, cy - 122.0f, cx, cy - 44.0f, 6.0f, s.ink.with_alpha(level));
            p.end_graphics();
        }

        if (!live)
        {
            p.text(0, 6, "Open this command to continue.", kDim);
            return;
        }
        // The question types itself; the answers are two fields with the
        // inverse bar on one of them. N is where the bar starts.
        static constexpr std::string_view kQuestion = "Shut down? [Y/N]";
        const int typed = p.reduced ? 99 : static_cast<int>(prompt_age_ * 60.0f);
        p.text(0, 6, prefix(kQuestion, typed), kBright);
        if (typed < static_cast<int>(kQuestion.size()))
            return;
        const bool lit = bar_lit();
        for (int answer = 0; answer < 2; ++answer)
        {
            const char *label = answer == 0 ? "YES" : "NO";
            const float col = static_cast<float>(kPaneCol + (answer == 0 ? 3 : 11));
            const float row = static_cast<float>(kPaneRow + 8);
            if (answer == answer_ && lit)
            {
                s.bar(answer_bar(answer), 1.0f);
                s.inverse(col, row, label);
            }
            else
            {
                s.text(col, row, label, answer == answer_ ? kBright : kNormal);
            }
        }
    }

    void format_uptime(char *out, std::size_t size) const
    {
        // The app's uptime when the platform reports one, this design's own
        // clock otherwise (the PC snapshot tool reports none).
        const double seconds = context_.telemetry.uptime > 0.0 ? context_.telemetry.uptime
                                                               : static_cast<double>(clock_);
        const int total = static_cast<int>(std::min(seconds, 359999.0));
        std::snprintf(out, size, "%02d:%02d:%02d", total / 3600, total / 60 % 60, total % 60);
    }

    void draw_status(const Screen &s) const
    {
        char text[48];
        format_uptime(text, sizeof(text));
        s.text(0, kStatusRow, text, kNormal);
        s.text(10, kStatusRow, kThemes[theme_].name, kDim);
        // A shell prompt that says where the input is.
        std::snprintf(text, sizeof(text), "lab:/%s>", opened_ ? kCommandInfo[focus_].path : "");
        s.text(17, kStatusRow, text, kNormal);
        if (cursor_lit())
            s.bar(s.span(static_cast<float>(18 + cells(text)), kStatusRow, 1.0f), 1.0f);
    }

    // The hint row lives on the grid like everything else: a glyph takes
    // two cells, its label follows after one more, in the terminal's face.
    void draw_hints(const Screen &s) const
    {
        struct Hint
        {
            ui::Button button;
            const char *label;
        };
        Hint hints[4];
        int count = 0;
        if (mode_ == Mode::boot)
        {
            hints[count++] = {ui::Button::cross, "SKIP"};
        }
        else if (!opened_)
        {
            hints[count++] = {ui::Button::dpad, "SELECT"};
            hints[count++] = {ui::Button::cross, "OPEN"};
            hints[count++] = {ui::Button::square, "THEME"};
        }
        else
        {
            switch (focus_)
            {
            case kLibrary:
                hints[count++] = {ui::Button::cross, "MARK"};
                break;
            case kMessages:
                hints[count++] = {ui::Button::cross, "FLAG"};
                break;
            case kSystem:
                hints[count++] = {ui::Button::dpad, "WINDOW"};
                break;
            case kNetwork:
                hints[count++] = {ui::Button::cross, "PING"};
                break;
            case kDiagnostics:
                hints[count++] = {ui::Button::dpad, "RATE"};
                hints[count++] = {ui::Button::cross, hold_ ? "RUN" : "HOLD"};
                break;
            case kArchive:
            {
                int nodes[kNodeCount];
                const int visible = visible_nodes(nodes);
                const int node = nodes[std::min(cursor_[kArchive], visible - 1)];
                hints[count++] = {ui::Button::cross, !kNodes[node].folder ? "TAG"
                                                     : expanded_[node]    ? "CLOSE"
                                                                          : "OPEN"};
                break;
            }
            default:
                hints[count++] = {ui::Button::cross, "CONFIRM"};
                break;
            }
            hints[count++] = {ui::Button::circle, focus_ == kShutdown ? "CANCEL" : "MENU"};
            if (focus_ != kShutdown)
                hints[count++] = {ui::Button::square, "THEME"};
        }

        int total = 0;
        for (int i = 0; i < count; ++i)
            total += 3 + cells(hints[i].label) + (i + 1 < count ? 3 : 0);
        float col = static_cast<float>(kCols - total);
        const ui::GlyphStyle style = ui::GlyphStyle::mono(s.ink, s.dark);
        const float cy = s.y(kStatusRow) + kLine * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            const float gx = s.x(col) + (2.0f * s.cell - kGlyph) * 0.5f;
            // In the mono style a face button is only its symbol; a ring of
            // phosphor gives it the outline of a key.
            ui::draw_button(s.list, mono_fonts_, style, hints[i].button, gx, cy, kGlyph);
            if (hints[i].button != ui::Button::dpad)
                s.list.ring(gx + kGlyph * 0.5f, cy, kGlyph * 0.5f, 1.5f, s.ink.with_alpha(0.7f));
            s.text(col + 3.0f, kStatusRow, hints[i].label, kNormal);
            col += static_cast<float>(3 + cells(hints[i].label) + 3);
        }
    }

    // ---- drawing: power-off ----
    void draw_power_off(const Screen &s) const
    {
        const float t = power_.progress();
        if (context_.settings.reduced_motion)
        {
            s.list.push_opacity(1.0f - t);
            draw_main(s);
            s.list.pop_opacity();
            return;
        }
        // When a tube loses its deflection the picture is squeezed into a
        // horizontal line, the line into a dot, and the dot fades. The kit
        // scales uniformly only, so the squeeze is a clip rectangle closing
        // on the centre while a bar of light, all of the picture's energy
        // in one place, grows brighter inside it.
        const float squeeze = tween::clamp01(t / 0.5f);
        const float shrink = tween::clamp01((t - 0.5f) / 0.35f);
        const float fade = tween::clamp01((t - 0.85f) / 0.15f);
        const float half_h =
            tween::lerp(gfx::kVirtualHeight * 0.5f, 1.5f, tween::cubic_out(squeeze));
        const float half_w = tween::lerp(gfx::kVirtualWidth * 0.5f, 4.0f, tween::cubic_out(shrink));
        const Rect band{960.0f - half_w, 540.0f - half_h, 2.0f * half_w, 2.0f * half_h};
        s.list.push_clip(band);
        s.list.push_opacity(1.0f - tween::smoothstep(squeeze * 1.3f));
        draw_main(s);
        s.list.pop_opacity();
        s.list.pop_clip();
        const float glare = tween::smoothstep(squeeze) * (1.0f - fade);
        const float radius = shrink > 0.0f ? std::min(half_h, half_w) : 0.0f;
        const Color hot = gfx::mix(s.ink, Color::rgb(0xffffff), 0.7f).with_alpha(glare);
        const Color rim = gfx::mix(s.ink, Color::rgb(0xffffff), 0.25f).with_alpha(0.75f * glare);
        s.list.glow(band, radius, 46.0f, s.ink.with_alpha(0.55f * glare));
        if (shrink > 0.0f)
        {
            s.list.rounded_rect(band, radius, hot);
        }
        else
        {
            // Hottest in the middle, where the beam now spends all its time.
            const Rect half{band.x, band.y, band.w * 0.5f, band.h};
            s.list.gradient_rect_h(half, 0.0f, rim, hot);
            s.list.gradient_rect_h({half.x + half.w, half.y, half.w, half.h}, 0.0f, hot, rim);
        }
    }

    app::Context &context_;
    ui::Fonts mono_fonts_; // every face is the terminal's: glyph lettering stays in character
    float cell_ = 16.0f;
    float left_ = 96.0f;
    float top_ = 65.0f;

    Mode mode_ = Mode::boot;
    float age_ = 0.0f;   // seconds in the current mode: boot text, entrance
    float clock_ = 0.0f; // free-running: cursor blink, flicker, uptime
    int boot_key_ = 0;   // how much boot text had been typed last frame
    float type_cooldown_ = 0.0f;
    float boot_exit_age_ = -1.0f; // what the boot showed when it ended (its afterglow)

    int theme_ = 0;
    ui::SpringColor phosphor_;

    int focus_ = 0;           // the command under the bar
    bool opened_ = false;     // the right pane has the input
    int previous_ = 0;        // the page that is fading away
    float pane_age_ = 0.0f;   // drives the wipe of the page
    float page_ghost_ = 1.0f; // seconds since the page changed
    float detail_age_ = 0.0f; // drives the typed paragraphs
    float prompt_age_ = 0.0f;
    Afterglow menu_ghost_;
    Afterglow pane_ghost_;
    ui::Pulse bell_;

    int cursor_[kCommands] = {};
    int library_top_ = 0;
    std::vector<bool> marked_;
    bool flagged_[kMessageCount] = {};
    bool expanded_[kNodeCount] = {};
    bool tagged_[kNodeCount] = {};
    int ping_host_ = -1;
    float ping_age_ = 0.0f;
    int ping_result_[kHostCount] = {}; // 0 not asked, 1 answered, 2 no reply
    int window_ = 1;
    int frequency_ = 3;
    bool hold_ = false;
    float scope_time_ = 0.0f;
    int answer_ = 1; // 0 yes, 1 no
    tween::Timer power_;
};

} // namespace

std::unique_ptr<app::Concept> make_terminal(app::Context &context)
{
    return std::make_unique<Terminal>(context);
}

} // namespace hui::concepts
