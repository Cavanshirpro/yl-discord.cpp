// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/transport.hpp>
#include <stdexcept>

namespace yl::discord {
task<Message> MessageTransport::fetch_message(Snowflake, Snowflake) {
    throw std::logic_error("This transport does not support fetching messages"); co_return Message{};
}
task<Message> MessageTransport::edit_message(Snowflake, Snowflake, std::string) {
    throw std::logic_error("This transport does not support editing messages"); co_return Message{};
}
task<Message> MessageTransport::edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) {
    validate_message_edit(options);
    if (options.embeds || options.allowed_mentions != AllowedMentions{})
        throw std::logic_error("This transport does not support structured message edits");
    co_return co_await edit_message(channel, message, std::move(*options.content));
}
task<> MessageTransport::delete_message(Snowflake, Snowflake) {
    throw std::logic_error("This transport does not support deleting messages"); co_return;
}
task<> MessageTransport::add_reaction(Snowflake, Snowflake, std::string) {
    throw std::logic_error("This transport does not support reactions"); co_return;
}
task<> MessageTransport::remove_reaction(Snowflake, Snowflake, std::string) {
    throw std::logic_error("This transport does not support reactions"); co_return;
}
} // namespace yl::discord
