// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/models.hpp>
#include <optional>
#include <string>

namespace yl::discord {

/// @brief Owned outgoing payload; name avoids the Windows SendMessage macro.
struct MessageRequest {
    Snowflake channel_id;
    std::string content;
    std::optional<Snowflake> reply_to;
    std::vector<Embed> embeds;
    AllowedMentions allowed_mentions;
};

// Implementations must suspend for I/O rather than blocking the event-loop thread.
// Optional operations default to lazy unsupported-operation errors for older transports.
class YL_DISCORD_EXPORT MessageTransport {
public:
    virtual ~MessageTransport() = default;
    [[nodiscard]] virtual task<Message> send_message(MessageRequest request) = 0;
    [[nodiscard]] virtual task<Message> fetch_message(Snowflake channel, Snowflake message);
    [[nodiscard]] virtual task<Message> edit_message(Snowflake channel, Snowflake message, std::string content);
    [[nodiscard]] virtual task<Message> edit_message(Snowflake channel, Snowflake message, MessageEditOptions options);
    [[nodiscard]] virtual task<> delete_message(Snowflake channel, Snowflake message);
    [[nodiscard]] virtual task<> add_reaction(Snowflake channel, Snowflake message, std::string emoji);
    /// @brief Remove only the current bot's reaction.
    [[nodiscard]] virtual task<> remove_reaction(Snowflake channel, Snowflake message, std::string emoji);
};

} // namespace yl::discord
