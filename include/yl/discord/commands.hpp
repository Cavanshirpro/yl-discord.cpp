// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/client.hpp>
#include <yl/discord/arguments.hpp>
#include <tuple>
#include <unordered_map>

namespace yl::discord {

/// @brief Split quoted command arguments without evaluating shell expressions.
[[nodiscard]] YL_DISCORD_EXPORT std::vector<std::string> parse_arguments(std::string_view text);

/// @brief Owned command invocation with awaitable send/reply helpers.
class YL_DISCORD_EXPORT Context {
public:
    Message message;
    std::string command_name;
    std::vector<std::string> args;
    std::string invoked_with;

    template <typename T> [[nodiscard]] T argument(std::size_t index) const {
        if constexpr (detail::optional_argument<T>::value) {
            if (index >= args.size()) return std::nullopt;
            return convert_argument<typename detail::optional_argument<T>::type>(args[index]);
        } else {
            if (index >= args.size()) throw MissingRequiredArgument(index);
            return convert_argument<T>(args[index]);
        }
    }

    [[nodiscard]] task<Message> send(std::string content) const;
    [[nodiscard]] task<Message> reply(std::string content) const;
};

using CommandCheck = std::function<task<bool>(Context)>;

class YL_DISCORD_EXPORT CheckFailure : public CommandError {
public:
    CheckFailure() : CommandError("A command check rejected this invocation") {}
};

class YL_DISCORD_EXPORT CommandOnCooldown : public CommandError {
public:
    explicit CommandOnCooldown(std::chrono::steady_clock::duration retry_after)
        : CommandError("Command is on cooldown"), retry_after_(retry_after) {}
    [[nodiscard]] std::chrono::steady_clock::duration retry_after() const noexcept { return retry_after_; }
private:
    std::chrono::steady_clock::duration retry_after_;
};

class YL_DISCORD_EXPORT CooldownCapacityExceeded : public CommandError {
public:
    CooldownCapacityExceeded() : CommandError("Command cooldown bucket capacity reached") {}
};

enum class CooldownScope { global, user, channel, guild };

struct Cooldown {
    std::size_t rate = 1;
    std::chrono::milliseconds per = std::chrono::seconds(5);
    CooldownScope scope = CooldownScope::user;
    std::size_t max_buckets = 4096;
};

struct CommandOptions {
    std::vector<std::string> aliases;
    std::string description;
    std::string usage;
    bool hidden = false;
    std::vector<CommandCheck> checks;
    std::optional<Cooldown> cooldown;
};

struct CommandInfo {
    std::string name;
    std::vector<std::string> aliases;
    std::string description;
    std::string usage;
    bool hidden = false;
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
    template <typename... Args, typename Callback> requires (sizeof...(Args) > 0)
    Bot& command(std::string name, Callback callback, CommandOptions options = {});
    Bot& add_cog(std::shared_ptr<Cog> cog);
    /// @brief Remove a Cog's current commands; running invocations retain their owner.
    bool remove_cog(const std::shared_ptr<Cog>& cog);
    bool remove_command(std::string_view name);
    [[nodiscard]] std::vector<std::string> command_names() const;
    [[nodiscard]] std::optional<CommandInfo> get_command(std::string_view name) const;
    [[nodiscard]] std::string help_text(std::string_view command = {}) const;
    Bot& add_help_command(std::string name = "help");
    Bot& add_check(CommandCheck check);
    void clear_checks();
    bool reset_cooldown(std::string_view command, const Context& context);
    virtual task<> on_command_error(Context context, std::exception_ptr error);

protected:
    void handle_message(Message message) override;

private:
    struct Entry;
    void register_command(Command command, std::shared_ptr<Cog> owner = {});
    task<> process_commands(Message message);
    std::string prefix_;
    std::unordered_map<std::string, std::shared_ptr<Entry>> commands_;
    std::vector<std::shared_ptr<CommandCheck>> checks_;
};

namespace checks {
[[nodiscard]] YL_DISCORD_EXPORT CommandCheck guild_only();
[[nodiscard]] YL_DISCORD_EXPORT CommandCheck dm_only();
[[nodiscard]] YL_DISCORD_EXPORT CommandCheck user(Snowflake id);
} // namespace checks

namespace detail {
template <typename... Args>
consteval bool ordered_optional_arguments() {
    bool optional_seen = false;
    bool valid = true;
    ((valid = valid && (!optional_seen || optional_argument<Args>::value),
      optional_seen = optional_seen || optional_argument<Args>::value), ...);
    return valid;
}

template <typename... Args, std::size_t... I>
task<> invoke_typed_command(std::shared_ptr<std::function<task<>(Context, Args...)>> callback,
                            Context context, std::index_sequence<I...>) {
    if (context.args.size() > sizeof...(Args)) throw TooManyArguments{};
    // Braced initialization evaluates converters left to right before moving Context.
    std::tuple<Args...> values{context.template argument<Args>(I)...};
    auto work = std::apply([&](auto&&... value) {
        return (*callback)(std::move(context), std::move(value)...);
    }, std::move(values));
    co_await std::move(work);
}
} // namespace detail

template <typename... Args, typename Callback> requires (sizeof...(Args) > 0)
Bot& Bot::command(std::string name, Callback callback, CommandOptions options) {
    static_assert((std::same_as<Args, std::remove_cvref_t<Args>> && ...), "Use owning argument value types");
    static_assert(detail::ordered_optional_arguments<Args...>(), "Optional command arguments must be last");
    auto function = std::function<task<>(Context, Args...)>(std::move(callback));
    if (!function) throw std::invalid_argument("Command callback cannot be empty");
    auto stored = std::make_shared<decltype(function)>(std::move(function));
    if (options.usage.empty()) {
        const auto append = [&]<typename T>() {
            if (!options.usage.empty()) options.usage += ' ';
            constexpr bool optional = detail::optional_argument<T>::value;
            options.usage += optional ? '[' : '<';
            options.usage += detail::argument_type_name<T>();
            options.usage += optional ? ']' : '>';
        };
        (append.template operator()<Args>(), ...);
    }
    return command(std::move(name), [stored](Context context) {
        return detail::invoke_typed_command<Args...>(stored, std::move(context), std::index_sequence_for<Args...>{});
    }, std::move(options));
}

} // namespace yl::discord
