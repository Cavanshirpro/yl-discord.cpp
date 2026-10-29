// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/models.hpp>
#include <yl/discord/transport.hpp>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace yl::discord {

Snowflake::Snowflake(std::string_view text) {
    if (text.empty()) {
        throw std::invalid_argument("Snowflake cannot be empty");
    }
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value_);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("Snowflake must be an unsigned 64-bit decimal integer");
    }
}

std::chrono::sys_time<std::chrono::milliseconds> Snowflake::created_at() const noexcept {
    constexpr std::uint64_t discord_epoch_ms = 1420070400000ULL;
    return std::chrono::sys_time<std::chrono::milliseconds>{
        std::chrono::milliseconds{(value_ >> 22) + discord_epoch_ms}};
}

task<Message> Message::reply(std::string reply_content) const {
    return send_impl(*this, std::move(reply_content), true);
}
task<Message> Message::reply(MessageOptions options) const { return send_impl(*this, std::move(options), true); }

task<Message> Message::send_impl(Message source, std::string content, bool reply) {
    MessageOptions options;
    options.content = std::move(content);
    return send_impl(std::move(source), std::move(options), reply);
}
task<Message> Message::send_impl(Message source, MessageOptions options, bool reply) {
    source.require_transport();
    validate_message_options(options);
    MessageRequest request{source.channel_id, std::move(options.content), std::nullopt,
                           std::move(options.embeds), std::move(options.allowed_mentions), std::move(options.files)};
    if (reply) {
        request.reply_to = source.id;
    }
    auto result = co_await source.transport_->send_message(std::move(request));
    result.transport_ = source.transport_;
    co_return result;
}

void Message::require_transport() const {
    if (!transport_) throw std::logic_error("Message is not bound to a transport; dispatch it through a Client first");
}
task<Message> Message::fetch() const { return fetch_impl(*this); }
task<Message> Message::edit(std::string content) const { return edit_impl(*this, std::move(content)); }
task<Message> Message::edit(MessageEditOptions options) const { return edit_impl(*this, std::move(options)); }
task<> Message::delete_message() const { return delete_impl(*this); }
task<> Message::add_reaction(std::string emoji) const { return reaction_impl(*this, std::move(emoji), false); }
task<> Message::remove_reaction(std::string emoji) const { return reaction_impl(*this, std::move(emoji), true); }

task<Message> Message::fetch_impl(Message source) {
    source.require_transport();
    auto result = co_await source.transport_->fetch_message(source.channel_id, source.id);
    result.transport_ = source.transport_;
    co_return result;
}
task<Message> Message::edit_impl(Message source, std::string content) {
    MessageEditOptions options;
    options.content = std::move(content);
    return edit_impl(std::move(source), std::move(options));
}
task<Message> Message::edit_impl(Message source, MessageEditOptions options) {
    source.require_transport();
    validate_message_edit(options);
    auto result = co_await source.transport_->edit_message(source.channel_id, source.id, std::move(options));
    result.transport_ = source.transport_;
    co_return result;
}
task<> Message::delete_impl(Message source) {
    source.require_transport();
    co_await source.transport_->delete_message(source.channel_id, source.id);
}
task<> Message::reaction_impl(Message source, std::string emoji, bool remove) {
    source.require_transport();
    if (remove) co_await source.transport_->remove_reaction(source.channel_id, source.id, std::move(emoji));
    else co_await source.transport_->add_reaction(source.channel_id, source.id, std::move(emoji));
}

} // namespace yl::discord
