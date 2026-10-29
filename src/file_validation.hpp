// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <yl/discord/files.hpp>
namespace yl::discord::detail {
void validate_upload_files(const std::vector<FileUpload>& files, std::size_t retained = 0);
}
