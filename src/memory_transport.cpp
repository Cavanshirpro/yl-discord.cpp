// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/testing/memory_transport.hpp>
#include <utility>

namespace yl::discord::testing {

MemoryTransport::MemoryTransport(User self) : self_(std::move(self)) {}

task<Message> MemoryTransport::send_message(MessageRequest request) {
    sent_.push_back(request);
    Message result;
    result.id = Snowflake{next_id_++};
    result.channel_id = request.channel_id;
    result.author = self_;
    result.content = std::move(request.content);
    co_return result;
}

} // namespace yl::discord::testing
