// ps5-homebrew-ui - Design "File Browser": a file manager worth looking at.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen every homebrew ends up needing: places on the left, the entries
// of one folder in the middle, a preview on the right. It works on an invented
// tree held in memory and never touches the real file system. What makes it
// feel finished:
//
//   - numbers are set in the monospaced face and right-aligned, so sizes and
//     dates form columns the eye can run down;
//   - entering a folder slides the list away and brings the new one in from
//     the right, the breadcrumb grows by one segment, and going back lands on
//     the folder you came from: every folder remembers its focus and scroll;
//   - sorting does not redraw the list: each row keeps a spring for its slot
//     and travels to its new place, and the focus stays on the same entry;
//   - operations have results you can watch: a copy fills a measured bar and
//     the entries then exist in the destination, deleted rows collapse in a
//     stagger while the storage gauge runs down to its new value;
//   - the context menu is a frosted popover pinned to the focused row that
//     slides to stay on screen, and actions that make no sense for several
//     items are dimmed and say why;
//   - sounds carry state: rows lower in the list tick lower, marks climb with
//     the number selected, and a finished operation sounds as full as the
//     drive now is.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

// ---- the design language --------------------------------------------------

const Color kWhite = Color::rgb(0xffffff);
const Color kInk = Color::rgb(0xe8ecf3);
const Color kGraphite = Color::rgb(0x14171c);
const Color kPanel = Color::rgb(0x191d24);
const Color kAccent = Color::rgb(0x5aa9ff);
const Color kDanger = Color::rgb(0xff6f6f);
const Color kWarn = Color::rgb(0xf0c463);
const Color kSystem = Color::rgb(0x5d6675);
const Color kClear = Color::rgb(0x000000, 0.0f);

constexpr float kMargin = 96.0f;
constexpr float kRight = 1824.0f;
constexpr float kRailX = kMargin; // the places rail
constexpr float kRailW = 264.0f;
constexpr float kRailGap = 56.0f; // between the drives and the pinned folders
constexpr float kListX = 392.0f;  // the entry list
constexpr float kListW = 936.0f;
constexpr float kPreviewX = 1360.0f;
constexpr float kPreviewW = 464.0f;
constexpr float kHeaderY = 203.0f; // baseline of the column captions
constexpr float kListTop = 220.0f;
constexpr float kRowH = 52.0f;
constexpr int kRows = 13; // rows in view
constexpr float kListH = kRowH * static_cast<float>(kRows);
// Columns, as offsets from the list's left edge. Size and date are right
// edges: mono digits line up when they end on the same pixel.
constexpr float kIconX = 36.0f;
constexpr float kNameX = 66.0f;
constexpr float kNameW = 440.0f;
constexpr float kKindX = 556.0f;
constexpr float kSizeX = 756.0f;
constexpr float kDateX = 912.0f;
constexpr float kCheckShift = 30.0f; // a selected row makes room for its check
constexpr float kNameSize = 24.0f;
constexpr float kMonoSize = 20.0f;
constexpr float kRadius = 10.0f;       // rows, buttons, small panels
constexpr float kDialogRadius = 22.0f; // glass
constexpr float kBarY = 912.0f;        // the selection bar, when it is up
constexpr float kGhostTime = 0.3f;     // a deleted row's collapse

constexpr std::uint64_t kKB = 1024;
constexpr std::uint64_t kMB = kKB * 1024;
constexpr std::uint64_t kGB = kMB * 1024;

constexpr float kMenuW = 300.0f;
constexpr float kMenuHead = 46.0f;
constexpr float kMenuItemH = 50.0f;
constexpr float kMenuPad = 10.0f;
constexpr float kPickRowH = 48.0f;
constexpr int kPickRows = 9;

constexpr const char *kTechniques[] = {
    "Mono digits on shared right edges: sizes and dates read as columns",
    "Per-row slot springs: sorting and deleting move rows instead of redrawing them",
    "Folder transitions slide with the direction of travel; every folder keeps its focus",
    "Frosted context popover pinned to the focused row, kept on screen at the edges",
    "Operations with visible results: measured progress, collapsing rows, a live gauge",
    "File-kind icons assembled from kit shapes, tinted by kind",
    "Cues that carry state: row pitch, a climbing mark, a drive that sounds as full as it is",
};

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)}, // into Captures
    {0.5f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)}, // into the long folder
    {0.6f, action_bit(Action::north)},   // sort by size: rows travel
    {0.5f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, action_bit(Action::west)},
    {0.2f, 0, Direction::down},
    {0.2f, action_bit(Action::west)},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, action_bit(Action::west)},
    {1.0f, action_bit(Action::menu), Direction::none, "selection"},
    {0.9f, action_bit(Action::confirm), Direction::none, "menu"}, // Copy
    {0.4f, action_bit(Action::jump_next)},                        // to the USB drive
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.9f, action_bit(Action::confirm), Direction::none, "picker"},
    {1.4f, 0, Direction::none, "copying"},
    {1.6f, action_bit(Action::menu)}, // copied; one entry now: the focused one
    {0.2f, 0, Direction::down},
    {0.15f, 0, Direction::down},
    {0.15f, 0, Direction::down},
    {0.15f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)}, // Delete asks first
    {0.9f, 0, Direction::right, "delete"},
    {0.3f, action_bit(Action::confirm)},
    {1.6f, action_bit(Action::menu)},
    {0.2f, 0, Direction::down},
    {0.15f, 0, Direction::down},
    {0.15f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)}, // Rename
    {0.5f, 0, Direction::down},
    {0.9f, action_bit(Action::confirm), Direction::none, "rename"},
    {1.2f, action_bit(Action::confirm)}, // a file opens its details
    {0.9f, action_bit(Action::back), Direction::none, "details"},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::back)},
    {0.5f, action_bit(Action::north)}, // around the sorts, back to names
    {0.3f, action_bit(Action::north)},
    {0.3f, action_bit(Action::north)},
};

// ---- the model: an invented tree ------------------------------------------

enum class Kind : std::uint8_t
{
    folder,
    image,
    music,
    video,
    archive,
    save,
    text,
    app,
};
constexpr int kKinds = 8;

struct KindStyle
{
    const char *label;
    std::uint32_t tint;
};
constexpr KindStyle kKindStyles[kKinds] = {
    {"Folder", 0x6fb2ff},  {"Image", 0x5fd6b0},     {"Audio", 0xb9a2ff}, {"Video", 0xff9479},
    {"Archive", 0xe8c35c}, {"Save data", 0x63d2f2}, {"Text", 0xb4bdca},  {"App", 0xff9cc4},
};

Color tint_of(Kind kind)
{
    return Color::rgb(kKindStyles[static_cast<int>(kind)].tint);
}
const char *label_of(Kind kind)
{
    return kKindStyles[static_cast<int>(kind)].label;
}

struct Node
{
    std::string name;
    Kind kind = Kind::folder;
    std::uint64_t size = 0; // a file's bytes; folders are measured
    int date = 0;           // yyyymmdd
    int time = 0;           // hhmm
    int parent = -1;
    std::vector<int> children;
    int cover = -1;  // a catalogue cover stands in for a picture or a video frame
    int seconds = 0; // running time of music and video

    // Measured by Tree::measure after every change.
    std::uint64_t total = 0; // bytes of the entry and everything below it
    int items = 0;           // entries below a folder

    // What a folder remembers of the last visit.
    int cursor = 0;
    float scroll = 0.0f;

    // The entry as a row of its parent's list.
    std::string label; // the name, cut to the name column
    bool selected = false;
    tween::Spring slot; // position in the list, in rows: sorting retargets it
    float delay = 0.0f; // seconds before the slot starts to move (staggers)
    tween::Bounce check;
    ui::Pulse flash; // lights a row that has just arrived
};

// Entries live in one vector and are named by index. Nothing is ever erased:
// a deleted entry is only detached, so an index stays valid while an animation
// still shows the row.
class Tree
{
  public:
    int add(int parent, std::string name, Kind kind, std::uint64_t size, int date, int time)
    {
        Node node;
        node.name = std::move(name);
        node.kind = kind;
        node.size = size;
        node.date = date;
        node.time = time;
        node.parent = parent;
        const int id = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(node));
        if (parent >= 0)
            at(parent).children.push_back(id);
        return id;
    }

    Node &at(int id)
    {
        return nodes_[static_cast<std::size_t>(id)];
    }
    const Node &at(int id) const
    {
        return nodes_[static_cast<std::size_t>(id)];
    }

    int top_of(int id) const
    {
        while (at(id).parent >= 0)
            id = at(id).parent;
        return id;
    }

    // True when id is `ancestor` or lies below it.
    bool inside(int id, int ancestor) const
    {
        for (; id >= 0; id = at(id).parent)
        {
            if (id == ancestor)
                return true;
        }
        return false;
    }

    int depth(int id) const
    {
        int depth = 0;
        for (; at(id).parent >= 0; id = at(id).parent)
            ++depth;
        return depth;
    }

    void detach(int id)
    {
        Node &node = at(id);
        if (node.parent < 0)
            return;
        std::vector<int> &siblings = at(node.parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        node.parent = -1;
    }

    void attach(int id, int folder)
    {
        at(id).parent = folder;
        at(folder).children.push_back(id);
    }

    // `name`, or "name (2)" and so on when the folder already holds one.
    std::string unique_name(int folder, const std::string &name, int ignore = -1) const
    {
        const auto taken = [&](const std::string &candidate)
        {
            for (int child : at(folder).children)
            {
                if (child != ignore && at(child).name == candidate)
                    return true;
            }
            return false;
        };
        if (!taken(name))
            return name;
        std::string stem = name;
        std::string extension;
        const std::size_t dot = name.rfind('.');
        if (dot != std::string::npos && dot > 0)
        {
            stem = name.substr(0, dot);
            extension = name.substr(dot);
        }
        for (int n = 2;; ++n)
        {
            const std::string candidate = stem + " (" + std::to_string(n) + ")" + extension;
            if (!taken(candidate))
                return candidate;
        }
    }

    // A deep copy of id inside `folder`. add() may move the vector, so
    // nothing is held by reference across it.
    int clone(int id, int folder)
    {
        const std::string name = unique_name(folder, at(id).name);
        const int copy = add(folder, name, at(id).kind, at(id).size, at(id).date, at(id).time);
        at(copy).cover = at(id).cover;
        at(copy).seconds = at(id).seconds;
        const std::vector<int> children = at(id).children;
        for (int child : children)
            clone(child, copy);
        return copy;
    }

    void files_below(int id, std::vector<int> *out) const
    {
        if (at(id).kind != Kind::folder)
        {
            out->push_back(id);
            return;
        }
        for (int child : at(id).children)
            files_below(child, out);
    }

    // Fills total and items for id and everything below it; adds the bytes
    // per kind to by_kind when it is given.
    void measure(int id, std::uint64_t *by_kind = nullptr)
    {
        Node &node = at(id);
        if (node.kind != Kind::folder)
        {
            node.total = node.size;
            node.items = 0;
            if (by_kind != nullptr)
                by_kind[static_cast<int>(node.kind)] += node.size;
            return;
        }
        std::uint64_t total = 0;
        int items = 0;
        for (std::size_t i = 0; i < at(id).children.size(); ++i)
        {
            const int child = at(id).children[i];
            measure(child, by_kind);
            total += at(child).total;
            items += 1 + at(child).items;
        }
        at(id).total = total;
        at(id).items = items;
    }

    // Bytes per kind below id, without changing anything.
    void usage(int id, std::uint64_t *by_kind) const
    {
        const Node &node = at(id);
        if (node.kind != Kind::folder)
        {
            by_kind[static_cast<int>(node.kind)] += node.size;
            return;
        }
        for (int child : node.children)
            usage(child, by_kind);
    }

  private:
    std::vector<Node> nodes_;
};

// A storage root: a drive with a capacity and the space the system keeps.
struct Volume
{
    int root = 0;
    std::uint64_t capacity = 0;
    std::uint64_t system = 0;
    std::uint64_t by_kind[kKinds] = {};
    std::uint64_t used = 0;
    bool removable = false;

    std::uint64_t free_space() const
    {
        return capacity > used ? capacity - used : 0;
    }
};

enum class Sort : std::uint8_t
{
    name,
    size,
    date,
    kind,
};
constexpr int kSorts = 4;
constexpr const char *kSortHints[kSorts] = {"Sort: Name", "Sort: Size", "Sort: Date", "Sort: Kind"};
constexpr const char *kSortWords[kSorts] = {"by name", "largest first", "newest first", "by kind"};

enum class Zone : std::uint8_t
{
    rail,
    list,
};

// What has the input. Everything but `none` is drawn on glass over the screen.
enum class Layer : std::uint8_t
{
    none,
    menu,
    picker,
    progress,
    confirm,
    rename,
    details,
};
constexpr int kLayers = 7;

enum MenuItem : int
{
    kOpen,
    kCopy,
    kMove,
    kRename,
    kDelete,
    kDetails,
    kMenuItems,
};
constexpr const char *kMenuLabels[kMenuItems] = {"Open",   "Copy",   "Move",
                                                 "Rename", "Delete", "Details"};

struct PickRow
{
    int node = 0;
    int depth = 0;
    const char *reason = nullptr; // why it cannot be the destination; null when it can
};

struct Suggestion
{
    const char *label;
    std::string name;
};

// A row that is gone from the tree but still collapsing on screen.
struct Ghost
{
    std::string label;
    Kind kind = Kind::text;
    float y = 0.0f;   // list coordinates
    float age = 0.0f; // negative while it waits its turn
};

struct Job
{
    bool active = false;
    bool move = false;
    std::vector<int> sources;
    std::vector<int> files; // every file below the sources, in order
    std::uint64_t total = 0;
    int destination = 0;
    float elapsed = 0.0f;
    float duration = 2.0f;
    std::string title;
    std::string where;
};

// ---- small helpers --------------------------------------------------------

// Three significant digits from a megabyte up: "812 KB", "48.2 MB", "1.84 GB".
void format_size(std::uint64_t bytes, char *out, std::size_t capacity)
{
    static constexpr const char *kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 999.5 && unit < 4)
    {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        std::snprintf(out, capacity, "%d B", static_cast<int>(bytes));
    else if (unit == 1)
        std::snprintf(out, capacity, "%.0f KB", value);
    else if (value < 9.995)
        std::snprintf(out, capacity, "%.2f %s", value, kUnits[unit]);
    else if (value < 99.95)
        std::snprintf(out, capacity, "%.1f %s", value, kUnits[unit]);
    else
        std::snprintf(out, capacity, "%.0f %s", value, kUnits[unit]);
}

void format_date(int date, char *out, std::size_t capacity)
{
    std::snprintf(out, capacity, "%04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

void format_length(int seconds, char *out, std::size_t capacity)
{
    std::snprintf(out, capacity, "%d:%02d", seconds / 60, seconds % 60);
}

// "50,541,363 bytes"
void format_bytes(std::uint64_t bytes, char *out, std::size_t capacity)
{
    char digits[32];
    const int count =
        std::snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(bytes));
    std::string grouped;
    for (int i = 0; i < count; ++i)
    {
        if (i > 0 && (count - i) % 3 == 0)
            grouped += ',';
        grouped += digits[i];
    }
    std::snprintf(out, capacity, "%s bytes", grouped.c_str());
}

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-blind comparison, so "Harbour" sorts between "changelog" and "notes".
int compare_names(const std::string &a, const std::string &b)
{
    const std::size_t count = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const char x = lower(a[i]);
        const char y = lower(b[i]);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

// ---- icons, assembled from kit shapes -------------------------------------
//
// One family: a wash of the kind's tint inside a stroke of the same tint, with
// one telling detail. Everything is a fraction of the size s, so the 30 px
// icon of a row and the 120 px one of the preview are the same drawing.

void draw_icon(gfx::DrawList &list, Kind kind, float cx, float cy, float s)
{
    const Color tint = tint_of(kind);
    const Color wash = tint.with_alpha(0.16f);
    const float h = s * 0.5f;
    const float stroke = std::max(1.5f, s * 0.065f);
    const Rect page{cx - s * 0.36f, cy - h, s * 0.72f, s};
    switch (kind)
    {
    case Kind::folder:
    {
        // A tab behind a front flap: the only solid icon, so folders stand
        // out from files at a glance.
        const Color back = gfx::mix(tint, kGraphite, 0.38f);
        list.rounded_rect({cx - h, cy - s * 0.42f, s * 0.46f, s * 0.3f}, s * 0.09f, back);
        list.rounded_rect({cx - h, cy - s * 0.3f, s, s * 0.72f}, s * 0.1f, back);
        list.gradient_rect({cx - h, cy - s * 0.19f, s, s * 0.61f}, s * 0.1f, tint,
                           gfx::mix(tint, kGraphite, 0.2f));
        break;
    }
    case Kind::image:
    {
        const Rect frame{cx - h, cy - s * 0.4f, s, s * 0.8f};
        list.bordered_rect(frame, s * 0.1f, wash, stroke, tint);
        list.circle(cx + s * 0.2f, cy - s * 0.15f, s * 0.085f, tint);
        const float base = frame.y + frame.h - stroke - s * 0.05f;
        list.triangle({cx - s * 0.36f, base - s * 0.32f, s * 0.46f, s * 0.32f}, tint);
        list.triangle({cx - s * 0.02f, base - s * 0.2f, s * 0.36f, s * 0.2f},
                      tint.with_alpha(0.65f));
        break;
    }
    case Kind::music:
    {
        // A beamed pair: two heads, two stems, one slanted beam.
        const float r = s * 0.13f;
        const float x1 = cx - s * 0.2f + r - stroke * 0.5f;
        const float x2 = cx + s * 0.24f + r - stroke * 0.5f;
        list.circle(cx - s * 0.2f, cy + s * 0.27f, r, tint);
        list.circle(cx + s * 0.24f, cy + s * 0.17f, r, tint);
        list.line(x1, cy + s * 0.27f, x1, cy - s * 0.28f, stroke, tint);
        list.line(x2, cy + s * 0.17f, x2, cy - s * 0.38f, stroke, tint);
        list.line(x1, cy - s * 0.28f, x2, cy - s * 0.38f, stroke * 1.9f, tint);
        break;
    }
    case Kind::video:
    {
        // A strip of film: sprocket holes along both edges, a play mark.
        const Rect frame{cx - h, cy - s * 0.36f, s, s * 0.72f};
        list.bordered_rect(frame, s * 0.1f, wash, stroke, tint);
        for (int i = 0; i < 4; ++i)
        {
            const float x = frame.x + s * (0.15f + 0.2f * static_cast<float>(i));
            const float hole = s * 0.07f;
            list.rounded_rect({x, frame.y + stroke + s * 0.04f, s * 0.1f, hole}, 1, tint);
            list.rounded_rect({x, frame.y + frame.h - stroke - s * 0.04f - hole, s * 0.1f, hole}, 1,
                              tint);
        }
        list.triangle({cx - s * 0.1f, cy - s * 0.12f, s * 0.24f, s * 0.24f}, tint, 0.0f, 1.5708f);
        break;
    }
    case Kind::archive:
    {
        // A page with a zip: alternating teeth and a pull.
        list.bordered_rect(page, s * 0.1f, wash, stroke, tint);
        for (int i = 0; i < 5; ++i)
        {
            const float x = cx - s * 0.08f + static_cast<float>(i % 2) * s * 0.08f;
            list.rounded_rect({x, page.y + stroke + s * 0.05f + static_cast<float>(i) * s * 0.1f,
                               s * 0.08f, s * 0.08f},
                              1, tint);
        }
        list.bordered_rect({cx - s * 0.1f, cy + s * 0.16f, s * 0.2f, s * 0.2f}, s * 0.05f, kClear,
                           stroke * 0.8f, tint);
        break;
    }
    case Kind::save:
    {
        // A memory card: a shutter at the top, a label below.
        const Rect card{cx - s * 0.4f, cy - s * 0.44f, s * 0.8f, s * 0.88f};
        list.bordered_rect(card, s * 0.1f, wash, stroke, tint);
        list.rounded_rect({cx - s * 0.2f, card.y + stroke, s * 0.4f, s * 0.24f}, s * 0.03f, tint);
        list.bordered_rect({cx - s * 0.24f, cy + s * 0.04f, s * 0.48f, s * 0.26f}, s * 0.04f,
                           kClear, stroke * 0.8f, tint);
        break;
    }
    case Kind::text:
    {
        list.bordered_rect(page, s * 0.1f, wash, stroke, tint);
        for (int i = 0; i < 4; ++i)
        {
            const float y = cy - s * 0.24f + static_cast<float>(i) * s * 0.16f;
            list.line(cx - s * 0.18f, y, cx + (i == 3 ? s * 0.02f : s * 0.18f), y, stroke * 0.8f,
                      tint);
        }
        break;
    }
    case Kind::app:
    {
        // A rounded square with a launcher grid.
        list.gradient_rect({cx - s * 0.42f, cy - s * 0.42f, s * 0.84f, s * 0.84f}, s * 0.24f, tint,
                           gfx::mix(tint, kGraphite, 0.32f));
        for (int i = 0; i < 4; ++i)
        {
            list.circle(cx + (i % 2 == 0 ? -1.0f : 1.0f) * s * 0.14f,
                        cy + (i < 2 ? -1.0f : 1.0f) * s * 0.14f, s * 0.075f,
                        kGraphite.with_alpha(0.82f));
        }
        break;
    }
    }
}

// A drive: a box with an activity light; a removable one shows its plug.
void draw_drive(gfx::DrawList &list, bool removable, float cx, float cy, float s, Color tint)
{
    const float stroke = std::max(1.5f, s * 0.065f);
    if (removable)
    {
        list.rounded_rect({cx - s * 0.5f, cy - s * 0.13f, s * 0.24f, s * 0.26f}, s * 0.04f, tint);
        list.bordered_rect({cx - s * 0.3f, cy - s * 0.24f, s * 0.8f, s * 0.48f}, s * 0.1f,
                           tint.with_alpha(0.16f), stroke, tint);
        list.circle(cx + s * 0.3f, cy, s * 0.06f, tint);
        return;
    }
    list.bordered_rect({cx - s * 0.5f, cy - s * 0.32f, s, s * 0.64f}, s * 0.12f,
                       tint.with_alpha(0.16f), stroke, tint);
    list.line(cx - s * 0.3f, cy + s * 0.1f, cx + s * 0.06f, cy + s * 0.1f, stroke * 0.8f,
              tint.with_alpha(0.6f));
    list.circle(cx + s * 0.3f, cy + s * 0.1f, s * 0.06f, tint);
}

// A filled disc with a check drawn as two strokes.
void draw_check(gfx::DrawList &list, float cx, float cy, float r, Color disc, Color ink)
{
    if (r < 0.5f)
        return;
    list.circle(cx, cy, r, disc);
    const float t = r * 0.24f;
    list.line(cx - r * 0.45f, cy + r * 0.04f, cx - r * 0.12f, cy + r * 0.38f, t, ink);
    list.line(cx - r * 0.12f, cy + r * 0.38f, cx + r * 0.46f, cy - r * 0.3f, t, ink);
}

void draw_chevron(gfx::DrawList &list, float cx, float cy, float s, Color color)
{
    list.line(cx - s * 0.3f, cy - s * 0.5f, cx + s * 0.3f, cy, 2.0f, color);
    list.line(cx + s * 0.3f, cy, cx - s * 0.3f, cy + s * 0.5f, 2.0f, color);
}

// One line of text cut to max_width. The string is only copied when it has
// to be shortened, so the usual case costs no allocation per frame.
float fit_text(gfx::DrawList &list, const ui::FontRef &font, std::string_view value, float x,
               float baseline, float size, Color color, float max_width,
               gfx::Align align = gfx::Align::left)
{
    if (font.measure(value, size) <= max_width)
        return ui::text(list, font, value, x, baseline, size, color, align);
    return ui::text(list, font, font.font->fit(value, size, max_width), x, baseline, size, color,
                    align);
}

// Frosted panel: the blurred screen, a graphite tint, a hairline of light.
void glass_panel(gfx::DrawList &list, std::uint32_t glass, const Rect &r, float radius)
{
    list.shadow({r.x, r.y + 18, r.w, r.h}, radius, 46, Color::rgb(0x000000, 0.55f));
    list.glass(glass, r, radius, kWhite);
    list.rounded_rect(r, radius, kPanel.with_alpha(0.72f));
    list.bordered_rect(r, radius, kClear, 1.5f, kWhite.with_alpha(0.14f));
}

// ---- the design -----------------------------------------------------------

class Files final : public app::Concept
{
  public:
    explicit Files(app::Context &context) : context_(context)
    {
        build_tree();
        measure();
        folder_ = volumes_[0].root;
        path_ = old_path_ = path_to(folder_);
        rebuild_rows(-1, true);
        shown_ = previous_ = preview_target();
        rail_y_.snap(place_rect(0).y);
        snap_gauge();
        arrow_x_.snap(arrow_x(sort_));
        zone_fx_.snap(1.0f);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "files",
            "File Browser",
            "A file manager: aligned columns, folders that keep your place, visible results",
            "src/concepts/files.cpp",
            audio::SoundSet::paper,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        layer_ = Layer::none;
        job_.active = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        switch (layer_)
        {
        case Layer::none:
            if (zone_ == Zone::rail)
                update_rail(input, feedback);
            else
                update_list(input, feedback);
            break;
        case Layer::menu:
            update_menu(input, feedback);
            break;
        case Layer::picker:
            update_picker(input, feedback);
            break;
        case Layer::progress:
            update_progress(input, feedback);
            break;
        case Layer::confirm:
            update_confirm(input, feedback);
            break;
        case Layer::rename:
            update_rename(input, feedback);
            break;
        case Layer::details:
            update_details(input, feedback);
            break;
        }
        advance(dt, feedback);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = Color::rgb(0x191d24);
        frame.backdrop.colors[1] = Color::rgb(0x0e1014);
        frame.backdrop.colors[2] = Color::rgb(0x21405f);
        frame.backdrop.params[0] = 0.78f;
        frame.backdrop.params[1] = 0.0f;
        frame.backdrop.params[2] = 0.45f;
        frame.backdrop.time = context_.settings.reduced_motion ? 0.0f : clock_;

        // A dialog pushes the screen back: it shrinks a little and dims. The
        // menu is lighter than a dialog, so it only dims by half as much.
        gfx::DrawList &list = frame.scene;
        const float back = dim_.value;
        list.push_transform(1.0f - 0.02f * back, 960, 540, 0, 0);
        draw_top(list);
        draw_rail(list);
        draw_list(list);
        draw_preview(list);
        draw_selection_bar(list);
        list.pop_transform();
        if (back > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x06080b, 0.5f * back));

        gfx::DrawList &overlay = frame.overlay;
        bool glass = false;
        for (int i = 1; i < kLayers; ++i)
            glass = glass || fx_[i].value > 0.01f;
        if (glass)
        {
            frame.glass = true;
            if (fx_[static_cast<int>(Layer::menu)].value > 0.01f)
                draw_menu(overlay, frame.glass_texture);
            if (fx_[static_cast<int>(Layer::picker)].value > 0.01f)
                draw_picker(overlay, frame.glass_texture);
            if (fx_[static_cast<int>(Layer::progress)].value > 0.01f)
                draw_progress(overlay, frame.glass_texture);
            if (fx_[static_cast<int>(Layer::confirm)].value > 0.01f)
                draw_confirm(overlay, frame.glass_texture);
            if (fx_[static_cast<int>(Layer::rename)].value > 0.01f)
                draw_rename(overlay, frame.glass_texture);
            if (fx_[static_cast<int>(Layer::details)].value > 0.01f)
                draw_details(overlay, frame.glass_texture);
        }
        draw_toast(overlay);
        draw_hints(layer_ == Layer::none ? frame.scene : frame.overlay);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- the tree ----

    Node &node(int id)
    {
        return tree_.at(id);
    }
    const Node &node(int id) const
    {
        return tree_.at(id);
    }

    void build_tree()
    {
        const auto dir = [this](int parent, const char *name, int date)
        { return tree_.add(parent, name, Kind::folder, 0, date, 1200); };
        const auto file = [this](int parent, const std::string &name, Kind kind, std::uint64_t size,
                                 int date, int time, int cover = -1, int seconds = 0)
        {
            const int id = tree_.add(parent, name, kind, size, date, time);
            tree_.at(id).cover = cover;
            tree_.at(id).seconds = seconds;
            return id;
        };
        const auto title = [this](int index)
        { return std::string(context_.catalog[static_cast<std::size_t>(index)].title); };
        char text[96];

        const int internal = dir(-1, "Internal", 20261001);
        volumes_.push_back({internal, 64 * kGB, 11 * kGB + 600 * kMB, {}, 0, false});

        const int apps = dir(internal, "Apps", 20260912);
        file(apps, "Ember Chess.app", Kind::app, 184 * kMB, 20260811, 932);
        file(apps, "Lantern Player.app", Kind::app, 96 * kMB + 400 * kKB, 20260702, 1810);
        file(apps, "Pocket Atlas.app", Kind::app, 412 * kMB, 20260530, 2204);
        file(apps, "Quill Notes.app", Kind::app, 28 * kMB + 100 * kKB, 20260912, 741);
        file(apps, "Tide Transfer.app", Kind::app, 12 * kMB + 800 * kKB, 20260419, 1356);

        const int captures = dir(internal, "Captures", 20260930);
        const int older = dir(captures, "2025", 20251228);
        file(older, "Saltwind 2025-11-02 2014.png", Kind::image, 3400 * kKB, 20251102, 2014, 9);
        file(older, "Kiln 2025-12-24 1820.png", Kind::image, 5100 * kKB, 20251224, 1820, 19);
        file(older, "Farlight 2025-12-28 2305.mp4", Kind::video, 268 * kMB, 20251228, 2305, 18, 31);
        // The long folder: a year of captures named the way a console names
        // them, title first. Every fifth one is a clip.
        const int year = dir(captures, "2026", 20260930);
        for (int i = 0; i < 31; ++i)
        {
            const int cover = (i * 5 + 2) % 24;
            const int month = 1 + i * 9 / 31;
            const int day = 1 + i * 11 % 28;
            const int clock = (18 + i % 5) * 100 + i * 17 % 60;
            const bool clip = i % 5 == 3;
            std::snprintf(text, sizeof(text), " 2026-%02d-%02d %04d.%s", month, day, clock,
                          clip ? "mp4" : "png");
            const std::uint64_t size =
                clip ? static_cast<std::uint64_t>(180 + i * 97 % 760) * kMB
                     : static_cast<std::uint64_t>(2100 + i * 613 % 4700) * kKB;
            file(year, title(cover) + text, clip ? Kind::video : Kind::image, size,
                 20260000 + month * 100 + day, clock, cover, clip ? 20 + i * 13 % 70 : 0);
        }
        const int favourites = dir(captures, "Favourites", 20260921);
        file(favourites, "Blue Hour - pier.png", Kind::image, 6800 * kKB, 20260921, 2112, 21);
        file(favourites, "Polar Bloom - first light.png", Kind::image, 4900 * kKB, 20260803, 644,
             10);

        const int downloads = dir(internal, "Downloads", 20260927);
        dir(downloads, "Incoming", 20260927);
        file(downloads, "controller-map.txt", Kind::text, 3 * kKB, 20260611, 1502);
        file(downloads, "firmware-notes.txt", Kind::text, 14 * kKB, 20260927, 1018);
        file(downloads, "theme-pack.zip", Kind::archive, 86 * kMB + 400 * kKB, 20260825, 2231);
        file(downloads, "wallpapers.zip", Kind::archive, 231 * kMB, 20260718, 1947);

        const int music = dir(internal, "Music", 20260819);
        const int loops = dir(music, "Loading Loops", 20260322);
        file(loops, "Loop A - warm pad.ogg", Kind::music, 2400 * kKB, 20260322, 1120, -1, 64);
        file(loops, "Loop B - soft keys.ogg", Kind::music, 3100 * kKB, 20260322, 1124, -1, 82);
        const int drives = dir(music, "Night Drives", 20260819);
        file(drives, "01 Overpass.flac", Kind::music, 31 * kMB + 200 * kKB, 20260819, 2301, -1,
             221);
        file(drives, "02 Sodium Lights.flac", Kind::music, 28 * kMB + 700 * kKB, 20260819, 2301, -1,
             204);
        file(drives, "03 Last Exit.flac", Kind::music, 36 * kMB + 900 * kKB, 20260819, 2302, -1,
             262);
        file(drives, "04 Tunnel Hum.flac", Kind::music, 24 * kMB + 300 * kKB, 20260819, 2302, -1,
             173);
        file(music, "Menu Theme.ogg", Kind::music, 6200 * kKB, 20260214, 909, -1, 148);

        const int saves = dir(internal, "Saves", 20261001);
        const int backups = dir(saves, "Backups", 20260930);
        file(backups, "saves-2026-08.zip", Kind::archive, 96 * kMB + 200 * kKB, 20260831, 400);
        file(backups, "saves-2026-09.zip", Kind::archive, 118 * kMB, 20260930, 400);
        file(saves, title(7) + ".sav", Kind::save, 42 * kMB + 600 * kKB, 20261001, 2147);
        file(saves, title(5) + ".sav", Kind::save, 7300 * kKB, 20260917, 1932);
        file(saves, title(14) + ".sav", Kind::save, 11 * kMB + 900 * kKB, 20260926, 2208);
        file(saves, title(1) + ".sav", Kind::save, 18 * kMB + 400 * kKB, 20260929, 1845);

        file(internal, "changelog.txt", Kind::text, 18 * kKB, 20260930, 1614);
        file(internal, "notes.txt", Kind::text, 2 * kKB, 20261001, 918);
        file(internal, "system-report.zip", Kind::archive, 44 * kMB, 20260714, 327);

        const int usb = dir(-1, "USB drive", 20260928);
        volumes_.push_back({usb, 32 * kGB, 200 * kMB, {}, 0, true});
        const int usb_backups = dir(usb, "Backups", 20260915);
        file(usb_backups, "console-2026-08.zip", Kind::archive, 1900 * kMB, 20260815, 310);
        file(usb_backups, "console-2026-09.zip", Kind::archive, 2100 * kMB, 20260915, 310);
        const int photos = dir(usb, "Photos", 20260822);
        file(photos, "Harbour wall.png", Kind::image, 7200 * kKB, 20260822, 1731, 3);
        file(photos, "Orchard gate.png", Kind::image, 5600 * kKB, 20260706, 1102, 2);
        file(photos, "Signal tower.png", Kind::image, 6100 * kKB, 20260511, 2040, 22);
        dir(usb, "Transfer", 20260928);
        const int videos = dir(usb, "Videos", 20260905);
        file(videos, "Harbour at dawn.mp4", Kind::video, kGB + 512 * kMB, 20260905, 612, 9, 754);
        file(videos, "Night market.mp4", Kind::video, 2 * kGB + 256 * kMB, 20260830, 2218, 3, 1131);
        file(videos, "Rooftop timelapse.mp4", Kind::video, 768 * kMB, 20260712, 1954, 12, 386);
        file(videos, "Tram ride.mp4", Kind::video, 3 * kGB, 20260601, 1640, 6, 1508);
        file(usb, "README.txt", Kind::text, 1 * kKB, 20260928, 1203);

        // The rail: the drives, then a few folders pinned for quick access.
        places_ = {internal, usb, captures, music, saves, downloads};
    }

    // Recomputes folder sizes and the space each drive uses, and drops pins
    // whose folder is gone.
    void measure()
    {
        for (Volume &volume : volumes_)
        {
            std::fill(std::begin(volume.by_kind), std::end(volume.by_kind), std::uint64_t{0});
            tree_.measure(volume.root, volume.by_kind);
            volume.used = volume.system + node(volume.root).total;
        }
        const int drives = static_cast<int>(volumes_.size());
        for (int i = static_cast<int>(places_.size()) - 1; i >= drives; --i)
        {
            if (volume_of(places_[static_cast<std::size_t>(i)]) < 0)
                places_.erase(places_.begin() + i);
        }
        place_ = std::min(place_, static_cast<int>(places_.size()) - 1);
    }

    // The drive an entry is on, or -1 when it has been deleted.
    int volume_of(int id) const
    {
        const int top = tree_.top_of(id);
        for (std::size_t i = 0; i < volumes_.size(); ++i)
        {
            if (volumes_[i].root == top)
                return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<int> path_to(int id) const
    {
        std::vector<int> path;
        for (; id >= 0; id = node(id).parent)
            path.insert(path.begin(), id);
        return path;
    }

    // "Internal / Captures" for the folder that holds id.
    std::string location_of(int id) const
    {
        std::string text;
        for (int step : path_to(id))
        {
            if (step == id && node(id).parent >= 0)
                break;
            if (!text.empty())
                text += " / ";
            text += node(step).name;
        }
        return text;
    }

    // A fuller drive sounds higher: completion and deletion cues take this.
    float level_pitch(int volume) const
    {
        const Volume &v = volumes_[static_cast<std::size_t>(volume)];
        return 0.85f + 0.4f * static_cast<float>(static_cast<double>(v.used) /
                                                 static_cast<double>(v.capacity));
    }

    // ---- the list ----

    // Folders first, always; then the chosen order; names settle ties.
    bool ordered(int a, int b) const
    {
        const Node &x = node(a);
        const Node &y = node(b);
        if ((x.kind == Kind::folder) != (y.kind == Kind::folder))
            return x.kind == Kind::folder;
        switch (sort_)
        {
        case Sort::size:
            if (x.total != y.total)
                return x.total > y.total;
            break;
        case Sort::date:
        {
            const std::int64_t p = static_cast<std::int64_t>(x.date) * 10000 + x.time;
            const std::int64_t q = static_cast<std::int64_t>(y.date) * 10000 + y.time;
            if (p != q)
                return p > q;
            break;
        }
        case Sort::kind:
            if (x.kind != y.kind)
                return x.kind < y.kind;
            break;
        case Sort::name:
            break;
        }
        const int names = compare_names(x.name, y.name);
        return names != 0 ? names < 0 : a < b;
    }

    int index_of(int id) const
    {
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            if (rows_[i] == id)
                return static_cast<int>(i);
        }
        return -1;
    }

    // Rebuilds the rows of the current folder in sort order. Rows that were
    // already listed keep their slot spring and travel to the new index; new
    // ones appear in place with a flash. `snap` places everything at once (a
    // different folder). The focus follows `keep`, or stays at its index.
    void rebuild_rows(int keep, bool snap)
    {
        const std::vector<int> before = rows_;
        rows_ = node(folder_).children;
        std::sort(rows_.begin(), rows_.end(), [this](int a, int b) { return ordered(a, b); });
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            Node &row = node(rows_[i]);
            const bool known =
                !snap && std::find(before.begin(), before.end(), rows_[i]) != before.end();
            if (!known)
            {
                row.slot.snap(static_cast<float>(i));
                row.delay = 0.0f;
                row.check.snap(row.selected ? 1.0f : 0.0f);
                if (!snap)
                    row.flash.trigger();
            }
            row.label = context_.fonts.regular.font->fit(row.name, kNameSize, kNameW);
        }
        const int count = static_cast<int>(rows_.size());
        int index = keep >= 0 ? index_of(keep) : -1;
        if (index < 0)
            index = focus_;
        focus_ = count > 0 ? std::clamp(index, 0, count - 1) : 0;
    }

    std::vector<int> selection() const
    {
        std::vector<int> picked;
        for (int id : rows_)
        {
            if (node(id).selected)
                picked.push_back(id);
        }
        return picked;
    }

    // What an action works on: the selection, or else the focused entry.
    std::vector<int> targets() const
    {
        std::vector<int> picked = selection();
        if (picked.empty() && !rows_.empty())
            picked.push_back(rows_[static_cast<std::size_t>(focus_)]);
        return picked;
    }

    std::uint64_t bytes_of(const std::vector<int> &ids) const
    {
        std::uint64_t bytes = 0;
        for (int id : ids)
            bytes += node(id).total;
        return bytes;
    }

    // Rows lower in the list tick lower.
    static float focus_pitch(int index)
    {
        return std::max(0.74f, 1.12f - 0.01f * static_cast<float>(index));
    }

    // Moves the focus and lets the highlight glide: the highlight is drawn at
    // the focused row plus an offset that springs back to zero, so it also
    // rides along when the row itself travels.
    void set_focus(int index)
    {
        if (rows_.empty())
            return;
        const float before = node(rows_[static_cast<std::size_t>(focus_)]).slot.value;
        focus_ = index;
        const float after = node(rows_[static_cast<std::size_t>(focus_)]).slot.value;
        focus_offset_.value += (before - after) * kRowH;
    }

    void step_focus(int delta, bool repeat, audio::Cue cue, app::Feedback &feedback)
    {
        const int count = static_cast<int>(rows_.size());
        const int next = count > 0 ? std::clamp(focus_ + delta, 0, count - 1) : focus_;
        if (count == 0 || next == focus_)
        {
            if (!repeat)
                refuse(feedback, 0.0f, delta > 0 ? 1.0f : -1.0f);
            return;
        }
        set_focus(next);
        feedback.play(cue, focus_pitch(next), ui::pan_for_x(kListX + kListW * 0.5f));
    }

    // The soft "no" of an edge: quiet error, light rumble, a nudge that way.
    void refuse(app::Feedback &feedback, float dx, float dy)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_x_ = dx;
        nudge_y_ = dy;
    }

    // The "not this one" of a dimmed choice.
    void deny(app::Feedback &feedback)
    {
        feedback.play(audio::Cue::invalid, 1.0f, 0.0f, 0.7f);
        feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
        nudge_x_ = 1.0f;
        nudge_y_ = 0.0f;
    }

    // Shows another folder. direction: 1 went deeper (the old list leaves to
    // the left), -1 came back up, 0 jumped. focus_child puts the focus on that
    // entry; otherwise the folder's own remembered focus is restored.
    void show_folder(int id, int direction, int focus_child)
    {
        if (id == folder_)
            return;
        const bool calm = context_.settings.reduced_motion;
        Node &from = node(folder_);
        from.cursor = focus_;
        from.scroll = scroll_.position.target;
        // A selection belongs to the folder it was made in.
        for (int row : rows_)
            node(row).selected = false;
        out_rows_ = rows_;
        out_focus_ = focus_;
        out_scroll_ = scroll_.offset();
        old_path_ = path_;

        folder_ = id;
        path_ = path_to(id);
        rows_.clear();
        ghosts_.clear();
        focus_ = node(id).cursor;
        rebuild_rows(focus_child, true);
        scroll_.position.snap(node(id).scroll);
        reveal_focus();
        scroll_.position.snap(scroll_.position.target);
        focus_offset_.snap(0.0f);
        slide_direction_ = direction;
        slide_.start(calm ? 0.14f : 0.34f);
        crumb_.start(calm ? 0.14f : 0.42f);
    }

    void reveal_focus()
    {
        const float count = static_cast<float>(rows_.size());
        if (rows_.empty())
        {
            scroll_.position.target = 0.0f;
            return;
        }
        const float start = static_cast<float>(focus_) * kRowH;
        scroll_.reveal(start, start + kRowH, kListH, kRowH, count * kRowH);
    }

    void update_list(const InputFrame &input, app::Feedback &feedback)
    {
        const bool empty = rows_.empty();
        switch (input.nav)
        {
        case Direction::up:
        case Direction::down:
            step_focus(input.nav == Direction::down ? 1 : -1, input.nav_repeat, audio::Cue::focus,
                       feedback);
            break;
        case Direction::left:
            zone_ = Zone::rail;
            place_ = current_place();
            feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(kRailX + kRailW * 0.5f));
            break;
        case Direction::right:
            if (!input.nav_repeat)
                refuse(feedback, 1.0f, 0.0f);
            break;
        case Direction::none:
            break;
        }

        if (input.is_pressed(Action::confirm))
        {
            if (empty)
                refuse(feedback, 1.0f, 0.0f);
            else
                open_entry(rows_[static_cast<std::size_t>(focus_)], feedback, audio::Cue::open);
        }
        else if (input.is_pressed(Action::back))
        {
            if (!selection().empty())
            {
                // Back leaves the innermost thing first: the selection.
                for (int row : rows_)
                    node(row).selected = false;
                feedback.play(audio::Cue::undo);
            }
            else
            {
                go_up(feedback);
            }
        }
        else if (input.is_pressed(Action::menu))
        {
            if (empty)
                refuse(feedback, 1.0f, 0.0f);
            else
                open_menu(feedback);
        }
        else if (input.is_pressed(Action::west))
        {
            if (empty)
                refuse(feedback, 1.0f, 0.0f);
            else
                toggle_selected(feedback);
        }
        else if (input.is_pressed(Action::north))
        {
            cycle_sort(feedback);
        }
        else if (input.is_pressed(Action::jump_prev))
        {
            step_focus(-(kRows - 1), false, audio::Cue::slide, feedback);
        }
        else if (input.is_pressed(Action::jump_next))
        {
            step_focus(kRows - 1, false, audio::Cue::slide, feedback);
        }
    }

    // A folder is entered; a file shows its details.
    void open_entry(int id, app::Feedback &feedback, audio::Cue file_cue)
    {
        if (node(id).kind == Kind::folder)
        {
            layer_ = Layer::none;
            show_folder(id, 1, -1);
            // Deeper sounds lower, like stairs going down.
            feedback.play(audio::Cue::open,
                          1.06f - 0.04f * static_cast<float>(tree_.depth(folder_)));
            return;
        }
        details_ = id;
        layer_ = Layer::details;
        feedback.play(file_cue == audio::Cue::open ? audio::Cue::modal_open : file_cue);
    }

    void go_up(app::Feedback &feedback)
    {
        const int parent = node(folder_).parent;
        if (parent < 0)
        {
            refuse(feedback, -1.0f, 0.0f);
            return;
        }
        const int child = folder_;
        show_folder(parent, -1, child);
        feedback.play(audio::Cue::back, 1.06f - 0.04f * static_cast<float>(tree_.depth(folder_)));
    }

    void toggle_selected(app::Feedback &feedback)
    {
        Node &row = node(rows_[static_cast<std::size_t>(focus_)]);
        row.selected = !row.selected;
        if (row.selected && !context_.settings.reduced_motion)
            row.check.kick(9.0f);
        // The mark climbs with the number selected, like a streak.
        const int count = static_cast<int>(selection().size());
        feedback.play(audio::Cue::mark, 1.0f + 0.04f * static_cast<float>(std::min(count, 10)),
                      ui::pan_for_x(kListX + kIconX));
    }

    void cycle_sort(app::Feedback &feedback)
    {
        sort_ = static_cast<Sort>((static_cast<int>(sort_) + 1) % kSorts);
        const int keep = rows_.empty() ? -1 : rows_[static_cast<std::size_t>(focus_)];
        rebuild_rows(keep, false);
        // The focus stays on its entry, which travels: no highlight jump.
        focus_offset_.snap(0.0f);
        feedback.play(audio::Cue::tab, 1.0f + 0.05f * static_cast<float>(sort_),
                      ui::pan_for_x(arrow_x(sort_)));
    }

    // ---- the rail ----

    // The place the current folder belongs to: the deepest pin above it.
    int current_place() const
    {
        int best = 0;
        int best_depth = -1;
        for (std::size_t i = 0; i < places_.size(); ++i)
        {
            const int depth = tree_.depth(places_[i]);
            if (tree_.inside(folder_, places_[i]) && depth > best_depth)
            {
                best = static_cast<int>(i);
                best_depth = depth;
            }
        }
        return best;
    }

    Rect place_rect(int index) const
    {
        float y = kListTop + static_cast<float>(index) * kRowH;
        if (index >= static_cast<int>(volumes_.size()))
            y += kRailGap;
        return {kRailX, y, kRailW, kRowH};
    }

    void update_rail(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(places_.size());
        switch (input.nav)
        {
        case Direction::up:
        case Direction::down:
        {
            const int next = place_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < count)
            {
                place_ = next;
                feedback.play(audio::Cue::focus, focus_pitch(next),
                              ui::pan_for_x(kRailX + kRailW * 0.5f));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, input.nav == Direction::down ? 1.0f : -1.0f);
            }
            break;
        }
        case Direction::right:
            zone_ = Zone::list;
            feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(kListX + kListW * 0.5f));
            break;
        case Direction::left:
            if (!input.nav_repeat)
                refuse(feedback, -1.0f, 0.0f);
            break;
        case Direction::none:
            break;
        }

        if (input.is_pressed(Action::confirm))
        {
            show_folder(places_[static_cast<std::size_t>(place_)], 0, -1);
            zone_ = Zone::list;
            feedback.play(audio::Cue::open,
                          1.06f - 0.04f * static_cast<float>(tree_.depth(folder_)),
                          ui::pan_for_x(kRailX + kRailW * 0.5f));
        }
        else if (input.is_pressed(Action::back))
        {
            zone_ = Zone::list;
            feedback.play(audio::Cue::back);
        }
        else if (input.is_pressed(Action::north))
        {
            cycle_sort(feedback);
        }
        else if (input.is_pressed(Action::west) || input.is_pressed(Action::menu))
        {
            // Places are not entries: nothing to select or act on here.
            deny(feedback);
        }
    }

    // ---- the context menu ----

    Rect menu_rect() const
    {
        const float h = kMenuPad * 2 + kMenuHead + kMenuItemH * static_cast<float>(kMenuItems);
        // The first item lines up with the row; near the bottom the popover
        // slides up instead of leaving the screen.
        const float y =
            std::clamp(menu_cy_ - kMenuPad - kMenuHead - kMenuItemH * 0.5f, 72.0f, 968.0f - h);
        // It hangs over the size and date columns, so the name it acts on
        // stays readable beside it.
        return {kListX + kListW - kMenuW - 8.0f, y, kMenuW, h};
    }

    // Open, Rename and Details work on one entry at a time.
    bool menu_enabled(int item) const
    {
        return menu_single_ || (item != kOpen && item != kRename && item != kDetails);
    }

    void open_menu(app::Feedback &feedback)
    {
        const std::vector<int> items = targets();
        menu_single_ = items.size() == 1;
        if (menu_single_)
        {
            menu_title_ = node(items[0]).name;
        }
        else
        {
            char text[48];
            std::snprintf(text, sizeof(text), "%d items", static_cast<int>(items.size()));
            menu_title_ = text;
        }
        menu_item_ = menu_single_ ? kOpen : kCopy;
        menu_pos_.snap(static_cast<float>(menu_item_));
        menu_cy_ = kListTop + (static_cast<float>(focus_) + 0.5f) * kRowH - scroll_.position.target;
        layer_ = Layer::menu;
        feedback.play(audio::Cue::modal_open, 1.0f, ui::pan_for_x(menu_rect().cx()));
    }

    void update_menu(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = menu_item_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kMenuItems)
            {
                menu_item_ = next;
                feedback.play(audio::Cue::focus, 1.06f - 0.02f * static_cast<float>(next),
                              ui::pan_for_x(menu_rect().cx()));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, input.nav == Direction::down ? 1.0f : -1.0f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            if (!menu_enabled(menu_item_))
            {
                deny(feedback);
                return;
            }
            const std::vector<int> items = targets();
            if (items.empty())
            {
                layer_ = Layer::none;
                return;
            }
            switch (menu_item_)
            {
            case kOpen:
                layer_ = Layer::none;
                open_entry(items[0], feedback, audio::Cue::select);
                break;
            case kCopy:
            case kMove:
                open_picker(menu_item_ == kMove);
                feedback.play(audio::Cue::select);
                break;
            case kRename:
                open_rename(items[0]);
                feedback.play(audio::Cue::select);
                break;
            case kDelete:
                open_confirm(items);
                feedback.play(audio::Cue::select);
                break;
            default:
                details_ = items[0];
                layer_ = Layer::details;
                feedback.play(audio::Cue::select);
                break;
            }
        }
        else if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
        {
            layer_ = Layer::none;
            feedback.play(audio::Cue::modal_close);
        }
    }

    // ---- copy and move: pick a folder, then watch it happen ----

    void add_pick_rows(int id, int depth, const std::vector<int> &sources, std::uint64_t bytes)
    {
        PickRow row;
        row.node = id;
        row.depth = depth;
        const int from = volume_of(sources[0]);
        const int to = volume_of(id);
        for (int source : sources)
        {
            if (tree_.inside(id, source))
                row.reason = "A folder cannot go inside itself";
        }
        if (row.reason == nullptr && pick_move_ && id == folder_)
            row.reason = "It is already here";
        // A move on one drive needs no room; everything else does.
        if (row.reason == nullptr && !(pick_move_ && from == to) &&
            bytes > volumes_[static_cast<std::size_t>(to)].free_space())
            row.reason = "Not enough free space on this drive";
        pick_rows_.push_back(row);

        std::vector<int> folders;
        for (int child : node(id).children)
        {
            if (node(child).kind == Kind::folder)
                folders.push_back(child);
        }
        std::sort(folders.begin(), folders.end(),
                  [this](int a, int b)
                  {
                      const int names = compare_names(node(a).name, node(b).name);
                      return names != 0 ? names < 0 : a < b;
                  });
        for (int child : folders)
            add_pick_rows(child, depth + 1, sources, bytes);
    }

    void open_picker(bool move)
    {
        const std::vector<int> sources = targets();
        const std::uint64_t bytes = bytes_of(sources);
        pick_move_ = move;
        pick_rows_.clear();
        for (const Volume &volume : volumes_)
            add_pick_rows(volume.root, 0, sources, bytes);

        char size[24];
        char text[160];
        format_size(bytes, size, sizeof(size));
        if (sources.size() == 1)
            std::snprintf(text, sizeof(text), "%s", node(sources[0]).name.c_str());
        else
            std::snprintf(text, sizeof(text), "%d items", static_cast<int>(sources.size()));
        pick_what_ = text;
        pick_size_ = size;
        pick_bytes_ = bytes;
        pick_from_ = volume_of(folder_);

        // Start on the last destination when it is still a good one, or on
        // the first folder that can take the entries.
        pick_focus_ = -1;
        const int count = static_cast<int>(pick_rows_.size());
        for (int i = 0; i < count; ++i)
        {
            const PickRow &row = pick_rows_[static_cast<std::size_t>(i)];
            if (row.reason == nullptr && (pick_focus_ < 0 || row.node == last_destination_))
                pick_focus_ = i;
        }
        pick_focus_ = std::max(pick_focus_, 0);
        pick_pos_.snap(static_cast<float>(pick_focus_));
        pick_scroll_.position.snap(0.0f);
        reveal_pick();
        pick_scroll_.position.snap(pick_scroll_.position.target);
        layer_ = Layer::picker;
    }

    void reveal_pick()
    {
        const float start = static_cast<float>(pick_focus_) * kPickRowH;
        pick_scroll_.reveal(start, start + kPickRowH, kPickRowH * static_cast<float>(kPickRows),
                            kPickRowH, kPickRowH * static_cast<float>(pick_rows_.size()));
    }

    void update_picker(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(pick_rows_.size());
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = pick_focus_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < count)
            {
                pick_focus_ = next;
                feedback.play(audio::Cue::focus, focus_pitch(next));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, input.nav == Direction::down ? 1.0f : -1.0f);
            }
        }
        // The triggers jump from drive to drive.
        const int jump = (input.is_pressed(Action::jump_next) ? 1 : 0) -
                         (input.is_pressed(Action::jump_prev) ? 1 : 0);
        if (jump != 0)
        {
            int next = pick_focus_ + jump;
            while (next >= 0 && next < count &&
                   pick_rows_[static_cast<std::size_t>(next)].depth > 0)
                next += jump;
            if (next >= 0 && next < count)
            {
                pick_focus_ = next;
                feedback.play(audio::Cue::slide, focus_pitch(next));
            }
            else
            {
                refuse(feedback, 0.0f, static_cast<float>(jump));
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            const PickRow &row = pick_rows_[static_cast<std::size_t>(pick_focus_)];
            if (row.reason != nullptr)
            {
                deny(feedback);
                return;
            }
            start_job(row.node);
            feedback.play(audio::Cue::select);
        }
        else if (input.is_pressed(Action::back))
        {
            layer_ = Layer::none;
            feedback.play(audio::Cue::modal_close);
        }
    }

    void start_job(int destination)
    {
        job_ = {};
        job_.active = true;
        job_.move = pick_move_;
        job_.sources = targets();
        for (int source : job_.sources)
            tree_.files_below(source, &job_.files);
        job_.total = bytes_of(job_.sources);
        job_.destination = destination;
        // About two seconds, a little longer for several files. A move within
        // one drive only renames, so it is quick.
        const bool same_drive = volume_of(job_.sources[0]) == volume_of(destination);
        const float files = static_cast<float>(job_.files.size());
        job_.duration = job_.move && same_drive
                            ? 0.9f
                            : std::min(4.0f, 2.0f + 0.3f * std::max(0.0f, files - 1));
        job_.title = std::string(job_.move ? "Moving " : "Copying ") + pick_what_;
        job_.where = "to " + location_of(destination);
        if (node(destination).parent >= 0)
            job_.where += " / " + node(destination).name;
        layer_ = Layer::progress;
    }

    void update_progress(const InputFrame &input, app::Feedback &feedback)
    {
        if (!input.is_pressed(Action::confirm) && !input.is_pressed(Action::back))
            return;
        // Nothing has been written yet: the tree only changes on completion,
        // so cancelling leaves no half-copied folder behind.
        job_.active = false;
        layer_ = Layer::none;
        feedback.play(audio::Cue::back);
        show_toast(job_.move ? "Move cancelled. Nothing was changed."
                             : "Copy cancelled. Nothing was changed.",
                   kWarn);
    }

    void finish_job(app::Feedback &feedback)
    {
        job_.active = false;
        const int focused = rows_.empty() ? -1 : rows_[static_cast<std::size_t>(focus_)];
        int first = -1;
        int count = 0;
        for (int source : job_.sources)
        {
            // The source may have been removed from under a queued job.
            if (volume_of(source) < 0 || volume_of(job_.destination) < 0)
                continue;
            ++count;
            node(source).selected = false;
            if (!job_.move)
            {
                tree_.clone(source, job_.destination);
                continue;
            }
            const int index = index_of(source);
            if (index >= 0)
            {
                first = first < 0 ? index : std::min(first, index);
                add_ghost(source);
            }
            const std::string name = tree_.unique_name(job_.destination, node(source).name);
            tree_.detach(source);
            node(source).name = name;
            tree_.attach(source, job_.destination);
        }
        last_destination_ = job_.destination;
        after_change(focused, first);
        if (layer_ == Layer::progress)
            layer_ = Layer::none;

        char text[200];
        const std::string where =
            context_.fonts.regular.font->fit(node(job_.destination).name, 24, 260.0f);
        if (count == 1)
            std::snprintf(text, sizeof(text), "%s to %s", job_.move ? "Moved" : "Copied",
                          where.c_str());
        else
            std::snprintf(text, sizeof(text), "%s %d items to %s", job_.move ? "Moved" : "Copied",
                          count, where.c_str());
        show_toast(text, kAccent);
        feedback.play(audio::Cue::notify, level_pitch(volume_of(job_.destination)));
    }

    // ---- delete ----

    void open_confirm(const std::vector<int> &items)
    {
        char size[24];
        char text[200];
        format_size(bytes_of(items), size, sizeof(size));
        if (items.size() == 1)
        {
            const Node &entry = node(items[0]);
            confirm_title_ =
                entry.kind == Kind::folder ? "Delete this folder?" : "Delete this file?";
            confirm_name_ = entry.name;
        }
        else
        {
            confirm_title_ = "Delete " + std::to_string(items.size()) + " items?";
            confirm_name_ = node(items[0]).name + " and " + std::to_string(items.size() - 1) +
                            (items.size() == 2 ? " other" : " others");
        }
        std::snprintf(
            text, sizeof(text), "Frees %s on %s. This cannot be undone.", size,
            node(volumes_[static_cast<std::size_t>(volume_of(folder_))].root).name.c_str());
        confirm_body_ = text;
        // A destructive question starts on the safe answer.
        confirm_choice_ = 0;
        confirm_pos_.snap(0.0f);
        layer_ = Layer::confirm;
    }

    void update_confirm(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = input.nav == Direction::right ? 1 : 0;
            if (next != confirm_choice_)
            {
                confirm_choice_ = next;
                feedback.play(audio::Cue::focus, 1.0f, next == 1 ? 0.12f : -0.12f);
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, next == 1 ? 1.0f : -1.0f, 0.0f);
            }
        }
        if (input.is_pressed(Action::confirm) && confirm_choice_ == 1)
        {
            layer_ = Layer::none;
            delete_targets(feedback);
        }
        else if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            layer_ = Layer::none;
            feedback.play(audio::Cue::modal_close);
        }
    }

    void add_ghost(int id)
    {
        Ghost ghost;
        ghost.label = node(id).label;
        ghost.kind = node(id).kind;
        ghost.y = node(id).slot.value * kRowH;
        ghost.age = -0.035f * static_cast<float>(std::min<std::size_t>(ghosts_.size(), 8));
        ghosts_.push_back(std::move(ghost));
    }

    void delete_targets(app::Feedback &feedback)
    {
        const std::vector<int> doomed = targets();
        if (doomed.empty())
            return;
        const int volume = volume_of(folder_);
        const std::uint64_t freed = bytes_of(doomed);
        const int focused = rows_[static_cast<std::size_t>(focus_)];
        int first = static_cast<int>(rows_.size());
        for (int id : doomed)
        {
            first = std::min(first, index_of(id));
            add_ghost(id);
            node(id).selected = false;
            tree_.detach(id);
        }
        after_change(focused, first);

        char size[24];
        char text[120];
        format_size(freed, size, sizeof(size));
        if (doomed.size() == 1)
            std::snprintf(text, sizeof(text), "Deleted 1 item. %s freed.", size);
        else
            std::snprintf(text, sizeof(text), "Deleted %d items. %s freed.",
                          static_cast<int>(doomed.size()), size);
        show_toast(text, kDanger);
        // The drive is emptier now, and sounds it.
        feedback.play(audio::Cue::erase, level_pitch(volume));
    }

    // The tree changed under the list: measure again and let the rows find
    // their places. Rows from `first_gap` on close the gap one after another.
    void after_change(int keep, int first_gap)
    {
        measure();
        const bool kept = keep >= 0 && node(keep).parent == folder_;
        rebuild_rows(kept ? keep : -1, false);
        focus_offset_.snap(0.0f);
        if (first_gap < 0 || context_.settings.reduced_motion)
            return;
        for (std::size_t i = static_cast<std::size_t>(first_gap); i < rows_.size(); ++i)
        {
            const float order = static_cast<float>(i - static_cast<std::size_t>(first_gap));
            node(rows_[i]).delay = 0.1f + std::min(0.3f, 0.025f * order);
        }
    }

    // ---- rename: a field that retypes itself from a few suggestions ----

    void open_rename(int id)
    {
        const Node &entry = node(id);
        std::string stem = entry.name;
        std::string extension;
        const std::size_t dot = stem.rfind('.');
        if (entry.kind != Kind::folder && dot != std::string::npos && dot > 0)
        {
            extension = stem.substr(dot);
            stem.erase(dot);
        }
        suggestions_.clear();
        const auto offer = [this](const char *label, std::string name)
        {
            for (const Suggestion &known : suggestions_)
            {
                if (known.name == name)
                    return;
            }
            suggestions_.push_back({label, std::move(name)});
        };
        std::string tidy = stem;
        std::string plain;
        for (char &c : tidy)
        {
            if (c == '_')
                c = ' ';
        }
        if (!tidy.empty() && tidy[0] >= 'a' && tidy[0] <= 'z')
            tidy[0] = static_cast<char>(tidy[0] - 'a' + 'A');
        for (char c : stem)
            plain += c == ' ' ? '-' : lower(c);
        offer("Keep", entry.name);
        offer("Tidy", tidy + extension);
        offer("Dated", "2026-10-01 " + entry.name);
        offer("Plain", plain + extension);
        offer("Numbered", stem + " 01" + extension);
        offer("Archived", stem + " (old)" + extension);

        rename_ = id;
        rename_choice_ = 0;
        rename_pos_.snap(0.0f);
        typed_ = static_cast<float>(entry.name.size());
        layer_ = Layer::rename;
    }

    void update_rename(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = static_cast<int>(suggestions_.size());
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = rename_choice_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < count)
            {
                // The field keeps what the two names share and retypes the
                // rest, so the change reads as an edit, not a swap.
                const std::string &from =
                    suggestions_[static_cast<std::size_t>(rename_choice_)].name;
                const std::string &to = suggestions_[static_cast<std::size_t>(next)].name;
                std::size_t shared = 0;
                while (shared < from.size() && shared < to.size() && from[shared] == to[shared])
                    ++shared;
                typed_ = std::min(typed_, static_cast<float>(shared));
                rename_choice_ = next;
                feedback.play(audio::Cue::type, 1.06f - 0.02f * static_cast<float>(next));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback, 0.0f, input.nav == Direction::down ? 1.0f : -1.0f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            layer_ = Layer::none;
            const std::string &wanted = suggestions_[static_cast<std::size_t>(rename_choice_)].name;
            if (rename_choice_ == 0 || volume_of(rename_) < 0)
            {
                feedback.play(audio::Cue::modal_close);
                return;
            }
            const int parent = node(rename_).parent;
            node(rename_).name = tree_.unique_name(parent, wanted, rename_);
            if (parent == folder_)
            {
                // The row travels to where its new name sorts.
                const int focused = rows_.empty() ? -1 : rows_[static_cast<std::size_t>(focus_)];
                rebuild_rows(focused, false);
                focus_offset_.snap(0.0f);
                node(rename_).flash.trigger();
            }
            show_toast("Renamed to " +
                           context_.fonts.regular.font->fit(node(rename_).name, 24, 420.0f),
                       kAccent);
            feedback.play(audio::Cue::place);
        }
        else if (input.is_pressed(Action::back))
        {
            layer_ = Layer::none;
            feedback.play(audio::Cue::modal_close);
        }
    }

    void update_details(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            layer_ = Layer::none;
            feedback.play(audio::Cue::modal_close);
        }
    }

    void show_toast(std::string text, Color color)
    {
        toast_text_ = std::move(text);
        toast_color_ = color;
        toast_left_ = 3.2f;
    }

    // ---- animation state ----

    int preview_target() const
    {
        if (zone_ == Zone::rail)
            return places_[static_cast<std::size_t>(place_)];
        return rows_.empty() ? folder_ : rows_[static_cast<std::size_t>(focus_)];
    }

    // Gauge segments: the system's share, then one per file kind.
    float gauge_target(const Volume &volume, int segment) const
    {
        const std::uint64_t bytes = segment == 0 ? volume.system : volume.by_kind[segment];
        return static_cast<float>(static_cast<double>(bytes) /
                                  static_cast<double>(volume.capacity));
    }

    void snap_gauge()
    {
        const Volume &volume = volumes_[static_cast<std::size_t>(volume_of(folder_))];
        for (int i = 0; i < kKinds; ++i)
            gauge_[i].snap(gauge_target(volume, i));
        free_gb_.snap(static_cast<float>(static_cast<double>(volume.free_space()) /
                                         static_cast<double>(kGB)));
    }

    float arrow_x(Sort sort) const
    {
        const ui::FontRef &font = context_.fonts.semibold;
        switch (sort)
        {
        case Sort::name:
            return kListX + kNameX + font.measure("NAME", 15, 3.0f) + 14;
        case Sort::kind:
            return kListX + kKindX + font.measure("KIND", 15, 3.0f) + 14;
        case Sort::size:
            return kListX + kSizeX - font.measure("SIZE", 15, 3.0f) - 16;
        case Sort::date:
            return kListX + kDateX - font.measure("DATE", 15, 3.0f) - 16;
        }
        return kListX;
    }

    void advance(float dt, app::Feedback &feedback)
    {
        const bool calm = context_.settings.reduced_motion;
        if (job_.active)
        {
            job_.elapsed += dt;
            if (job_.elapsed >= job_.duration)
                finish_job(feedback);
        }

        // Rows: each one chases its index, after its own delay.
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            Node &row = node(rows_[i]);
            if (row.delay > 0.0f)
                row.delay -= dt;
            else
                row.slot.target = static_cast<float>(i);
            row.slot.update(dt, calm ? 40.0f : 15.0f);
            row.check.target = row.selected ? 1.0f : 0.0f;
            row.check.update(dt, 18.0f, calm ? 1.0f : 0.6f);
            row.flash.update(dt, 2.5f);
        }
        for (Ghost &ghost : ghosts_)
            ghost.age += dt;
        ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(),
                                     [](const Ghost &ghost) { return ghost.age >= kGhostTime; }),
                      ghosts_.end());
        focus_offset_.update(dt, calm ? 40.0f : 21.0f);
        reveal_focus();
        scroll_.update(dt, calm ? 40.0f : 15.0f);
        slide_.update(dt);
        crumb_.update(dt);
        nudge_.update(dt, 9.0f);

        rail_y_.target = place_rect(place_).y;
        rail_y_.update(dt, calm ? 40.0f : 21.0f);
        zone_fx_.target = zone_ == Zone::list ? 1.0f : 0.0f;
        zone_fx_.update(dt, 18.0f);
        arrow_x_.target = arrow_x(sort_);
        arrow_x_.update(dt, calm ? 40.0f : 16.0f);

        const int target = preview_target();
        if (target != shown_)
        {
            previous_ = shown_;
            shown_ = target;
            preview_.start(calm ? 0.12f : 0.28f);
        }
        preview_.update(dt);

        // Layers cross-fade: each has its own presence, so the menu can leave
        // while the dialog it opened arrives.
        for (int i = 1; i < kLayers; ++i)
        {
            fx_[i].target = static_cast<int>(layer_) == i ? 1.0f : 0.0f;
            fx_[i].update(dt, calm ? 40.0f : 16.0f);
        }
        dim_.target = layer_ == Layer::none ? 0.0f : (layer_ == Layer::menu ? 0.5f : 1.0f);
        dim_.update(dt, calm ? 40.0f : 13.0f);
        menu_pos_.target = static_cast<float>(menu_item_);
        menu_pos_.update(dt, 24.0f);
        pick_pos_.target = static_cast<float>(pick_focus_);
        pick_pos_.update(dt, 22.0f);
        if (!pick_rows_.empty())
            reveal_pick();
        pick_scroll_.update(dt, 15.0f);
        confirm_pos_.target = static_cast<float>(confirm_choice_);
        confirm_pos_.update(dt, 22.0f);
        rename_pos_.target = static_cast<float>(rename_choice_);
        rename_pos_.update(dt, 22.0f);
        if (!suggestions_.empty())
        {
            const float full = static_cast<float>(
                suggestions_[static_cast<std::size_t>(rename_choice_)].name.size());
            typed_ = calm ? full : std::min(full, typed_ + dt * 60.0f);
        }

        // The selection bar keeps the last numbers while it slides away.
        const std::vector<int> picked = selection();
        if (!picked.empty())
        {
            bar_count_ = static_cast<int>(picked.size());
            bar_bytes_ = bytes_of(picked);
        }
        bar_.target = picked.empty() ? 0.0f : 1.0f;
        bar_.update(dt, calm ? 40.0f : 14.0f);

        if (toast_left_ > 0.0f)
            toast_left_ -= dt;
        toast_.target = toast_left_ > 0.0f ? 1.0f : 0.0f;
        toast_.update(dt, calm ? 40.0f : 13.0f);

        // The gauge eases to the drive's real numbers, so a delete is seen
        // running down and a copy filling up.
        const Volume &volume = volumes_[static_cast<std::size_t>(volume_of(folder_))];
        for (int i = 0; i < kKinds; ++i)
        {
            gauge_[i].target = gauge_target(volume, i);
            gauge_[i].update(dt, 5.0f);
        }
        free_gb_.target =
            static_cast<float>(static_cast<double>(volume.free_space()) / static_cast<double>(kGB));
        free_gb_.update(dt, 5.0f);
    }

    // Entrance progress of the nth part of the screen.
    float in(int index) const
    {
        return tween::stagger(age_, index, 0.05f, 0.45f);
    }

    // ---- drawing: the screen ----

    void draw_top(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[96];
        char size[24];
        const float enter = in(0);
        list.push_opacity(enter);
        list.push_transform(1.0f, 0, 0, 0, -12.0f * (1.0f - enter));

        // The breadcrumb. Segments the old and the new path share stay put;
        // the rest leave or arrive along the direction of travel.
        const float t = crumb_.running ? crumb_.progress() : 1.0f;
        const float way = slide_direction_ < 0 ? -1.0f : 1.0f;
        std::size_t shared = 0;
        while (shared < path_.size() && shared < old_path_.size() &&
               path_[shared] == old_path_[shared])
            ++shared;
        const float nudge = nudge_x_ < 0.0f && layer_ == Layer::none
                                ? ui::shake(nudge_.value, clock_, 10.0f)
                                : 0.0f;
        const auto segment = [&](int id, bool first, float strength, float alpha, float x)
        {
            if (!first)
                draw_chevron(list, x + 20, 97, 14, kInk.with_alpha(0.35f * alpha));
            const float left = first ? x : x + 40;
            const float width = fit_text(list, fonts.semibold, node(id).name, left, 108, 30,
                                         kInk.with_alpha(alpha * strength), 360.0f);
            return left + width - x;
        };
        float x = kMargin + nudge;
        for (std::size_t i = 0; i < shared; ++i)
        {
            const float was = i + 1 == old_path_.size() ? 1.0f : 0.5f;
            const float is = i + 1 == path_.size() ? 1.0f : 0.5f;
            x += segment(path_[i], i == 0, tween::lerp(was, is, t), 1.0f, x);
        }
        if (t < 1.0f)
        {
            const float leave = tween::clamp01(t * 2.5f);
            float old_x = x - way * 26.0f * tween::cubic_in(leave);
            for (std::size_t i = shared; i < old_path_.size(); ++i)
                old_x += segment(old_path_[i], i == 0, i + 1 == old_path_.size() ? 1.0f : 0.5f,
                                 1.0f - tween::smoothstep(leave), old_x);
        }
        const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
        float new_x = x + way * 30.0f * (1.0f - tween::quint_out(arrive));
        for (std::size_t i = shared; i < path_.size(); ++i)
            new_x += segment(path_[i], i == 0, i + 1 == path_.size() ? 1.0f : 0.5f,
                             tween::smoothstep(arrive), new_x);

        // Under it, the folder in numbers.
        const Node &folder = node(folder_);
        format_size(folder.total, size, sizeof(size));
        std::snprintf(text, sizeof(text), "%d %s  \xC2\xB7  %s  \xC2\xB7  %s",
                      static_cast<int>(rows_.size()), rows_.size() == 1 ? "item" : "items", size,
                      kSortWords[static_cast<int>(sort_)]);
        ui::text(list, fonts.regular, text, kMargin, 144, 20, kInk.with_alpha(0.5f));
        list.pop_transform();
        list.pop_opacity();

        draw_gauge(list);
        list.rounded_rect({kMargin, 164, kRight - kMargin, 1}, 0, kWhite.with_alpha(0.09f * enter));
    }

    // The drive's space as one bar: the system's share, one segment per kind,
    // and what is left. The focused entry's kind lights its segment.
    void draw_gauge(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Volume &volume = volumes_[static_cast<std::size_t>(volume_of(folder_))];
        char text[64];
        const float enter = in(1);
        list.push_opacity(enter);
        ui::text(list, fonts.semibold, ui::upper(node(volume.root).name), kPreviewX, 98, 15,
                 kInk.with_alpha(0.55f), gfx::Align::left, 3.0f);
        std::snprintf(text, sizeof(text), "%.2f GB free of %d GB",
                      static_cast<double>(free_gb_.value), static_cast<int>(volume.capacity / kGB));
        ui::text(list, fonts.mono, text, kRight, 98, kMonoSize, kInk.with_alpha(0.9f),
                 gfx::Align::right);

        const Rect bar{kPreviewX, 116, kPreviewW, 10};
        list.rounded_rect(bar, 5, kWhite.with_alpha(0.08f));
        const Kind lit = node(shown_).kind;
        float x = bar.x;
        for (int i = 0; i < kKinds; ++i)
        {
            const float width = gauge_[i].value * bar.w * enter;
            if (width >= 1.0f)
            {
                const bool focused = i > 0 && static_cast<int>(lit) == i;
                const Color color = i == 0 ? kSystem : tint_of(static_cast<Kind>(i));
                const Rect part{x, bar.y, std::max(1.0f, width - 2.0f), bar.h};
                if (focused)
                    list.glow(part, 3, 10, color.with_alpha(0.45f));
                list.rounded_rect(part, 3, color.with_alpha(focused || i == 0 ? 1.0f : 0.72f));
            }
            x += width;
        }
        // The legend names the lit segment, so the colours explain themselves
        // as the focus moves.
        char size[24];
        if (lit != Kind::folder)
        {
            format_size(volume.by_kind[static_cast<int>(lit)], size, sizeof(size));
            std::snprintf(text, sizeof(text), "%s  %s", label_of(lit), size);
        }
        else
        {
            format_size(volume.used, size, sizeof(size));
            std::snprintf(text, sizeof(text), "In use  %s", size);
        }
        list.circle(kPreviewX + 6, 144, 5, lit != Kind::folder ? tint_of(lit) : kSystem);
        ui::text(list, fonts.regular, text, kPreviewX + 20, 151, 20, kInk.with_alpha(0.6f));
        list.pop_opacity();
    }

    void draw_rail(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const int drives = static_cast<int>(volumes_.size());
        const int count = static_cast<int>(places_.size());
        const float enter = in(2);
        list.push_opacity(enter);
        list.push_transform(1.0f, 0, 0, -18.0f * (1.0f - enter), 0);
        ui::text(list, fonts.semibold, "DRIVES", kRailX + 14, kHeaderY, 15, kInk.with_alpha(0.5f),
                 gfx::Align::left, 3.0f);
        if (count > drives)
            ui::text(list, fonts.semibold, "PINNED", kRailX + 14,
                     place_rect(drives).y - (kListTop - kHeaderY), 15, kInk.with_alpha(0.5f),
                     gfx::Align::left, 3.0f);

        // The highlight: bright while the rail has the input, a ghost of
        // itself while the list has it.
        const float active = 1.0f - zone_fx_.value;
        if (active > 0.01f)
        {
            Rect ring{kRailX, rail_y_.value, kRailW, kRowH};
            ring = ring.inset(3);
            if (zone_ == Zone::rail && layer_ == Layer::none)
            {
                ring.x += ui::shake(nudge_.value, clock_, 10.0f) * nudge_x_;
                ring.y += ui::shake(nudge_.value, clock_, 6.0f) * nudge_y_;
            }
            list.push_opacity(active);
            list.glow(ring, kRadius, 12, kAccent.with_alpha(0.16f + 0.08f * breath()));
            list.bordered_rect(ring, kRadius, kAccent.with_alpha(0.16f), 2, kAccent);
            list.pop_opacity();
        }

        const int here = current_place();
        for (int i = 0; i < count; ++i)
        {
            const Rect r = place_rect(i);
            const int id = places_[static_cast<std::size_t>(i)];
            const bool focused = zone_ == Zone::rail && i == place_;
            const bool current = i == here;
            const float cy = r.cy();
            // The place you are in keeps a quiet marker when the rail is idle.
            if (current)
                list.rounded_rect({r.x + 3, cy - 11, 3, 22}, 1.5f,
                                  kAccent.with_alpha(zone_fx_.value));
            const Color tint = current || focused ? kAccent : kInk.with_alpha(0.6f);
            if (i < drives)
                draw_drive(list, volumes_[static_cast<std::size_t>(i)].removable, r.x + 34, cy, 26,
                           tint);
            else
                draw_icon(list, Kind::folder, r.x + 34, cy, 26);
            fit_text(list, current || focused ? fonts.semibold : fonts.regular, node(id).name,
                     r.x + 62, cy + kNameSize * 0.35f, kNameSize,
                     kInk.with_alpha(current || focused ? 1.0f : 0.72f),
                     kRailW - 62 - (i < drives ? 80.0f : 16.0f));
            if (i < drives)
            {
                // A drive carries its fill level as a small meter.
                const Volume &volume = volumes_[static_cast<std::size_t>(i)];
                const float used = static_cast<float>(static_cast<double>(volume.used) /
                                                      static_cast<double>(volume.capacity));
                const Rect meter{r.x + r.w - 64, cy - 3, 46, 6};
                list.rounded_rect(meter, 3, kWhite.with_alpha(0.12f));
                list.rounded_rect({meter.x, meter.y, std::max(6.0f, meter.w * used * enter), 6}, 3,
                                  tint);
            }
        }
        list.pop_transform();
        list.pop_opacity();
        list.rounded_rect({kListX - 16, kListTop - 36, 1, kListH + 36}, 0,
                          kWhite.with_alpha(0.07f * enter));
    }

    float breath() const
    {
        return context_.settings.reduced_motion ? 0.5f : ui::breathe(clock_);
    }

    void draw_list(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float enter = in(2);

        // Column captions; the sorted one is lit and carries the arrow.
        list.push_opacity(enter);
        const auto caption = [&](const char *label, float x, gfx::Align align, Sort sort)
        {
            const bool sorted = sort_ == sort;
            ui::text(list, fonts.semibold, label, kListX + x, kHeaderY, 15,
                     sorted ? kAccent : kInk.with_alpha(0.5f), align, 3.0f);
        };
        caption("NAME", kNameX, gfx::Align::left, Sort::name);
        caption("KIND", kKindX, gfx::Align::left, Sort::kind);
        caption("SIZE", kSizeX, gfx::Align::right, Sort::size);
        caption("DATE", kDateX, gfx::Align::right, Sort::date);
        // Names and kinds ascend, sizes and dates start at the top: the arrow
        // points the way the column reads.
        const bool descending = sort_ == Sort::size || sort_ == Sort::date;
        list.triangle({arrow_x_.value - 5, kHeaderY - 13, 10, 10}, kAccent, 0.0f,
                      descending ? 3.1415927f : 0.0f);
        list.rounded_rect({kListX, kListTop - 1, kListW, 1}, 0, kWhite.with_alpha(0.09f));
        list.pop_opacity();

        // Rows are clipped to the list. During a folder change the old rows
        // leave one way while the new ones arrive from the other.
        list.push_clip({kListX - 10, kListTop, kListW + 20, kListH});
        if (slide_.running)
        {
            const bool calm = context_.settings.reduced_motion;
            const float t = slide_.progress();
            const float way = calm ? 0.0f : static_cast<float>(slide_direction_);
            const float leave = tween::clamp01(t * 1.8f);
            draw_rows(list, out_rows_, out_focus_, out_scroll_, 1.0f - tween::smoothstep(leave),
                      -way * 220.0f * tween::cubic_in(leave), false);
            const float arrive = tween::clamp01((t - 0.15f) / 0.85f);
            draw_rows(list, rows_, focus_, scroll_.offset(), tween::smoothstep(arrive),
                      way * 240.0f * (1.0f - tween::quint_out(arrive)), true);
        }
        else
        {
            draw_rows(list, rows_, focus_, scroll_.offset(), 1.0f, 0.0f, true);
        }
        list.pop_clip();

        // A thin thumb says where in a long folder you are.
        const float content = static_cast<float>(rows_.size()) * kRowH;
        if (content > kListH)
        {
            const Rect track{kListX + kListW + 6, kListTop + 4, 3, kListH - 8};
            const float size = std::max(40.0f, track.h * kListH / content);
            const float at = scroll_.offset() / (content - kListH);
            list.rounded_rect(track, 1.5f, kWhite.with_alpha(0.07f * enter));
            list.rounded_rect({track.x, track.y + (track.h - size) * tween::clamp01(at), 3, size},
                              1.5f, kInk.with_alpha(0.4f * enter));
        }
    }

    // One folder's rows at an opacity and a horizontal offset. `live` is the
    // current folder: it has the highlight, the entrance and the ghosts.
    void draw_rows(gfx::DrawList &list, const std::vector<int> &rows, int focus, float scroll,
                   float alpha, float dx, bool live) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        list.push_opacity(alpha);
        list.push_transform(1.0f, 0, 0, dx, 0);

        if (rows.empty())
        {
            // An empty folder is a state, not a blank: say so.
            const float cx = kListX + kListW * 0.5f;
            const float appear = live ? in(4) : 1.0f;
            list.push_opacity(appear);
            list.ring(cx, kListTop + 236, 84, 1.5f, kWhite.with_alpha(0.08f));
            list.push_opacity(0.55f);
            draw_icon(list, Kind::folder, cx, kListTop + 236, 76);
            list.pop_opacity();
            ui::text(list, fonts.semibold, "This folder is empty", cx, kListTop + 380, 28, kInk,
                     gfx::Align::center);
            ui::text(list, fonts.regular, "Copy or move something here to fill it.", cx,
                     kListTop + 420, 22, kInk.with_alpha(0.55f), gfx::Align::center);
            list.pop_opacity();
            list.pop_transform();
            list.pop_opacity();
            return;
        }

        // The highlight: one object under the rows. It sits on the focused
        // row's own animated slot, plus the glide offset of the last move.
        const Node &focused = node(rows[static_cast<std::size_t>(focus)]);
        Rect ring{kListX, kListTop + focused.slot.value * kRowH - scroll, kListW, kRowH};
        if (live)
        {
            ring.y += focus_offset_.value;
            if (zone_ == Zone::list && layer_ == Layer::none)
            {
                ring.x += ui::shake(nudge_.value, clock_, 10.0f) * nudge_x_;
                ring.y += ui::shake(nudge_.value, clock_, 6.0f) * nudge_y_;
            }
        }
        ring = ring.inset(3);
        const float active = live ? zone_fx_.value : 1.0f;
        list.shadow({ring.x, ring.y + 6, ring.w, ring.h}, kRadius, 18,
                    Color::rgb(0x000000, 0.35f * active));
        list.glow(ring, kRadius, 12, kAccent.with_alpha((0.14f + 0.08f * breath()) * active));
        list.bordered_rect(ring, kRadius,
                           gfx::mix(kWhite.with_alpha(0.05f), kAccent.with_alpha(0.17f), active), 2,
                           gfx::mix(kWhite.with_alpha(0.16f), kAccent, active));

        // The rows in view. They are drawn in three passes (shapes, the
        // reading face, the mono face) instead of row by row: a change of
        // font texture starts a new draw call, and this way a full list costs
        // three of them, not three per row.
        struct Visible
        {
            const Node *row;
            float y;
            float appear;
            bool focused;
        };
        Visible visible[kRows * 3];
        int count = 0;
        for (std::size_t i = 0; i < rows.size() && count < kRows * 3; ++i)
        {
            const Node &row = node(rows[i]);
            const float y = kListTop + row.slot.value * kRowH - scroll;
            if (y > kListTop + kListH || y + kRowH < kListTop)
                continue;
            // On entry the rows arrive top to bottom, a beat apart.
            const float order = (y - kListTop) / kRowH;
            float appear = live ? tween::cubic_out((age_ - 0.15f - 0.025f * order) / 0.35f) : 1.0f;
            // A row on its way to another slot thins out while it travels, so
            // rows that cross during a sort do not print over each other.
            appear *=
                1.0f - 0.6f * tween::clamp01(std::fabs(row.slot.value - row.slot.target) / 4.0f);
            if (appear > 0.01f)
                visible[count++] = {&row, y, appear, static_cast<int>(i) == focus};
        }
        const float settle = live ? 14.0f * (1.0f - tween::cubic_out((age_ - 0.15f) / 0.6f)) : 0.0f;
        const auto shift_of = [settle](const Visible &v)
        { return kCheckShift * tween::clamp01(v.row->check.value) + settle; };
        for (int i = 0; i < count; ++i)
        {
            const Visible &v = visible[i];
            const float check = tween::clamp01(v.row->check.value);
            const float cy = v.y + kRowH * 0.5f;
            const Rect body{kListX + 3, v.y + 3, kListW - 6, kRowH - 6};
            list.push_opacity(v.appear);
            if (check > 0.01f)
                list.rounded_rect(body, kRadius, kAccent.with_alpha(0.11f * check));
            if (v.row->flash.value > 0.01f)
                list.rounded_rect(body, kRadius, kAccent.with_alpha(0.3f * v.row->flash.value));
            if (!v.focused)
                list.rounded_rect({kListX + 14, v.y + kRowH - 0.5f, kListW - 28, 1}, 0,
                                  kWhite.with_alpha(0.045f));
            // The check pops in (its spring overshoots) while the row's
            // content steps aside for it.
            draw_check(list, kListX + 24, cy, 11.0f * std::max(0.0f, v.row->check.value), kAccent,
                       kGraphite);
            draw_icon(list, v.row->kind, kListX + kIconX + shift_of(v), cy, 30);
            list.pop_opacity();
        }
        for (int i = 0; i < count; ++i)
        {
            const Visible &v = visible[i];
            const float check = tween::clamp01(v.row->check.value);
            const float cy = v.y + kRowH * 0.5f;
            const float strength = (v.focused ? 1.0f : 0.84f) * v.appear;
            ui::text(list, fonts.regular, v.row->label, kListX + kNameX + shift_of(v),
                     cy + kNameSize * 0.35f, kNameSize,
                     gfx::mix(kInk, kAccent, 0.35f * check).with_alpha(strength));
            ui::text(list, fonts.regular, label_of(v.row->kind), kListX + kKindX, cy + 7, 20,
                     kInk.with_alpha(0.5f * strength));
        }
        for (int i = 0; i < count; ++i)
        {
            const Visible &v = visible[i];
            const float cy = v.y + kRowH * 0.5f;
            const float strength = (v.focused ? 1.0f : 0.84f) * v.appear;
            format_size(v.row->total, text, sizeof(text));
            ui::text(list, fonts.mono, text, kListX + kSizeX, cy + 7, kMonoSize,
                     kInk.with_alpha((v.row->kind == Kind::folder ? 0.5f : 0.86f) * strength),
                     gfx::Align::right);
            format_date(v.row->date, text, sizeof(text));
            ui::text(list, fonts.mono, text, kListX + kDateX, cy + 7, kMonoSize,
                     kInk.with_alpha(0.62f * strength), gfx::Align::right);
        }

        // Deleted rows: a red wash that folds shut where the row was.
        if (live)
        {
            for (const Ghost &ghost : ghosts_)
            {
                const float t = tween::clamp01(ghost.age / kGhostTime);
                const float fold = 1.0f - tween::cubic_in(t);
                const float y = kListTop + ghost.y - scroll;
                const float h = (kRowH - 6) * fold;
                const Rect r{kListX + 3, y + 3 + (kRowH - 6 - h) * 0.5f, kListW - 6, h};
                list.rounded_rect(r, std::min(kRadius, h * 0.5f), kDanger.with_alpha(0.22f * fold));
                list.push_clip(r);
                list.push_opacity(fold);
                draw_icon(list, ghost.kind, kListX + kIconX, y + kRowH * 0.5f, 30);
                ui::text(list, fonts.regular, ghost.label, kListX + kNameX,
                         y + kRowH * 0.5f + kNameSize * 0.35f, kNameSize, kInk.with_alpha(0.8f));
                list.pop_opacity();
                list.pop_clip();
            }
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // A picture or a clip shows a catalogue cover cut to 16:9; everything
    // else shows its icon, large, on a wash of its tint.
    void draw_art(gfx::DrawList &list, int id, const Rect &r) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Node &entry = node(id);
        const Color tint = tint_of(entry.kind);
        if (entry.cover >= 0 && (entry.kind == Kind::image || entry.kind == Kind::video))
        {
            // The covers are square canvases (bottom row first): take the
            // middle band whose shape matches the frame.
            const float band = std::min(1.0f, r.h / r.w);
            const Rect uv{0.0f, 0.5f + band * 0.5f, 1.0f, -band};
            list.image(context_.catalog[static_cast<std::size_t>(entry.cover)].cover, r, uv, kWhite,
                       14);
            if (entry.kind == Kind::video)
            {
                char text[16];
                list.circle(r.cx(), r.cy(), 34, Color::rgb(0x0b0d11, 0.62f));
                list.ring(r.cx(), r.cy(), 34, 2, kWhite.with_alpha(0.8f));
                list.triangle({r.cx() - 11, r.cy() - 14, 28, 28}, kWhite, 0.0f, 1.5708f);
                format_length(entry.seconds, text, sizeof(text));
                const float w = fonts.mono.measure(text, 18) + 20;
                list.rounded_rect({r.x + r.w - w - 12, r.y + r.h - 42, w, 30}, 8,
                                  Color::rgb(0x0b0d11, 0.72f));
                ui::text(list, fonts.mono, text, r.x + r.w - 22, r.y + r.h - 21, 18, kWhite,
                         gfx::Align::right);
            }
        }
        else
        {
            list.gradient_rect(r, 14, tint.with_alpha(0.12f), tint.with_alpha(0.03f));
            if (entry.parent < 0)
                draw_drive(list,
                           volumes_[static_cast<std::size_t>(std::max(0, volume_of(id)))].removable,
                           r.cx(), r.cy(), std::min(120.0f, r.h * 0.5f), kAccent);
            else
                draw_icon(list, entry.kind, r.cx(), r.cy(), std::min(120.0f, r.h * 0.5f));
        }
        list.bordered_rect(r, 14, kClear, 1.5f, kWhite.with_alpha(0.12f));
    }

    // The label/value lines under a preview. Returns the y after the last.
    float draw_facts(gfx::DrawList &list, int id, float x, float y, float w, bool full) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Node &entry = node(id);
        char text[64];
        const auto fact = [&](const char *label, const char *value, const ui::FontRef &font)
        {
            ui::text(list, fonts.regular, label, x, y + 30, 20, kInk.with_alpha(0.5f));
            fit_text(list, font, value, x + w, y + 30, kMonoSize, kInk.with_alpha(0.92f), w - 130,
                     gfx::Align::right);
            list.rounded_rect({x, y + 45.5f, w, 1}, 0, kWhite.with_alpha(0.07f));
            y += 46;
        };
        format_size(entry.total, text, sizeof(text));
        fact(entry.kind == Kind::folder ? "Total size" : "Size", text, fonts.mono);
        if (full && entry.kind != Kind::folder)
        {
            format_bytes(entry.total, text, sizeof(text));
            fact("Exactly", text, fonts.mono);
        }
        if (entry.kind == Kind::folder)
        {
            std::snprintf(text, sizeof(text), "%d %s", entry.items,
                          entry.items == 1 ? "item" : "items");
            fact("Contains", text, fonts.mono);
        }
        else if (entry.kind == Kind::image)
        {
            fact("Dimensions", "3840 x 2160", fonts.mono);
        }
        else if (entry.seconds > 0)
        {
            format_length(entry.seconds, text, sizeof(text));
            fact("Length", text, fonts.mono);
        }
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", entry.date / 10000,
                      entry.date / 100 % 100, entry.date % 100, entry.time / 100, entry.time % 100);
        fact("Modified", text, fonts.mono);
        if (volume_of(id) >= 0)
            fact(entry.parent < 0 ? "Drive" : "In", location_of(id).c_str(), fonts.regular);

        // A folder also shows what it is made of, in the gauge's colours.
        if (entry.kind == Kind::folder && entry.total > 0)
        {
            std::uint64_t by_kind[kKinds] = {};
            tree_.usage(id, by_kind);
            float at = x;
            for (int i = 1; i < kKinds; ++i)
            {
                const float part = static_cast<float>(static_cast<double>(by_kind[i]) /
                                                      static_cast<double>(entry.total)) *
                                   w;
                if (part >= 1.0f)
                    list.rounded_rect({at, y + 22, std::max(1.0f, part - 2.0f), 8}, 3,
                                      tint_of(static_cast<Kind>(i)));
                at += part;
            }
            y += 46;
        }
        return y;
    }

    // One entry's preview, drawn at an opacity and an offset so two of them
    // can cross-fade.
    void draw_preview_item(gfx::DrawList &list, int id, float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Node &entry = node(id);
        const float x = kPreviewX;
        list.push_opacity(alpha);
        draw_art(list, id, {x, kListTop - 36 + slide * 0.4f, kPreviewW, 261});
        list.push_transform(1.0f, 0, 0, 0, slide);
        ui::text(list, fonts.semibold, ui::upper(entry.parent < 0 ? "Drive" : label_of(entry.kind)),
                 x, 488, 15, entry.parent < 0 ? kAccent : tint_of(entry.kind), gfx::Align::left,
                 3.0f);
        const float after =
            ui::paragraph(list, fonts.semibold, entry.name, x, 528, 28, kPreviewW, 36, kInk, 2);
        draw_facts(list, id, x, after - 20, kPreviewW, false);
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_preview(gfx::DrawList &list) const
    {
        const float enter = in(4);
        list.rounded_rect({kPreviewX - 16, kListTop - 36, 1, kListH + 36}, 0,
                          kWhite.with_alpha(0.07f * enter));
        list.push_opacity(enter);
        if (preview_.running)
        {
            // The old entry leaves quickly; the new one arrives a beat later.
            const bool calm = context_.settings.reduced_motion;
            const float t = preview_.progress();
            draw_preview_item(list, previous_, 1.0f - tween::smoothstep(t * 2.4f), 0.0f);
            const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
            draw_preview_item(list, shown_, tween::smoothstep(arrive),
                              calm ? 0.0f : 14.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_preview_item(list, shown_, 1.0f, 24.0f * (1.0f - enter));
        }
        list.pop_opacity();
    }

    // "3 selected, 48.2 MB": slides up from the bottom while anything is
    // marked, and says which buttons act on the selection.
    void draw_selection_bar(gfx::DrawList &list) const
    {
        const float t = bar_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const Rect bar{kListX, kBarY + 90.0f * (1.0f - t), kListW, 60};
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.shadow({bar.x, bar.y + 10, bar.w, bar.h}, 16, 26, Color::rgb(0x000000, 0.5f));
        list.rounded_rect(bar, 16, Color::rgb(0x202a38));
        list.bordered_rect(bar, 16, kClear, 1.5f, kAccent.with_alpha(0.55f));
        draw_check(list, bar.x + 34, bar.cy(), 13, kAccent, kGraphite);
        std::snprintf(text, sizeof(text), "%d selected", bar_count_);
        float x = bar.x + 60;
        x += ui::text(list, fonts.semibold, text, x, bar.cy() + 9, 24, kInk);
        list.rounded_rect({x + 16, bar.cy() - 11, 1, 22}, 0, kWhite.with_alpha(0.2f));
        format_size(bar_bytes_, text, sizeof(text));
        ui::text(list, fonts.mono, text, x + 34, bar.cy() + 8, 22, kInk.with_alpha(0.9f));

        ui::HintLayout layout;
        layout.size = 32;
        layout.text_size = 22;
        layout.cy = bar.cy();
        layout.item_gap = 30;
        const ui::Hint hints[] = {{ui::Button::options, "Copy, move, delete"},
                                  {ui::Button::circle, "Clear"}};
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 2, bar.x + bar.w - 24, true,
                       layout);
        list.pop_opacity();
    }

    // ---- drawing: glass ----

    // A dialog arrives by growing the last few percent and rising a little.
    void push_layer(gfx::DrawList &list, Layer layer, const Rect &panel) const
    {
        const float t = fx_[static_cast<int>(layer)].value;
        const bool calm = context_.settings.reduced_motion;
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.push_transform(calm ? 1.0f : 0.95f + 0.05f * t, panel.cx(), panel.cy(), 0,
                            calm ? 0.0f : 16.0f * (1.0f - t));
    }

    static void pop_layer(gfx::DrawList &list)
    {
        list.pop_transform();
        list.pop_opacity();
    }

    // The nudge of a refusal, for whatever has the input in this layer.
    float layer_nudge(Layer layer, bool horizontal) const
    {
        if (layer_ != layer)
            return 0.0f;
        return ui::shake(nudge_.value, clock_, horizontal ? 10.0f : 6.0f) *
               (horizontal ? nudge_x_ : nudge_y_);
    }

    void draw_menu(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect panel = menu_rect();
        const float t = fx_[static_cast<int>(Layer::menu)].value;
        const bool calm = context_.settings.reduced_motion;
        // It grows out of its pointer: the row it belongs to.
        const float tip_y = std::clamp(menu_cy_, panel.y + 26, panel.y + panel.h - 26);
        list.push_opacity(tween::clamp01(t * 1.6f));
        list.push_transform(calm ? 1.0f : 0.9f + 0.1f * t, panel.x - 10, tip_y, 0, 0);
        glass_panel(list, glass, panel, 16);
        // The pointer: a small triangle on the edge, aimed at the row. It
        // keeps to the row while the panel slides to stay on screen. Its two
        // free edges continue the panel's hairline.
        list.triangle({panel.x - 16, tip_y - 11, 22, 22}, Color::rgb(0x1e232b), 0.0f, -1.5708f);
        list.rounded_rect({panel.x - 1, tip_y - 10, 3, 20}, 0, Color::rgb(0x1e232b));
        list.line(panel.x - 15, tip_y, panel.x, tip_y - 10.5f, 1.5f, kWhite.with_alpha(0.16f));
        list.line(panel.x - 15, tip_y, panel.x, tip_y + 10.5f, 1.5f, kWhite.with_alpha(0.16f));

        // The head says what the menu acts on; on a dimmed item it says why
        // that one is not available.
        const bool blocked = !menu_enabled(menu_item_);
        if (blocked)
            ui::text(list, fonts.regular, "One entry at a time", panel.x + 20,
                     panel.y + kMenuPad + 28, 20, kWarn);
        else
            fit_text(list, fonts.regular, menu_title_, panel.x + 20, panel.y + kMenuPad + 28, 20,
                     kInk.with_alpha(0.55f), kMenuW - 40);
        list.rounded_rect({panel.x + 12, panel.y + kMenuPad + kMenuHead - 6, panel.w - 24, 1}, 0,
                          kWhite.with_alpha(0.1f));

        const float top = panel.y + kMenuPad + kMenuHead;
        Rect ring{panel.x + 8, top + menu_pos_.value * kMenuItemH + 2, panel.w - 16,
                  kMenuItemH - 4};
        ring.x += layer_nudge(Layer::menu, true);
        ring.y += layer_nudge(Layer::menu, false);
        list.rounded_rect(ring, kRadius, blocked ? kWhite.with_alpha(0.1f) : kAccent);
        for (int i = 0; i < kMenuItems; ++i)
        {
            const float cy = top + (static_cast<float>(i) + 0.5f) * kMenuItemH;
            const bool focused = i == menu_item_;
            const bool enabled = menu_enabled(i);
            Color ink = i == kDelete ? kDanger : kInk;
            if (focused && enabled)
                ink = Color::rgb(0x0b1220);
            ui::text(list, focused ? fonts.semibold : fonts.regular, kMenuLabels[i], panel.x + 24,
                     cy + 24 * 0.35f, 24, ink.with_alpha(enabled ? 1.0f : 0.32f));
        }
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_picker(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float view = kPickRowH * static_cast<float>(kPickRows);
        const Rect panel{600, 218, 720, 644};
        char text[96];
        push_layer(list, Layer::picker, panel);
        glass_panel(list, glass, panel, kDialogRadius);
        ui::text(list, fonts.semibold, pick_move_ ? "MOVE TO" : "COPY TO", panel.x + 40,
                 panel.y + 54, 15, kAccent, gfx::Align::left, 3.0f);
        fit_text(list, fonts.semibold, pick_what_, panel.x + 40, panel.y + 96, 28, kInk, 480);
        ui::text(list, fonts.mono, pick_size_, panel.x + panel.w - 40, panel.y + 96, kMonoSize,
                 kInk.with_alpha(0.7f), gfx::Align::right);
        const float top = panel.y + 126;
        list.rounded_rect({panel.x + 24, top - 1, panel.w - 48, 1}, 0, kWhite.with_alpha(0.1f));
        list.rounded_rect({panel.x + 24, top + view, panel.w - 48, 1}, 0, kWhite.with_alpha(0.1f));

        // Every folder of every drive in one indented list: one press picks.
        list.push_clip({panel.x + 16, top, panel.w - 32, view});
        const float scroll = pick_scroll_.offset();
        Rect ring{panel.x + 24, top + pick_pos_.value * kPickRowH - scroll + 2, panel.w - 48,
                  kPickRowH - 4};
        ring.x += layer_nudge(Layer::picker, true);
        ring.y += layer_nudge(Layer::picker, false);
        const bool blocked = pick_rows_[static_cast<std::size_t>(pick_focus_)].reason != nullptr;
        list.bordered_rect(ring, kRadius, (blocked ? kWhite : kAccent).with_alpha(0.16f), 2,
                           blocked ? kWhite.with_alpha(0.35f) : kAccent);
        for (std::size_t i = 0; i < pick_rows_.size(); ++i)
        {
            const PickRow &row = pick_rows_[i];
            const float y = top + static_cast<float>(i) * kPickRowH - scroll;
            if (y > top + view || y + kPickRowH < top)
                continue;
            const Node &folder = node(row.node);
            const float cy = y + kPickRowH * 0.5f;
            const float x = panel.x + 44 + static_cast<float>(row.depth) * 30;
            list.push_opacity(row.reason == nullptr ? 1.0f : 0.36f);
            // Hairlines tie a folder to the one it sits in.
            for (int d = 1; d <= row.depth; ++d)
                list.rounded_rect({panel.x + 56 + static_cast<float>(d - 1) * 30, y, 1, kPickRowH},
                                  0, kWhite.with_alpha(0.1f));
            if (row.depth == 0)
            {
                const int volume = volume_of(row.node);
                const Volume &drive = volumes_[static_cast<std::size_t>(std::max(0, volume))];
                draw_drive(list, drive.removable, x + 12, cy, 24, kAccent);
                char size[24];
                format_size(drive.free_space(), size, sizeof(size));
                std::snprintf(text, sizeof(text), "%s free", size);
                ui::text(list, fonts.mono, text, panel.x + panel.w - 44, cy + 7, kMonoSize,
                         kInk.with_alpha(0.6f), gfx::Align::right);
            }
            else
            {
                draw_icon(list, Kind::folder, x + 12, cy, 24);
            }
            fit_text(list, row.depth == 0 ? fonts.semibold : fonts.regular, folder.name, x + 38,
                     cy + 24 * 0.35f, 24, kInk, 360);
            list.pop_opacity();
        }
        list.pop_clip();
        const float content = kPickRowH * static_cast<float>(pick_rows_.size());
        if (content > view)
        {
            const Rect track{panel.x + panel.w - 14, top + 4, 3, view - 8};
            const float size = std::max(40.0f, track.h * view / content);
            const float at = tween::clamp01(scroll / (content - view));
            list.rounded_rect(track, 1.5f, kWhite.with_alpha(0.07f));
            list.rounded_rect({track.x, track.y + (track.h - size) * at, 3, size}, 1.5f,
                              kInk.with_alpha(0.4f));
        }

        // The foot answers the question the focus raises: why not, or what
        // will be left.
        const PickRow &focused = pick_rows_[static_cast<std::size_t>(pick_focus_)];
        const float foot = top + view + 48;
        if (focused.reason != nullptr)
        {
            ui::text(list, fonts.regular, focused.reason, panel.x + 40, foot, 22, kWarn);
        }
        else
        {
            const int to = volume_of(focused.node);
            const Volume &drive = volumes_[static_cast<std::size_t>(std::max(0, to))];
            std::uint64_t left = drive.free_space();
            if (!(pick_move_ && pick_from_ == to))
                left -= std::min(left, pick_bytes_);
            char size[24];
            format_size(left, size, sizeof(size));
            ui::text(list, fonts.regular, "Free afterwards", panel.x + 40, foot, 22,
                     kInk.with_alpha(0.55f));
            ui::text(list, fonts.mono, size, panel.x + panel.w - 40, foot, kMonoSize,
                     kInk.with_alpha(0.9f), gfx::Align::right);
        }
        pop_layer(list);
    }

    void draw_progress(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect panel{540, 350, 840, 364};
        char text[96];
        char size[24];
        push_layer(list, Layer::progress, panel);
        glass_panel(list, glass, panel, kDialogRadius);
        const float x = panel.x + 44;
        const float w = panel.w - 88;
        const float progress = tween::clamp01(job_.elapsed / job_.duration);

        fit_text(list, fonts.semibold, job_.title, x, panel.y + 66, 30, kInk, w);
        fit_text(list, fonts.regular, job_.where, x, panel.y + 102, 22, kInk.with_alpha(0.55f), w);

        // The file being written is the one whose bytes the bar is in.
        const int count = static_cast<int>(job_.files.size());
        int current = 0;
        if (count > 0)
        {
            if (job_.total > 0)
            {
                const double done = static_cast<double>(job_.total) * static_cast<double>(progress);
                double sum = 0.0;
                for (current = 0; current < count - 1; ++current)
                {
                    sum += static_cast<double>(
                        node(job_.files[static_cast<std::size_t>(current)]).size);
                    if (sum > done)
                        break;
                }
            }
            else
            {
                current =
                    std::min(count - 1, static_cast<int>(progress * static_cast<float>(count)));
            }
            std::snprintf(text, sizeof(text), "%d of %d", current + 1, count);
            const float counter = ui::text(list, fonts.mono, text, x + w, panel.y + 162, kMonoSize,
                                           kInk.with_alpha(0.7f), gfx::Align::right);
            const Node &entry = node(job_.files[static_cast<std::size_t>(current)]);
            draw_icon(list, entry.kind, x + 14, panel.y + 154, 28);
            fit_text(list, fonts.regular, entry.name, x + 42, panel.y + 162, 24, kInk,
                     w - counter - 70);
        }

        // The bar shows measured progress; a sheen runs along the filled part
        // so it reads as work in flight, not as a static meter.
        const Rect bar{x, panel.y + 186, w, 12};
        list.rounded_rect(bar, 6, kWhite.with_alpha(0.1f));
        const Rect fill{bar.x, bar.y, std::max(12.0f, bar.w * progress), bar.h};
        list.glow(fill, 6, 10, kAccent.with_alpha(0.3f));
        list.gradient_rect_h(fill, 6, Color::rgb(0x3b86e8), kAccent);
        if (!context_.settings.reduced_motion && fill.w > 60.0f)
        {
            const float sheen = fill.x + std::fmod(clock_ * 420.0f, fill.w + 120.0f) - 60.0f;
            list.push_clip(fill);
            list.gradient_rect_h({sheen, fill.y, 60, fill.h}, 0, kWhite.with_alpha(0.0f),
                                 kWhite.with_alpha(0.4f));
            list.pop_clip();
        }

        // Rate and time left, in mono so the digits tick in place.
        const double rate = static_cast<double>(job_.total) / static_cast<double>(job_.duration) *
                            (1.0 + 0.06 * std::sin(static_cast<double>(job_.elapsed) * 5.0));
        format_size(static_cast<std::uint64_t>(rate), size, sizeof(size));
        std::snprintf(text, sizeof(text), "%s/s", size);
        ui::text(list, fonts.mono, text, x, panel.y + 234, kMonoSize, kInk.with_alpha(0.8f));
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(progress * 100.0f));
        ui::text(list, fonts.mono, text, bar.cx(), panel.y + 234, kMonoSize, kAccent,
                 gfx::Align::center);
        const int left = static_cast<int>(std::ceil(std::max(0.0f, job_.duration - job_.elapsed)));
        std::snprintf(text, sizeof(text), "0:%02d left", left);
        ui::text(list, fonts.mono, text, x + w, panel.y + 234, kMonoSize, kInk.with_alpha(0.8f),
                 gfx::Align::right);

        const Rect cancel{panel.cx() - 110, panel.y + 268, 220, 56};
        list.glow(cancel, kRadius + 4, 12, kAccent.with_alpha(0.14f + 0.08f * breath()));
        list.bordered_rect(cancel, kRadius + 4, kAccent.with_alpha(0.16f), 2, kAccent);
        ui::text(list, fonts.semibold, "Cancel", cancel.cx(), cancel.cy() + 9, 24, kInk,
                 gfx::Align::center);
        pop_layer(list);
    }

    void draw_confirm(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect panel{580, 390, 760, 300};
        push_layer(list, Layer::confirm, panel);
        glass_panel(list, glass, panel, kDialogRadius);
        // A bin, from the same shapes as the file icons.
        const float cx = panel.x + 78;
        const float cy = panel.y + 100;
        list.circle(cx, cy, 34, kDanger.with_alpha(0.14f));
        list.bordered_rect({cx - 12, cy - 9, 24, 26}, 4, kClear, 2.5f, kDanger);
        list.line(cx - 17, cy - 13, cx + 17, cy - 13, 2.5f, kDanger);
        list.line(cx - 5, cy - 18, cx + 5, cy - 18, 2.5f, kDanger);
        list.line(cx - 4, cy - 2, cx - 4, cy + 10, 2.0f, kDanger);
        list.line(cx + 4, cy - 2, cx + 4, cy + 10, 2.0f, kDanger);

        const float x = panel.x + 132;
        const float w = panel.w - 132 - 44;
        ui::text(list, fonts.semibold, confirm_title_, x, panel.y + 74, 30, kInk);
        fit_text(list, fonts.regular, confirm_name_, x, panel.y + 114, 24, kInk.with_alpha(0.9f),
                 w);
        fit_text(list, fonts.regular, confirm_body_, x, panel.y + 150, 22, kInk.with_alpha(0.55f),
                 w);

        // Two buttons, one highlight that glides between them.
        const float bw = (panel.w - 88 - 20) * 0.5f;
        const Rect left{panel.x + 44, panel.y + 196, bw, 60};
        const Rect right{left.x + bw + 20, left.y, bw, 60};
        const float at = confirm_pos_.value;
        Rect ring{left.x + (right.x - left.x) * at, left.y, bw, 60};
        ring.x += layer_nudge(Layer::confirm, true);
        const Color hot = gfx::mix(kAccent, kDanger, tween::clamp01(at));
        list.bordered_rect(left, kRadius + 4, kWhite.with_alpha(0.05f), 1.5f,
                           kWhite.with_alpha(0.16f));
        list.bordered_rect(right, kRadius + 4, kWhite.with_alpha(0.05f), 1.5f,
                           kWhite.with_alpha(0.16f));
        list.glow(ring, kRadius + 4, 14, hot.with_alpha(0.3f));
        list.rounded_rect(ring, kRadius + 4, hot);
        const Color dark = Color::rgb(0x0b1220);
        ui::text(list, fonts.semibold, "Cancel", left.cx(), left.cy() + 9, 26,
                 confirm_choice_ == 0 ? dark : kInk, gfx::Align::center);
        ui::text(list, fonts.semibold, "Delete", right.cx(), right.cy() + 9, 26,
                 confirm_choice_ == 1 ? dark : kDanger, gfx::Align::center);
        pop_layer(list);
    }

    void draw_rename(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float count = static_cast<float>(suggestions_.size());
        const Rect panel{560, 540 - (236 + count * 52) * 0.5f, 800, 236 + count * 52};
        push_layer(list, Layer::rename, panel);
        glass_panel(list, glass, panel, kDialogRadius);
        const float x = panel.x + 44;
        const float w = panel.w - 88;
        ui::text(list, fonts.semibold, "RENAME", x, panel.y + 54, 15, kAccent, gfx::Align::left,
                 3.0f);

        // The field: the name as far as it has been typed, then a caret.
        const Rect field{x, panel.y + 76, w, 64};
        const Node &entry = node(rename_);
        const std::string &wanted = suggestions_[static_cast<std::size_t>(rename_choice_)].name;
        const std::size_t typed = std::min(wanted.size(), static_cast<std::size_t>(typed_));
        list.bordered_rect(field, kRadius, Color::rgb(0x0b0e13, 0.6f), 2, kAccent);
        draw_icon(list, entry.kind, field.x + 34, field.cy(), 30);
        const float end = field.x + 64 +
                          fit_text(list, fonts.regular, std::string_view(wanted).substr(0, typed),
                                   field.x + 64, field.cy() + 9, 26, kInk, w - 100);
        const bool typing = typed < wanted.size();
        if (typing || std::fmod(clock_, 1.0f) < 0.55f || context_.settings.reduced_motion)
            list.rounded_rect({end + 3, field.cy() - 15, 2.5f, 30}, 1, kAccent);

        ui::text(list, fonts.semibold, "SUGGESTIONS", x, panel.y + 184, 15, kInk.with_alpha(0.5f),
                 gfx::Align::left, 3.0f);
        const float top = panel.y + 200;
        Rect ring{x - 12, top + rename_pos_.value * 52 + 2, w + 24, 48};
        ring.y += layer_nudge(Layer::rename, false);
        list.bordered_rect(ring, kRadius, kAccent.with_alpha(0.16f), 2, kAccent);
        for (std::size_t i = 0; i < suggestions_.size(); ++i)
        {
            const Suggestion &suggestion = suggestions_[i];
            const float cy = top + (static_cast<float>(i) + 0.5f) * 52;
            const bool focused = static_cast<int>(i) == rename_choice_;
            ui::text(list, fonts.semibold, suggestion.label, x + 8, cy + 7, 20,
                     focused ? kAccent : kInk.with_alpha(0.55f));
            fit_text(list, fonts.regular, suggestion.name, x + 150, cy + 8, 22,
                     kInk.with_alpha(focused ? 1.0f : 0.75f), w - 170);
        }
        pop_layer(list);
    }

    void draw_details(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect panel{400, 296, 1120, 488};
        push_layer(list, Layer::details, panel);
        glass_panel(list, glass, panel, kDialogRadius);
        const Node &entry = node(details_);
        draw_art(list, details_, {panel.x + 44, panel.y + 44, 480, 270});
        const float x = panel.x + 568;
        const float w = panel.w - 568 - 44;
        ui::text(list, fonts.semibold, ui::upper(label_of(entry.kind)), x, panel.y + 66, 15,
                 tint_of(entry.kind), gfx::Align::left, 3.0f);
        const float after =
            ui::paragraph(list, fonts.semibold, entry.name, x, panel.y + 110, 30, w, 38, kInk, 2);
        draw_facts(list, details_, x, after - 18, w, true);

        // Under the picture: where the entry sits on its drive.
        const int volume = volume_of(details_);
        if (volume >= 0)
        {
            const Volume &drive = volumes_[static_cast<std::size_t>(volume)];
            const float share = static_cast<float>(static_cast<double>(entry.total) /
                                                   static_cast<double>(drive.capacity));
            char text[64];
            const Rect bar{panel.x + 44, panel.y + 396, 480, 8};
            ui::text(list, fonts.semibold, "SHARE OF THE DRIVE", bar.x, bar.y - 22, 15,
                     kInk.with_alpha(0.5f), gfx::Align::left, 3.0f);
            std::snprintf(text, sizeof(text), "%.2f%%", static_cast<double>(share) * 100.0);
            ui::text(list, fonts.mono, text, bar.x + bar.w, bar.y - 20, kMonoSize,
                     kInk.with_alpha(0.9f), gfx::Align::right);
            list.rounded_rect(bar, 4, kWhite.with_alpha(0.1f));
            list.rounded_rect({bar.x, bar.y, std::max(8.0f, bar.w * share), bar.h}, 4,
                              tint_of(entry.kind));
            std::snprintf(text, sizeof(text), "on %s", node(drive.root).name.c_str());
            ui::text(list, fonts.regular, text, bar.x, bar.y + 44, 22, kInk.with_alpha(0.55f));
        }
        pop_layer(list);
    }

    // A toast in the bottom-left corner, on the hint row's line: results are
    // reported where the eye rests after an action, without covering the list.
    void draw_toast(gfx::DrawList &list) const
    {
        const float t = toast_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const float w = std::min(600.0f, fonts.regular.measure(toast_text_, 24)) + 76;
        const Rect pill{kMargin, 986 + 60.0f * (1.0f - t), w, 48};
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.shadow({pill.x, pill.y + 8, pill.w, pill.h}, 24, 24, Color::rgb(0x000000, 0.5f));
        list.rounded_rect(pill, 24, Color::rgb(0x222a36));
        list.bordered_rect(pill, 24, kClear, 1.5f, toast_color_.with_alpha(0.6f));
        list.circle(pill.x + 28, pill.cy(), 6, toast_color_);
        fit_text(list, fonts.regular, toast_text_, pill.x + 48, pill.cy() + 8.5f, 24, kInk, 600);
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        switch (layer_)
        {
        case Layer::none:
        {
            list.push_opacity(in(8) * (1.0f - tween::clamp01(dim_.value * 2.0f)));
            if (zone_ == Zone::rail)
            {
                const ui::Hint hints[] = {{ui::Button::cross, "Go to"},
                                          {ui::Button::circle, "Back to the list"}};
                ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            }
            else if (rows_.empty())
            {
                // Nothing to open, mark or act on: only the way out is offered.
                const ui::Hint hints[] = {{ui::Button::circle, "Up"}, {ui::Button::dpad, "Places"}};
                ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            }
            else
            {
                const bool marking = bar_.target > 0.5f;
                const ui::Hint hints[] = {
                    {ui::Button::cross, "Open"},
                    {ui::Button::circle, marking ? "Clear" : "Up"},
                    {ui::Button::square, "Select"},
                    {ui::Button::triangle, kSortHints[static_cast<int>(sort_)]},
                    {ui::Button::options, "Menu"},
                    {ui::Button::l2, "Page", ui::Button::r2},
                };
                ui::draw_hints(list, fonts, style, hints, 6, kRight, true);
            }
            list.pop_opacity();
            break;
        }
        case Layer::menu:
        case Layer::confirm:
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Choose"},
                                      {ui::Button::circle, "Cancel"}};
            ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
            break;
        }
        case Layer::picker:
        {
            const ui::Hint hints[] = {{ui::Button::cross, pick_move_ ? "Move here" : "Copy here"},
                                      {ui::Button::l2, "Drive", ui::Button::r2},
                                      {ui::Button::circle, "Cancel"}};
            ui::draw_hints(list, fonts, style, hints, 3, kRight, true);
            break;
        }
        case Layer::progress:
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Cancel"}};
            ui::draw_hints(list, fonts, style, hints, 1, kRight, true);
            break;
        }
        case Layer::rename:
        {
            const ui::Hint hints[] = {{ui::Button::dpad, "Suggestion"},
                                      {ui::Button::cross, "Rename"},
                                      {ui::Button::circle, "Cancel"}};
            ui::draw_hints(list, fonts, style, hints, 3, kRight, true);
            break;
        }
        case Layer::details:
        {
            const ui::Hint hints[] = {{ui::Button::circle, "Close"}};
            ui::draw_hints(list, fonts, style, hints, 1, kRight, true);
            break;
        }
        }
    }

    app::Context &context_;
    Tree tree_;
    std::vector<Volume> volumes_;
    std::vector<int> places_; // the rail: drive roots first, then pinned folders

    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion

    // The browser.
    int folder_ = 0;
    std::vector<int> path_;     // root .. folder_
    std::vector<int> old_path_; // ... and the path the breadcrumb is leaving
    std::vector<int> rows_;     // the folder's entries, in sort order
    int focus_ = 0;
    Sort sort_ = Sort::name;
    Zone zone_ = Zone::list;
    int place_ = 0;
    ui::Scroller scroll_;
    tween::Spring focus_offset_; // the highlight's glide, decaying to zero
    tween::Spring rail_y_;
    tween::Spring zone_fx_; // 1 while the list has the input
    tween::Spring arrow_x_; // the sort arrow, gliding between captions
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    std::vector<Ghost> ghosts_;

    // The folder being left, kept for the length of the slide.
    std::vector<int> out_rows_;
    int out_focus_ = 0;
    float out_scroll_ = 0.0f;
    int slide_direction_ = 0;
    tween::Timer slide_;
    tween::Timer crumb_;

    // The preview pane.
    int shown_ = 0;
    int previous_ = 0;
    tween::Timer preview_;

    // The storage gauge and the selection bar.
    tween::Spring gauge_[kKinds];
    tween::Spring free_gb_;
    tween::Spring bar_;
    int bar_count_ = 0;
    std::uint64_t bar_bytes_ = 0;

    // Glass layers.
    Layer layer_ = Layer::none;
    tween::Spring fx_[kLayers]; // each layer's presence, so they cross-fade
    tween::Spring dim_;
    int menu_item_ = 0;
    bool menu_single_ = true;
    float menu_cy_ = 0.0f; // the row the menu points at
    std::string menu_title_;
    tween::Spring menu_pos_;
    std::vector<PickRow> pick_rows_;
    int pick_focus_ = 0;
    bool pick_move_ = false;
    std::string pick_what_;
    std::string pick_size_;
    std::uint64_t pick_bytes_ = 0;
    int pick_from_ = 0; // the drive the entries are on
    tween::Spring pick_pos_;
    ui::Scroller pick_scroll_;
    int last_destination_ = -1;
    Job job_;
    std::string confirm_title_;
    std::string confirm_name_;
    std::string confirm_body_;
    int confirm_choice_ = 0;
    tween::Spring confirm_pos_;
    int rename_ = 0;
    std::vector<Suggestion> suggestions_;
    int rename_choice_ = 0;
    tween::Spring rename_pos_;
    float typed_ = 0.0f; // characters of the name the field shows so far
    int details_ = 0;

    std::string toast_text_;
    Color toast_color_ = kAccent;
    float toast_left_ = 0.0f;
    tween::Spring toast_;
};

} // namespace

std::unique_ptr<app::Concept> make_files(app::Context &context)
{
    return std::make_unique<Files>(context);
}

} // namespace hui::concepts
