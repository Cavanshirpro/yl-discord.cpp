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
} // namespace yl::discord::detail
