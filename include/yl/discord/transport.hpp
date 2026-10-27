// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/models.hpp>
#include <optional>
#include <string>

namespace yl::discord {

struct SendMessage {
    Snowflake channel_id;
    std::string content;
    std::optional<Snowflake> reply_to;
};

// Implementations must suspend for I/O rather than blocking the event-loop thread.
// This narrow port covers message sending, not the future complete REST API.
class YL_DISCORD_EXPORT MessageTransport {
public:
    virtual ~MessageTransport() = default;
    [[nodiscard]] virtual task<Message> send_message(SendMessage request) = 0;
};

} // namespace yl::discord
