// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/transport.hpp>
#include <map>
#include <vector>

namespace yl::discord::testing {

enum class MessageActionKind { fetch, edit, erase, add_reaction, remove_reaction };
struct MessageAction {
    MessageActionKind kind;
    Snowflake channel_id;
    Snowflake message_id;
    std::string value;
    std::optional<MessageEditOptions> edit_options;
};

// Test double. Access its records only on the event loop or after run() returns.
class YL_DISCORD_EXPORT MemoryTransport final : public MessageTransport {
public:
    explicit MemoryTransport(User self = User{Snowflake{1}, "offline-bot", true});
    task<Message> send_message(MessageRequest request) override;
    task<Message> fetch_message(Snowflake channel, Snowflake message) override;
    task<Message> edit_message(Snowflake channel, Snowflake message, std::string content) override;
    task<Message> edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) override;
    task<> delete_message(Snowflake channel, Snowflake message) override;
    task<> add_reaction(Snowflake channel, Snowflake message, std::string emoji) override;
    task<> remove_reaction(Snowflake channel, Snowflake message, std::string emoji) override;
    /// @brief Seed a fixture snapshot without retaining its bound transport or recording I/O.
    void store(Message message);
    [[nodiscard]] const std::vector<MessageRequest>& sent() const noexcept { return sent_; }
    [[nodiscard]] const std::vector<MessageAction>& actions() const noexcept { return actions_; }

private:
    User self_;
    std::uint64_t next_id_ = 1;
    std::uint64_t next_attachment_id_ = 1'000'000;
    std::vector<MessageRequest> sent_;
    std::vector<MessageAction> actions_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, Message> messages_;
    Message& require(Snowflake channel, Snowflake message);
    std::vector<Attachment> create_attachments(const std::vector<FileUpload>& files);
};

} // namespace yl::discord::testing
