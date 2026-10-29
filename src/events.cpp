// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/events.hpp>

namespace yl::discord {
void ListenerConnection::disconnect() noexcept {
    if (auto control = control_.lock()) control->disconnect();
}
bool ListenerConnection::connected() const noexcept {
    auto control = control_.lock();
    return control && control->connected();
}
} // namespace yl::discord
