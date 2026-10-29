// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/models.hpp>
#include <charconv>
#include <cmath>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace yl::discord {

class YL_DISCORD_EXPORT CommandError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class YL_DISCORD_EXPORT ArgumentError : public CommandError {
public:
    using CommandError::CommandError;
};

class YL_DISCORD_EXPORT BadArgument : public ArgumentError {
public:
    using ArgumentError::ArgumentError;
};

class YL_DISCORD_EXPORT MissingRequiredArgument : public ArgumentError {
public:
    explicit MissingRequiredArgument(std::size_t index)
        : ArgumentError("Missing argument " + std::to_string(index + 1)), index_(index) {}
    [[nodiscard]] std::size_t index() const noexcept { return index_; }
private:
    std::size_t index_;
};

class YL_DISCORD_EXPORT TooManyArguments : public ArgumentError {
public:
    TooManyArguments() : ArgumentError("Too many command arguments") {}
};

namespace detail {
template <typename T> struct optional_argument : std::false_type {};
template <typename T> struct optional_argument<std::optional<T>> : std::true_type { using type = T; };

template <typename T>
constexpr std::string_view argument_type_name() {
    if constexpr (optional_argument<T>::value) return argument_type_name<typename optional_argument<T>::type>();
    else if constexpr (std::same_as<T, std::string>) return "text";
    else if constexpr (std::same_as<T, bool>) return "bool";
    else if constexpr (std::integral<T>) return "integer";
    else if constexpr (std::floating_point<T>) return "number";
    else if constexpr (std::same_as<T, Snowflake>) return "id";
    else return "value";
}
} // namespace detail

/// @brief Convert one complete token. Specialize for an application-owned value type.
/// No whitespace, partial numeric parses, nonfinite numbers or signed unsigned IDs.
template <typename T>
[[nodiscard]] T convert_argument(std::string_view text) {
    static_assert(std::same_as<T, std::remove_cvref_t<T>>, "Command arguments must be owning value types");
    if constexpr (std::same_as<T, std::string>) {
        return std::string(text);
    } else if constexpr (std::same_as<T, bool>) {
        if (text == "true" || text == "yes" || text == "1" || text == "on") return true;
        if (text == "false" || text == "no" || text == "0" || text == "off") return false;
        throw BadArgument("Expected true/false, yes/no, on/off or 1/0");
    } else if constexpr (std::integral<T> || std::floating_point<T>) {
        if (text.empty()) throw BadArgument("Expected a number");
        T result{};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw BadArgument("Expected a complete in-range " + std::string(detail::argument_type_name<T>()));
        if constexpr (std::floating_point<T>) {
            if (!std::isfinite(result)) throw BadArgument("Expected a finite number");
        }
        return result;
    } else if constexpr (std::same_as<T, Snowflake>) {
        // IDs and ordinary user, role or channel mentions; never resolve names over HTTP.
        if (text.starts_with("<@") && text.ends_with('>')) {
            text.remove_prefix(2);
            text.remove_suffix(1);
            if (text.starts_with('!') || text.starts_with('&')) text.remove_prefix(1);
        } else if (text.starts_with("<#") && text.ends_with('>')) {
            text.remove_prefix(2);
            text.remove_suffix(1);
        }
        try { return Snowflake{text}; }
        catch (const std::invalid_argument&) { throw BadArgument("Expected a Discord ID or mention"); }
    } else {
        static_assert(!sizeof(T), "Provide a convert_argument<T>(string_view) specialization for this type");
    }
}

} // namespace yl::discord
