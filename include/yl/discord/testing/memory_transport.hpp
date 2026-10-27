// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/transport.hpp>
#include <vector>

namespace yl::discord::testing {

// Test double. Access its records only on the event loop or after run() returns.
class YL_DISCORD_EXPORT MemoryTransport final : public MessageTransport {
public:
    explicit MemoryTransport(User self = User{Snowflake{1}, "offline-bot", true});
    task<Message> send_message(MessageRequest request) override;
    [[nodiscard]] const std::vector<MessageRequest>& sent() const noexcept { return sent_; }

private:
    User self_;
    std::uint64_t next_id_ = 1;
    std::vector<MessageRequest> sent_;
};

} // namespace yl::discord::testing
