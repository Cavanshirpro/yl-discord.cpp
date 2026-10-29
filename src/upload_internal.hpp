// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <yl/discord/files.hpp>
#include <curl/curl.h>

namespace yl::discord::detail {
class MultipartForm {
public:
    MultipartForm(CURL* easy, const std::string& json, const std::vector<FileUpload>& files);
    [[nodiscard]] curl_mime* get() const noexcept { return mime_.get(); }
    MultipartForm(const MultipartForm&) = delete;
    MultipartForm& operator=(const MultipartForm&) = delete;
private:
    struct Part { FileUpload file; std::size_t offset = 0; };
    struct Deleter { void operator()(curl_mime* value) const noexcept { curl_mime_free(value); } };
    std::vector<Part> parts_;
    // Declared last so mime/free callbacks are gone before their userdata storage.
    std::unique_ptr<curl_mime, Deleter> mime_;
    static std::size_t read(char*, std::size_t, std::size_t, void*) noexcept;
    static int seek(void*, curl_off_t, int) noexcept;
};
} // namespace yl::discord::detail
