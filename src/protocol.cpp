// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "network_internal.hpp"

namespace yl::discord::detail {
namespace {
Json mention_payload(const AllowedMentions& mentions) {
    Json parse = Json::array();
    if (mentions.everyone) parse.push_back("everyone");
    if (mentions.all_users) parse.push_back("users");
    if (mentions.all_roles) parse.push_back("roles");
    Json result{{"parse", parse}, {"replied_user", mentions.replied_user}};
    if (!mentions.users.empty()) {
        result["users"] = Json::array();
        for (auto id : mentions.users) result["users"].push_back(id.str());
    }
    if (!mentions.roles.empty()) {
        result["roles"] = Json::array();
        for (auto id : mentions.roles) result["roles"].push_back(id.str());
    }
    return result;
}
Json embed_payload(const Embed& embed) {
    Json result = Json::object();
    for (const auto& [key, value] : {std::pair{"title", &embed.title}, {"description", &embed.description},
                                   {"url", &embed.url}, {"timestamp", &embed.timestamp}})
        if (!value->empty()) result[key] = *value;
    if (embed.color) result["color"] = *embed.color;
    if (!embed.fields.empty()) {
        result["fields"] = Json::array();
        for (const auto& field : embed.fields) result["fields"].push_back({{"name", field.name}, {"value", field.value}, {"inline", field.inline_field}});
    }
    if (embed.footer) {
        result["footer"] = {{"text", embed.footer->text}};
        if (!embed.footer->icon_url.empty()) result["footer"]["icon_url"] = embed.footer->icon_url;
    }
    if (embed.author) {
        result["author"] = {{"name", embed.author->name}};
        if (!embed.author->url.empty()) result["author"]["url"] = embed.author->url;
        if (!embed.author->icon_url.empty()) result["author"]["icon_url"] = embed.author->icon_url;
    }
    if (!embed.image_url.empty()) result["image"] = {{"url", embed.image_url}};
    if (!embed.thumbnail_url.empty()) result["thumbnail"] = {{"url", embed.thumbnail_url}};
    return result;
}
Json embeds_payload(const std::vector<Embed>& embeds) {
    auto result = Json::array();
    for (const auto& embed : embeds) result.push_back(embed_payload(embed));
    return result;
}
} // namespace
// Keep JSON expression templates out of coroutine state-machine construction.
std::string heartbeat_payload(std::optional<std::uint64_t> sequence) {
    return Json{{"op", 1}, {"d", sequence ? Json(*sequence) : Json(nullptr)}}.dump();
}
std::string identify_payload(const std::string& token, Intents intents) {
#if defined(_WIN32)
    constexpr auto os = "windows";
#elif defined(__APPLE__)
    constexpr auto os = "darwin";
#else
    constexpr auto os = "linux";
#endif
    return Json{{"op", 2}, {"d", {{"token", token}, {"intents", intents.value()},
        {"properties", {{"os", os}, {"browser", "yl-discord.cpp"}, {"device", "yl-discord.cpp"}}}}}}.dump();
}
std::string resume_payload(const std::string& token, const std::string& session, std::uint64_t sequence) {
    return Json{{"op", 6}, {"d", {{"token", token}, {"session_id", session}, {"seq", sequence}}}}.dump();
}
std::string message_payload(const MessageRequest& request) {
    Json body{{"content", request.content}, {"allowed_mentions", mention_payload(request.allowed_mentions)}};
    if (!request.embeds.empty()) body["embeds"] = embeds_payload(request.embeds);
    if (request.reply_to) body["message_reference"] = {{"message_id", request.reply_to->str()}, {"channel_id", request.channel_id.str()}};
    return body.dump();
}
std::string edit_message_payload(const MessageEditOptions& options) {
    // Editing must repeat the mention policy; Discord does not inherit it from creation.
    Json body{{"allowed_mentions", mention_payload(options.allowed_mentions)}};
    if (options.content) body["content"] = *options.content;
    if (options.embeds) body["embeds"] = embeds_payload(*options.embeds);
    return body.dump();
}
std::vector<Embed> parse_embeds(const Json& data) {
    if (!data.is_array()) throw ProtocolError("Message embeds must be an array");
    std::vector<Embed> result;
    for (const auto& value : data) {
        Embed embed;
        embed.title = value.value("title", std::string{}); embed.description = value.value("description", std::string{});
        embed.url = value.value("url", std::string{}); embed.timestamp = value.value("timestamp", std::string{});
        embed.type = value.value("type", std::string{"rich"});
        if (value.contains("color")) {
            const auto& color = value.at("color");
            if (!color.is_number_integer() || color < 0 || color > 0xffffff) throw ProtocolError("Invalid embed color");
            embed.color = color.get<std::uint32_t>();
        }
        if (value.contains("fields")) {
            if (!value.at("fields").is_array()) throw ProtocolError("Embed fields must be an array");
            for (const auto& field : value.at("fields")) embed.fields.push_back({field.at("name").get<std::string>(), field.at("value").get<std::string>(), field.value("inline", false)});
        }
        if (value.contains("footer")) embed.footer = EmbedFooter{value.at("footer").at("text").get<std::string>(), value.at("footer").value("icon_url", std::string{})};
        if (value.contains("author")) embed.author = EmbedAuthor{value.at("author").at("name").get<std::string>(), value.at("author").value("url", std::string{}), value.at("author").value("icon_url", std::string{})};
        if (value.contains("image")) embed.image_url = value.at("image").value("url", std::string{});
        if (value.contains("thumbnail")) embed.thumbnail_url = value.at("thumbnail").value("url", std::string{});
        result.push_back(std::move(embed));
    }
    return result;
}
Message parse_message_response(const std::string& body) {
    const auto data = Json::parse(body, nullptr, false);
    if (!data.is_object()) throw ProtocolError("Invalid message response");
    try { return parse_message(data); }
    catch (const Json::exception&) { throw ProtocolError("Invalid message response fields"); }
    catch (const std::invalid_argument&) { throw ProtocolError("Invalid message response ID"); }
}
std::string reaction_path(const std::string& value) {
    std::string_view emoji = value;
    if ((emoji.starts_with("<:") || emoji.starts_with("<a:")) && emoji.ends_with('>')) {
        emoji.remove_prefix(emoji.starts_with("<a:") ? 3 : 2);
        emoji.remove_suffix(1);
    }
    if (emoji.empty() || emoji.size() > 256) throw std::invalid_argument("Emoji must contain 1 to 256 UTF-8 bytes");
    if (emoji == "." || emoji == "..") throw std::invalid_argument("Emoji cannot be a URL path segment marker");
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char byte : emoji) {
        if (byte <= 32 || byte == 127) throw std::invalid_argument("Emoji cannot contain whitespace or controls");
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~') encoded += static_cast<char>(byte);
        else { encoded += '%'; encoded += hex[byte >> 4]; encoded += hex[byte & 15]; }
    }
    return encoded;
}
} // namespace yl::discord::detail
