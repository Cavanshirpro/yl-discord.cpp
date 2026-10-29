// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/commands.hpp>
#include <algorithm>
#include <cctype>
#include <deque>
#include <map>
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

struct Bot::Entry {
    std::shared_ptr<Cog> owner;
    Command command;
    using Clock = std::chrono::steady_clock;
    using Key = std::pair<int, std::uint64_t>;
    std::map<Key, std::deque<Clock::time_point>> buckets;

    Key key(const Context& context) const {
        switch (command.options.cooldown->scope) {
        case CooldownScope::global: return {0, 0};
        case CooldownScope::user: return {1, context.message.author.id.value()};
        case CooldownScope::channel: return {2, context.message.channel_id.value()};
        case CooldownScope::guild:
            if (context.message.guild_id) return {3, context.message.guild_id->value()};
            return {2, context.message.channel_id.value()};
        }
        throw std::logic_error("Invalid cooldown scope");
    }

    void consume(const Context& context) {
        if (!command.options.cooldown) return;
        const auto& cooldown = *command.options.cooldown;
        const auto now = Clock::now();
        const auto id = key(context);
        auto found = buckets.find(id);
        if (found == buckets.end()) {
            // Only expired buckets may be reclaimed: evicting active users bypasses limits.
            if (buckets.size() >= cooldown.max_buckets) {
                std::erase_if(buckets, [&](const auto& pair) {
                    return pair.second.empty() || now - pair.second.back() >= cooldown.per;
                });
            }
            if (buckets.size() >= cooldown.max_buckets) throw CooldownCapacityExceeded{};
            found = buckets.emplace(id, std::deque<Clock::time_point>{}).first;
        }
        auto& uses = found->second;
        while (!uses.empty() && now - uses.front() >= cooldown.per) uses.pop_front();
        if (uses.size() >= cooldown.rate) throw CommandOnCooldown(cooldown.per - (now - uses.front()));
        uses.push_back(now);
    }
};

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
task<Message> Context::send(MessageOptions options) const { return Message::send_impl(message, std::move(options), false); }
task<Message> Context::reply(MessageOptions options) const { return message.reply(std::move(options)); }

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
    for (const auto& check : command.options.checks)
        if (!check) throw std::invalid_argument("Command check cannot be empty");
    if (command.options.cooldown) {
        const auto& cooldown = *command.options.cooldown;
        if (cooldown.rate == 0 || cooldown.per <= std::chrono::milliseconds::zero() ||
            cooldown.per > std::chrono::hours(24 * 365) || cooldown.max_buckets == 0)
            throw std::invalid_argument("Cooldown requires positive rate/capacity and a period within one year");
        switch (cooldown.scope) {
        case CooldownScope::global: case CooldownScope::user: case CooldownScope::channel: case CooldownScope::guild: break;
        default: throw std::invalid_argument("Invalid cooldown scope");
        }
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
    auto entry = std::make_shared<Entry>(Entry{std::move(owner), std::move(command), {}});
    for (auto& name : names) {
        commands_.emplace(std::move(name), entry);
    }
}

bool Bot::remove_cog(const std::shared_ptr<Cog>& cog) {
    if (!cog) return false;
    return std::erase_if(commands_, [&](const auto& pair) { return pair.second->owner == cog; }) != 0;
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

std::optional<CommandInfo> Bot::get_command(std::string_view name) const {
    auto found = commands_.find(std::string(name));
    if (found == commands_.end()) return std::nullopt;
    const auto& command = found->second->command;
    return CommandInfo{command.name, command.options.aliases, command.options.description,
                       command.options.usage, command.options.hidden};
}

std::string Bot::help_text(std::string_view name) const {
    if (!name.empty()) {
        auto info = get_command(name);
        if (!info) return "Unknown command.";
        std::string result = prefix_ + info->name;
        if (!info->usage.empty()) result += " " + info->usage;
        if (!info->description.empty()) result += "\n" + info->description;
        if (!info->aliases.empty()) {
            result += "\nAliases:";
            for (const auto& alias : info->aliases) result += " " + prefix_ + alias;
        }
        return result;
    }
    std::string result = "Commands:";
    for (const auto& command : command_names()) {
        const auto info = *get_command(command);
        if (info.hidden) continue;
        result += "\n" + prefix_ + info.name;
        if (!info.usage.empty()) result += " " + info.usage;
        if (!info.description.empty()) result += " - " + info.description;
    }
    return result;
}

Bot& Bot::add_help_command(std::string name) {
    CommandOptions options;
    options.description = "Show available commands or help for one command";
    options.usage = "[command]";
    return command(std::move(name), [this](Context context) -> task<> {
        if (context.args.size() > 1) throw TooManyArguments{};
        const auto text = help_text(context.args.empty() ? std::string_view{} : context.args.front());
        std::size_t begin = 0;
        while (begin < text.size()) {
            // A byte budget is conservative for Discord's UTF-16 character limit.
            auto end = (std::min)(begin + 1900, text.size());
            if (end < text.size()) {
                while (end > begin && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
                if (end == begin) end = (std::min)(begin + 1900, text.size());
                const auto newline = text.rfind('\n', end - 1);
                if (newline != std::string::npos && newline > begin) end = newline + 1;
            }
            co_await context.send(text.substr(begin, end - begin));
            begin = end;
        }
    }, std::move(options));
}

Bot& Bot::add_check(CommandCheck check) {
    if (!check) throw std::invalid_argument("Command check cannot be empty");
    checks_.push_back(std::make_shared<CommandCheck>(std::move(check)));
    return *this;
}

void Bot::clear_checks() { checks_.clear(); }

bool Bot::reset_cooldown(std::string_view command, const Context& context) {
    auto found = commands_.find(std::string(command));
    if (found == commands_.end() || !found->second->command.options.cooldown) return false;
    return found->second->buckets.erase(found->second->key(context)) != 0;
}

task<> Bot::on_command_error(Context, std::exception_ptr error) {
    std::rethrow_exception(error);
    co_return;
}

void Bot::handle_message(Message message) {
    Client::handle_message(message);
    if (!message.author.bot && message.content.starts_with(prefix_)) {
        spawn(process_commands(std::move(message)), "commands");
    }
}

task<> Bot::process_commands(Message message) {
    Context context;
    context.message = std::move(message);
    std::exception_ptr failure;
    try {
        auto arguments = parse_arguments(std::string_view(context.message.content).substr(prefix_.size()));
        if (arguments.empty()) co_return;
        auto found = commands_.find(arguments.front());
        if (found == commands_.end()) co_return;
        // Keep callback/check captures and Cog alive across suspension and removal.
        auto entry = found->second;
        context.command_name = entry->command.name;
        context.invoked_with = std::move(arguments.front());
        arguments.erase(arguments.begin());
        context.args = std::move(arguments);
        const auto checks = checks_;
        for (const auto& check : checks)
            if (!(co_await (*check)(context))) throw CheckFailure{};
        for (const auto& check : entry->command.options.checks)
            if (!(co_await check(context))) throw CheckFailure{};
        entry->consume(context);
        co_await entry->command.callback(context);
        co_return;
    } catch (...) { failure = std::current_exception(); }
    co_await on_command_error(std::move(context), failure);
}

namespace checks {
CommandCheck guild_only() {
    return [](Context context) -> task<bool> { co_return context.message.guild_id.has_value(); };
}
CommandCheck dm_only() {
    return [](Context context) -> task<bool> { co_return !context.message.guild_id.has_value(); };
}
CommandCheck user(Snowflake id) {
    return [id](Context context) -> task<bool> { co_return context.message.author.id == id; };
}
} // namespace checks

} // namespace yl::discord
