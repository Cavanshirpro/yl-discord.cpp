// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "network_internal.hpp"

namespace yl::discord::detail {
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
    Json body{{"content", request.content}, {"allowed_mentions", {{"parse", Json::array()}, {"replied_user", false}}}};
    if (request.reply_to) body["message_reference"] = {{"message_id", request.reply_to->str()}, {"channel_id", request.channel_id.str()}};
    return body.dump();
}
std::string edit_message_payload(const std::string& content) {
    // Editing must repeat the mention policy; Discord does not inherit it from creation.
    return Json{{"content", content}, {"allowed_mentions", {{"parse", Json::array()}, {"replied_user", false}}}}.dump();
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
