// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string_view>
namespace yl::discord::detail {
inline std::size_t text_units(std::string_view text, std::size_t limit) {
    std::size_t total = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        std::uint32_t code = first;
        unsigned remaining = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; remaining = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; remaining = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; remaining = 3; minimum = 0x10000; }
        else if (first > 0x7f) throw std::invalid_argument("Message text must be valid UTF-8");
        if (remaining > text.size() - i) throw std::invalid_argument("Truncated UTF-8 in message text");
        while (remaining != 0) {
            --remaining;
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation in message text");
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            throw std::invalid_argument("Invalid Unicode scalar in message text");
        total += code > 0xffff ? 2 : 1;
        if (total > limit) throw std::invalid_argument("Message or embed text exceeds its character limit");
    }
    return total;
}
} // namespace yl::discord::detail
