// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/files.hpp>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <unordered_set>
#include "file_validation.hpp"
#include "text_internal.hpp"

namespace yl::discord {
FileUpload::FileUpload(std::string name, std::vector<std::uint8_t> bytes) : filename(std::move(name)) {
    if (bytes.size() > max_file_bytes) throw std::invalid_argument("File exceeds the 20 MiB upload limit");
    if (!bytes.empty()) bytes_ = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
}
FileUpload FileUpload::text(std::string name, std::string_view text) {
    if (text.size() > max_file_bytes) throw std::invalid_argument("File exceeds the 20 MiB upload limit");
    FileUpload result(std::move(name), std::vector<std::uint8_t>(text.begin(), text.end()));
    result.content_type = "text/plain; charset=utf-8";
    return result;
}
std::span<const std::uint8_t> FileUpload::bytes() const noexcept {
    return bytes_ ? std::span<const std::uint8_t>(*bytes_) : std::span<const std::uint8_t>{};
}
void detail::validate_upload_files(const std::vector<FileUpload>& files, std::size_t retained) {
    if (retained > 10 || files.size() > 10 - retained) throw std::invalid_argument("A message supports at most 10 attachments");
    std::size_t total = 0;
    std::unordered_set<std::string> names;
    for (const auto& file : files) {
        if (file.filename.empty() || file.filename.size() > 255 || file.filename == "." || file.filename == ".." ||
            file.filename.find_first_of("/\\:\"") != std::string::npos ||
            std::any_of(file.filename.begin(), file.filename.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::invalid_argument("Upload filename must be a nonempty basename of at most 255 UTF-8 bytes");
        (void)text_units(file.filename, 255);
        if (!names.insert(file.filename).second) throw std::invalid_argument("Upload filenames must be unique within one request");
        (void)text_units(file.description, 1024);
        const auto separator = file.content_type.find(';');
        const auto media = std::string_view(file.content_type).substr(0, separator);
        const auto slash = media.find('/');
        const auto token = [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos;
        };
        if (file.content_type.empty() || file.content_type.size() > 128 || slash == std::string_view::npos || slash == 0 || slash + 1 == media.size() ||
            !std::all_of(media.begin(), media.begin() + static_cast<std::ptrdiff_t>(slash), token) ||
            !std::all_of(media.begin() + static_cast<std::ptrdiff_t>(slash + 1), media.end(), token) ||
            std::any_of(file.content_type.begin(), file.content_type.end(), [](unsigned char c) { return c < 32 || c > 126; }))
            throw std::invalid_argument("Upload content type must be a printable ASCII MIME type of at most 128 bytes");
        if (file.size() > FileUpload::max_file_bytes || file.size() > FileUpload::max_total_bytes - total)
            throw std::invalid_argument("Upload exceeds the per-file or combined byte limit");
        total += file.size();
    }
}
} // namespace yl::discord
