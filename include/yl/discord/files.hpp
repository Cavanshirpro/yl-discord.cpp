// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <yl/discord/export.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yl::discord {
/// @brief Immutable shared upload bytes with owned, per-request metadata.
/// No filesystem access or network download is performed by this value type.
class YL_DISCORD_EXPORT FileUpload {
public:
    static constexpr std::size_t max_file_bytes = 20 * 1024 * 1024;
    static constexpr std::size_t max_total_bytes = 24 * 1024 * 1024;
    FileUpload() = default;
    FileUpload(std::string filename, std::vector<std::uint8_t> bytes);
    [[nodiscard]] static FileUpload text(std::string filename, std::string_view text);
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return bytes().size(); }

    std::string filename;
    std::string description;
    std::string content_type = "application/octet-stream";
private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
};
} // namespace yl::discord
