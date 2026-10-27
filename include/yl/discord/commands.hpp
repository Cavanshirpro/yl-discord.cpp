// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/client.hpp>
#include <stdexcept>
#include <unordered_map>

namespace yl::discord {

/// @brief Invalid command argument quoting or trailing escape.
class YL_DISCORD_EXPORT ArgumentError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// @brief Split quoted command arguments without evaluating shell expressions.
[[nodiscard]] YL_DISCORD_EXPORT std::vector<std::string> parse_arguments(std::string_view text);

/// @brief Owned command invocation with awaitable send/reply helpers.
class YL_DISCORD_EXPORT Context {
public:
    Message message;
    std::string command_name;
    std::vector<std::string> args;

    [[nodiscard]] task<Message> send(std::string content) const;
    [[nodiscard]] task<Message> reply(std::string content) const;
};

struct CommandOptions {
    std::vector<std::string> aliases;
    std::string description;
};

struct Command {
    std::string name;
    std::function<task<>(Context)> callback;
    CommandOptions options;
};

/// @brief A class-provided collection of commands owned while registered or running.
class YL_DISCORD_EXPORT Cog {
public:
    virtual ~Cog() = default;
    [[nodiscard]] virtual std::vector<Command> commands() = 0;
};

struct BotOptions {
    std::string command_prefix = "!";
    Intents intents = Intents::defaults();
    ConnectionOptions connection;
};

/// @brief Async Client with prefix commands, aliases and owned Cog callbacks.
class YL_DISCORD_EXPORT Bot : public Client {
public:
    explicit Bot(BotOptions options = {});
    explicit Bot(std::shared_ptr<MessageTransport> transport, BotOptions options = {});
    Bot& command(std::string name, std::function<task<>(Context)> callback, CommandOptions options = {});
    Bot& add_cog(std::shared_ptr<Cog> cog);
    bool remove_command(std::string_view name);
    [[nodiscard]] std::vector<std::string> command_names() const;

protected:
    void handle_message(Message message) override;

private:
    struct Entry {
        std::shared_ptr<Cog> owner;
        Command command;
    };
    void register_command(Command command, std::shared_ptr<Cog> owner = {});
    task<> process_commands(Message message);
    std::string prefix_;
    std::unordered_map<std::string, std::shared_ptr<Entry>> commands_;
};

} // namespace yl::discord
