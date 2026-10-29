// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;
int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bot.command("report", [](Context context) -> task<> {
        MessageOptions report;
        report.files.push_back(FileUpload::text("report.txt", "Status: ready\n"));
        auto sent = co_await context.reply(report);
        MessageEditOptions append;
        append.retained_attachment_ids = std::vector<Snowflake>{};
        for (const auto& attachment : sent.attachments) append.retained_attachment_ids->push_back(attachment.id);
        append.files.push_back(FileUpload::text("extra.txt", "Additional details\n"));
        auto updated = co_await sent.edit(append);
        co_await context.send("Report has " + std::to_string(updated.attachments.size()) + " attachments.");
    });
    Message message; message.id = Snowflake{100}; message.channel_id = Snowflake{42}; message.content = "!report";
    bot.dispatch(std::move(message)); bot.run();
    for (const auto& request : transport->sent()) {
        if (!request.content.empty()) std::cout << request.content << '\n';
        for (const auto& file : request.files) std::cout << file.filename << ": " << file.size() << " bytes\n";
    }
}
