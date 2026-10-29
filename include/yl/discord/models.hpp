// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/async.hpp>
#include <yl/discord/export.hpp>
#include <chrono>
#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace yl::discord {

class MessageTransport;
class Client;
class Context;

/// @brief A lossless unsigned Discord ID, convertible to its creation timestamp.
class YL_DISCORD_EXPORT Snowflake {
public:
    constexpr Snowflake() noexcept = default;
    explicit constexpr Snowflake(std::uint64_t value) noexcept : value_(value) {}
    explicit Snowflake(std::string_view value);

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] std::string str() const { return std::to_string(value_); }
    [[nodiscard]] std::chrono::sys_time<std::chrono::milliseconds> created_at() const noexcept;
    auto operator<=>(const Snowflake&) const = default;

private:
    std::uint64_t value_ = 0;
};

enum class Intent : std::uint64_t {
    guilds = 1ULL << 0,
    guild_members = 1ULL << 1,
    guild_presences = 1ULL << 8,
    guild_messages = 1ULL << 9,
    direct_messages = 1ULL << 12,
    message_content = 1ULL << 15
};

class Intents {
public:
    constexpr Intents() noexcept = default;
    explicit constexpr Intents(std::uint64_t bits) noexcept : bits_(bits) {}
    [[nodiscard]] static constexpr Intents defaults() noexcept {
        return Intents{}.with(Intent::guilds).with(Intent::guild_messages).with(Intent::direct_messages);
    }
    [[nodiscard]] constexpr Intents with(Intent intent) const noexcept {
        return Intents(bits_ | static_cast<std::uint64_t>(intent));
    }
    [[nodiscard]] constexpr bool contains(Intent intent) const noexcept {
        return (bits_ & static_cast<std::uint64_t>(intent)) != 0;
    }
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return bits_; }

private:
    std::uint64_t bits_ = 0;
};

struct User {
    Snowflake id;
    std::string name;
    bool bot = false;
};

/// @brief An owned message snapshot; reply() needs a Client-bound transport.
struct YL_DISCORD_EXPORT Message {
    Snowflake id;
    Snowflake channel_id;
    std::optional<Snowflake> guild_id;
    User author;
    std::string content;

    [[nodiscard]] task<Message> reply(std::string content) const;
    [[nodiscard]] task<Message> fetch() const;
    [[nodiscard]] task<Message> edit(std::string content) const;
    [[nodiscard]] task<> delete_message() const;
    [[nodiscard]] task<> add_reaction(std::string emoji) const;
    [[nodiscard]] task<> remove_reaction(std::string emoji) const;

private:
    static task<Message> send_impl(Message source, std::string content, bool reply);
    static task<Message> fetch_impl(Message source);
    static task<Message> edit_impl(Message source, std::string content);
    static task<> delete_impl(Message source);
    static task<> reaction_impl(Message source, std::string emoji, bool remove);
    void require_transport() const;
    std::shared_ptr<MessageTransport> transport_;
    friend class Client;
    friend class Context;
};

struct Ready {
    User user;
};

// Raw payloads stay available even when no high-level model exists yet.
struct RawEvent {
    std::string name;
    std::string json;
    std::optional<std::uint64_t> sequence;
};

} // namespace yl::discord
