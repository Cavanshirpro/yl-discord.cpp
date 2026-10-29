// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "message_validation.hpp"
#include <stdexcept>
#include <unordered_set>

namespace yl::discord {
namespace {
std::size_t units(std::string_view text, std::size_t limit) {
    std::size_t total = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        std::uint32_t code = first;
        unsigned remaining = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; remaining = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; remaining = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; remaining = 3; minimum = 0x10000; }
        else if (first > 0x7f) throw std::invalid_argument("Message text must be valid UTF-8");
        if (remaining > text.size() - i) throw std::invalid_argument("Truncated UTF-8 in message text");
        while (remaining--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation in message text");
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            throw std::invalid_argument("Invalid Unicode scalar in message text");
        total += code > 0xffff ? 2 : 1;
        if (total > limit) throw std::invalid_argument("Message or embed text exceeds its character limit");
    }
    return total;
}
void url(std::string_view value, bool attachment) {
    if (value.empty()) return;
    (void)units(value, 8192);
    if (!value.starts_with("https://") && !value.starts_with("http://") && !(attachment && value.starts_with("attachment://")))
        throw std::invalid_argument("Embed URLs require http(s), or attachment URLs for media");
    for (unsigned char byte : value) if (byte <= 32 || byte == 127) throw std::invalid_argument("Embed URLs cannot contain whitespace or controls");
}
void mentions(const AllowedMentions& value) {
    if ((value.all_users && !value.users.empty()) || (value.all_roles && !value.roles.empty()))
        throw std::invalid_argument("Mention parse flags cannot be combined with the same type's ID whitelist");
    for (const auto* ids : {&value.users, &value.roles}) {
        if (ids->size() > 100) throw std::invalid_argument("Mention ID whitelists are limited to 100 IDs");
        std::unordered_set<std::uint64_t> unique;
        for (auto id : *ids) if (id.value() == 0 || !unique.insert(id.value()).second)
            throw std::invalid_argument("Mention ID whitelists require distinct nonzero IDs");
    }
}
void embeds(const std::vector<Embed>& values) {
    if (values.size() > 10) throw std::invalid_argument("A message supports at most 10 embeds");
    std::size_t total = 0;
    for (const auto& embed : values) {
        total += units(embed.title, 256) + units(embed.description, 4096);
        if (embed.fields.size() > 25) throw std::invalid_argument("An embed supports at most 25 fields");
        if (embed.color && *embed.color > 0xffffff) throw std::invalid_argument("Embed color must fit in 24 bits");
        for (const auto& field : embed.fields) {
            if (field.name.empty() || field.value.empty()) throw std::invalid_argument("Embed fields require a name and value");
            total += units(field.name, 256) + units(field.value, 1024);
        }
        if (embed.footer) {
            if (embed.footer->text.empty()) throw std::invalid_argument("Embed footer requires text");
            total += units(embed.footer->text, 2048); url(embed.footer->icon_url, true);
        }
        if (embed.author) {
            if (embed.author->name.empty()) throw std::invalid_argument("Embed author requires a name");
            total += units(embed.author->name, 256); url(embed.author->url, false); url(embed.author->icon_url, true);
        }
        url(embed.url, false); url(embed.image_url, true); url(embed.thumbnail_url, true);
        (void)units(embed.timestamp, 128);
        if (total > 6000) throw std::invalid_argument("Combined embed text exceeds 6000 characters");
        if (embed.title.empty() && embed.description.empty() && embed.fields.empty() && !embed.footer && !embed.author &&
            embed.image_url.empty() && embed.thumbnail_url.empty()) throw std::invalid_argument("Embed requires displayable content");
    }
}
} // namespace

void detail::validate_message_parts(std::string_view content, const std::vector<Embed>& values, const AllowedMentions& allowed) {
    if (content.empty() && values.empty()) throw std::invalid_argument("Message requires content or embeds");
    (void)units(content, 2000); embeds(values); mentions(allowed);
}
void validate_message_options(const MessageOptions& options) { detail::validate_message_parts(options.content, options.embeds, options.allowed_mentions); }
void validate_message_edit(const MessageEditOptions& options) {
    if (!options.content && !options.embeds) throw std::invalid_argument("Edit requires content or embeds");
    if (options.content) (void)units(*options.content, 2000);
    if (options.embeds) embeds(*options.embeds);
    mentions(options.allowed_mentions);
}
} // namespace yl::discord
