// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/commands.hpp>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

namespace yl::discord {
namespace {

bool space(char value) { return std::isspace(static_cast<unsigned char>(value)) != 0; }

void validate_name(const std::string& name) {
    if (name.empty() || std::any_of(name.begin(), name.end(), [](char c) {
            return space(c) || c == '\'' || c == '"' || c == '\\';
        })) {
        throw std::invalid_argument("Command names must be nonempty and contain no whitespace, quotes or backslashes");
    }
}

} // namespace

std::vector<std::string> parse_arguments(std::string_view text) {
    std::vector<std::string> result;
    std::string current;
    char quote = '\0';
    bool escaped = false;
    bool started = false;
    for (char c : text) {
        if (escaped) {
            current += c;
            escaped = false;
        } else if (c == '\\' && quote != '\'') {
            escaped = true;
            started = true;
        } else if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else {
                current += c;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
            started = true;
        } else if (space(c)) {
            if (started) {
                result.push_back(std::move(current));
                current.clear();
                started = false;
            }
        } else {
            current += c;
            started = true;
        }
    }
    if (escaped || quote != '\0') {
        throw ArgumentError(escaped ? "Trailing escape in command arguments" : "Unclosed quote in command arguments");
    }
    if (started) {
        result.push_back(std::move(current));
    }
    return result;
}

task<Message> Context::send(std::string content) const {
    return Message::send_impl(message, std::move(content), false);
}

task<Message> Context::reply(std::string content) const { return message.reply(std::move(content)); }

Bot::Bot(BotOptions options)
    : Client(ClientOptions{options.intents, options.connection}), prefix_(std::move(options.command_prefix)) {
    if (prefix_.empty() || std::any_of(prefix_.begin(), prefix_.end(), space))
        throw std::invalid_argument("Command prefix must be nonempty and contain no whitespace");
}
Bot::Bot(std::shared_ptr<MessageTransport> transport, BotOptions options)
    : Client(std::move(transport), ClientOptions{options.intents, options.connection}), prefix_(std::move(options.command_prefix)) {
    if (prefix_.empty() || std::any_of(prefix_.begin(), prefix_.end(), space)) {
        throw std::invalid_argument("Command prefix must be nonempty and contain no whitespace");
    }
}

Bot& Bot::command(std::string name, std::function<task<>(Context)> callback, CommandOptions options) {
    register_command(Command{std::move(name), std::move(callback), std::move(options)});
    return *this;
}

void Bot::register_command(Command command, std::shared_ptr<Cog> owner) {
    if (!command.callback) {
        throw std::invalid_argument("Command callback cannot be empty");
    }
    std::vector<std::string> names{command.name};
    names.insert(names.end(), command.options.aliases.begin(), command.options.aliases.end());
    std::unordered_set<std::string> unique;
    for (const auto& name : names) {
        validate_name(name);
        if (commands_.contains(name) || !unique.insert(name).second) {
            throw std::invalid_argument("Duplicate command or alias: " + name);
        }
    }
    auto entry = std::make_shared<Entry>(Entry{std::move(owner), std::move(command)});
    for (auto& name : names) {
        commands_.emplace(std::move(name), entry);
    }
}

Bot& Bot::add_cog(std::shared_ptr<Cog> cog) {
    if (!cog) {
        throw std::invalid_argument("Cog cannot be null");
    }
    auto definitions = cog->commands();
    auto previous = commands_;
    try {
        for (auto& definition : definitions) {
            register_command(std::move(definition), cog);
        }
    } catch (...) {
        commands_ = std::move(previous);
        throw;
    }
    return *this;
}

bool Bot::remove_command(std::string_view name) {
    auto found = commands_.find(std::string(name));
    if (found == commands_.end()) {
        return false;
    }
    auto entry = found->second;
    std::erase_if(commands_, [&](const auto& pair) { return pair.second == entry; });
    return true;
}

std::vector<std::string> Bot::command_names() const {
    std::vector<std::string> names;
    for (const auto& [name, entry] : commands_) {
        if (name == entry->command.name) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void Bot::handle_message(Message message) {
    Client::handle_message(message);
    if (!message.author.bot && message.content.starts_with(prefix_)) {
        spawn(process_commands(std::move(message)), "commands");
    }
}

task<> Bot::process_commands(Message message) {
    auto arguments = parse_arguments(std::string_view(message.content).substr(prefix_.size()));
    if (arguments.empty()) {
        co_return;
    }
    auto found = commands_.find(arguments.front());
    if (found == commands_.end()) {
        co_return;
    }
    // Keep callback captures and Cog alive even if the registry changes while awaiting.
    auto entry = found->second;
    Context context;
    context.message = std::move(message);
    context.command_name = entry->command.name;
    arguments.erase(arguments.begin());
    context.args = std::move(arguments);
    co_await entry->command.callback(std::move(context));
}

} // namespace yl::discord
