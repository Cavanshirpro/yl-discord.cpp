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

task<Message> Message::send_impl(Message source, std::string content, bool reply) {
    if (!source.transport_) {
        throw std::logic_error("Message is not bound to a transport; dispatch it through a Client first");
    }
    MessageRequest request{source.channel_id, std::move(content), std::nullopt};
    if (reply) {
        request.reply_to = source.id;
    }
    auto result = co_await source.transport_->send_message(std::move(request));
    result.transport_ = source.transport_;
    co_return result;
}

} // namespace yl::discord
