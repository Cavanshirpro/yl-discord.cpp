// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/testing/memory_transport.hpp>
#include "message_validation.hpp"
#include <utility>
#include <stdexcept>
#include <limits>
#include <algorithm>

namespace yl::discord::testing {

MemoryTransport::MemoryTransport(User self) : self_(std::move(self)) {}

task<Message> MemoryTransport::send_message(MessageRequest request) {
    detail::validate_message_parts(request.content, request.embeds, request.allowed_mentions, request.files);
    while (next_id_ != 0 && messages_.contains({request.channel_id.value(), next_id_})) ++next_id_;
    if (next_id_ == 0) throw std::overflow_error("MemoryTransport message ID space exhausted");
    sent_.push_back(request);
    Message result;
    result.id = Snowflake{next_id_++};
    result.channel_id = request.channel_id;
    result.author = self_;
    result.content = std::move(request.content);
    result.embeds = std::move(request.embeds);
    result.attachments = create_attachments(request.files);
    store(result);
    co_return result;
}

void MemoryTransport::store(Message message) {
    // Copy public fields into an unbound value to avoid transport -> Message -> transport cycles.
    Message snapshot;
    snapshot.id = message.id; snapshot.channel_id = message.channel_id;
    snapshot.guild_id = message.guild_id; snapshot.author = std::move(message.author);
    snapshot.content = std::move(message.content);
    snapshot.embeds = std::move(message.embeds);
    snapshot.attachments = std::move(message.attachments);
    messages_[{snapshot.channel_id.value(), snapshot.id.value()}] = std::move(snapshot);
}
Message& MemoryTransport::require(Snowflake channel, Snowflake message) {
    auto found = messages_.find({channel.value(), message.value()});
    if (found == messages_.end()) throw std::out_of_range("MemoryTransport has no message with this channel/ID");
    return found->second;
}
task<Message> MemoryTransport::fetch_message(Snowflake channel, Snowflake message) {
    auto result = require(channel, message);
    actions_.push_back({MessageActionKind::fetch, channel, message, {}, std::nullopt});
    co_return result;
}
task<Message> MemoryTransport::edit_message(Snowflake channel, Snowflake message, std::string content) {
    MessageEditOptions options;
    options.content = std::move(content);
    return edit_message(channel, message, std::move(options));
}
task<Message> MemoryTransport::edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) {
    validate_message_edit(options);
    auto& result = require(channel, message);
    auto attachments = result.attachments;
    if (options.retained_attachment_ids) {
        attachments.clear();
        for (auto id : *options.retained_attachment_ids) {
            const auto found = std::find_if(result.attachments.begin(), result.attachments.end(), [id](const auto& item) { return item.id == id; });
            if (found == result.attachments.end()) throw std::out_of_range("MemoryTransport cannot retain an unknown attachment");
            attachments.push_back(*found);
        }
    }
    auto uploaded = create_attachments(options.files);
    attachments.insert(attachments.end(), uploaded.begin(), uploaded.end());
    actions_.push_back({MessageActionKind::edit, channel, message, options.content.value_or(std::string{}), options});
    if (options.content) result.content = std::move(*options.content);
    if (options.embeds) result.embeds = std::move(*options.embeds);
    result.attachments = std::move(attachments);
    co_return result;
}

std::vector<Attachment> MemoryTransport::create_attachments(const std::vector<FileUpload>& files) {
    std::vector<Attachment> result;
    for (const auto& file : files) {
        if (next_attachment_id_ == 0) throw std::overflow_error("MemoryTransport attachment ID space exhausted");
        Attachment item;
        item.id = Snowflake{next_attachment_id_++}; item.filename = file.filename;
        item.description = file.description; item.content_type = file.content_type;
        item.size = file.size(); item.url = "memory://attachments/" + item.id.str() + "/" + file.filename;
        result.push_back(std::move(item));
    }
    return result;
}
task<> MemoryTransport::delete_message(Snowflake channel, Snowflake message) {
    (void)require(channel, message);
    actions_.push_back({MessageActionKind::erase, channel, message, {}, std::nullopt});
    messages_.erase({channel.value(), message.value()});
    co_return;
}
task<> MemoryTransport::add_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    (void)require(channel, message);
    actions_.push_back({MessageActionKind::add_reaction, channel, message, std::move(emoji), std::nullopt});
    co_return;
}
task<> MemoryTransport::remove_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    (void)require(channel, message);
    actions_.push_back({MessageActionKind::remove_reaction, channel, message, std::move(emoji), std::nullopt});
    co_return;
}

} // namespace yl::discord::testing
