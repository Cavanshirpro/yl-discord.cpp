// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <yl/discord/models.hpp>
namespace yl::discord::detail {
void validate_message_parts(std::string_view content, const std::vector<Embed>& embeds, const AllowedMentions& mentions);
}
