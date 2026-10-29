// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "upload_internal.hpp"
#include <yl/discord/network.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace yl::discord::detail {
namespace {
void checked(CURLcode code) {
    if (code != CURLE_OK) throw TransportError("Could not configure multipart upload");
}
curl_mimepart* part(curl_mime* form) {
    auto* value = curl_mime_addpart(form);
    if (!value) throw std::bad_alloc();
    return value;
}
} // namespace

MultipartForm::MultipartForm(CURL* easy, const std::string& json, const std::vector<FileUpload>& files)
    : mime_(curl_mime_init(easy)) {
    if (!mime_) throw std::bad_alloc();
    constexpr std::size_t request_limit = 25 * 1024 * 1024;
    constexpr std::size_t framing_budget = 64 * 1024;
    std::size_t bytes = json.size();
    if (bytes > request_limit - framing_budget) throw std::invalid_argument("Multipart request exceeds the 25 MiB request budget");
    for (const auto& file : files) {
        if (file.size() > request_limit - framing_budget - bytes)
            throw std::invalid_argument("Multipart request exceeds the 25 MiB request budget");
        bytes += file.size();
    }
    parts_.reserve(files.size());
    auto* payload = part(mime_.get());
    checked(curl_mime_name(payload, "payload_json"));
    checked(curl_mime_type(payload, "application/json"));
    checked(curl_mime_data(payload, json.data(), json.size()));
    for (std::size_t index = 0; index < files.size(); ++index) {
        parts_.push_back(Part{files[index], 0});
        auto& source = parts_.back();
        auto* file = part(mime_.get());
        const auto name = "files[" + std::to_string(index) + "]";
        checked(curl_mime_name(file, name.c_str()));
        checked(curl_mime_filename(file, source.file.filename.c_str()));
        checked(curl_mime_type(file, source.file.content_type.c_str()));
        checked(curl_mime_data_cb(file, static_cast<curl_off_t>(source.file.size()), read, seek, nullptr, &source));
    }
}

std::size_t MultipartForm::read(char* output, std::size_t size, std::size_t count, void* data) noexcept {
    auto& source = *static_cast<Part*>(data);
    if (count != 0 && size > (std::numeric_limits<std::size_t>::max)() / count) return CURL_READFUNC_ABORT;
    const auto bytes = source.file.bytes();
    const auto available = bytes.size() - source.offset;
    const auto copied = (std::min)(available, size * count);
    if (copied != 0) std::memcpy(output, bytes.data() + source.offset, copied);
    source.offset += copied;
    return copied;
}
int MultipartForm::seek(void* data, curl_off_t offset, int origin) noexcept {
    auto& source = *static_cast<Part*>(data);
    const auto total = static_cast<curl_off_t>(source.file.size());
    curl_off_t base = 0;
    if (origin == SEEK_CUR) base = static_cast<curl_off_t>(source.offset);
    else if (origin == SEEK_END) base = total;
    else if (origin != SEEK_SET) return CURL_SEEKFUNC_CANTSEEK;
    if (offset < -base || offset > total - base) return CURL_SEEKFUNC_FAIL;
    source.offset = static_cast<std::size_t>(base + offset);
    return CURL_SEEKFUNC_OK;
}
} // namespace yl::discord::detail
