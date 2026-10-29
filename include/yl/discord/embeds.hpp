// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace yl::discord {
struct EmbedField {
    std::string name;
    std::string value;
    bool inline_field = false;
};
struct EmbedFooter {
    std::string text;
    std::string icon_url;
};
struct EmbedAuthor {
    std::string name;
    std::string url;
    std::string icon_url;
};
/// @brief Owned supported embed fields. Discord determines rendering and URL availability.
struct Embed {
    std::string title;
    std::string description;
    std::string url;
    std::optional<std::uint32_t> color;
    std::string timestamp;
    std::vector<EmbedField> fields;
    std::optional<EmbedFooter> footer;
    std::optional<EmbedAuthor> author;
    std::string image_url;
    std::string thumbnail_url;
    // Incoming type is informational; outgoing embeds use Discord's rich type.
    std::string type = "rich";
};
} // namespace yl::discord
