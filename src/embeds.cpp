// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "message_validation.hpp"
#include "file_validation.hpp"
#include "text_internal.hpp"
#include <stdexcept>
#include <unordered_set>

namespace yl::discord {
namespace {
std::size_t units(std::string_view text, std::size_t limit) {
    return detail::text_units(text, limit);
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

void detail::validate_message_parts(std::string_view content, const std::vector<Embed>& values, const AllowedMentions& allowed, const std::vector<FileUpload>& files) {
    if (content.empty() && values.empty() && files.empty()) throw std::invalid_argument("Message requires content, embeds or files");
    (void)units(content, 2000); embeds(values); mentions(allowed); validate_upload_files(files);
}
void validate_message_options(const MessageOptions& options) { detail::validate_message_parts(options.content, options.embeds, options.allowed_mentions, options.files); }
void validate_message_edit(const MessageEditOptions& options) {
    if (!options.content && !options.embeds && !options.retained_attachment_ids && options.files.empty())
        throw std::invalid_argument("Edit requires content, embeds or attachments");
    if (options.content) (void)units(*options.content, 2000);
    if (options.embeds) embeds(*options.embeds);
    mentions(options.allowed_mentions);
    if (!options.files.empty() && !options.retained_attachment_ids)
        throw std::invalid_argument("File edits require an explicit list of attachments to retain");
    detail::validate_upload_files(options.files, options.retained_attachment_ids ? options.retained_attachment_ids->size() : 0);
    if (options.retained_attachment_ids) {
        std::unordered_set<std::uint64_t> unique;
        for (auto id : *options.retained_attachment_ids) if (id.value() == 0 || !unique.insert(id.value()).second)
            throw std::invalid_argument("Retained attachments require distinct nonzero IDs");
    }
}
} // namespace yl::discord
