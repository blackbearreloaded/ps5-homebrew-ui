// ps5-homebrew-ui - Describes every design and theme as JSON (host tool).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools/gen-docs.py turns this into the galleries of the documentation, so
// the lists there can never drift from what the app contains.

#include "manifest.hpp"

#include "audio/cues.hpp"
#include "ui/theme.hpp"

#include <cstdio>

namespace hui::host
{

namespace
{

void quoted(std::FILE *file, const char *text)
{
    std::fputc('"', file);
    for (const char *c = text; *c != 0; ++c)
    {
        if (*c == '"' || *c == '\\')
            std::fputc('\\', file);
        std::fputc(*c, file);
    }
    std::fputc('"', file);
}

void field(std::FILE *file, const char *name, const char *value, bool last = false)
{
    quoted(file, name);
    std::fputs(": ", file);
    quoted(file, value);
    if (!last)
        std::fputs(", ", file);
}

} // namespace

bool write_manifest(const char *path, const app::Shell &shell)
{
    std::FILE *file = std::fopen(path, "w");
    if (file == nullptr)
        return false;
    std::fputs("{\n  \"designs\": [\n", file);
    for (std::size_t i = 0; i < shell.concept_count(); ++i)
    {
        const app::ConceptInfo &info = shell.concept_at(i).info();
        std::fputs("    {", file);
        field(file, "id", info.id);
        field(file, "name", info.name);
        field(file, "tagline", info.tagline);
        field(file, "source", info.source);
        field(file, "sounds", audio::sound_set_name(info.sounds));
        std::fputs("\"techniques\": [", file);
        for (std::size_t t = 0; t < info.techniques.size(); ++t)
        {
            if (t != 0)
                std::fputs(", ", file);
            quoted(file, info.techniques[t]);
        }
        std::fprintf(file, "]}%s\n", i + 1 < shell.concept_count() ? "," : "");
    }
    std::fputs("  ],\n  \"themes\": [\n", file);
    const auto themes = ui::themes();
    for (std::size_t i = 0; i < themes.size(); ++i)
    {
        std::fputs("    {", file);
        field(file, "id", themes[i].id);
        field(file, "name", themes[i].name);
        field(file, "family", themes[i].family);
        field(file, "summary", themes[i].summary);
        std::fprintf(file, "\"dark\": %s}%s\n", themes[i].dark ? "true" : "false",
                     i + 1 < themes.size() ? "," : "");
    }
    std::fputs("  ]\n}\n", file);
    return std::fclose(file) == 0;
}

} // namespace hui::host
