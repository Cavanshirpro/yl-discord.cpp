// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;
namespace {
void check(bool value, const char* expression) { if (!value) throw std::runtime_error(expression); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
template <typename Fn> void rejects(Fn function) {
    bool rejected = false; try { function(); } catch (const std::invalid_argument&) { rejected = true; } CHECK(rejected);
}
MessageOptions upload(FileUpload file) { MessageOptions result; result.files.push_back(std::move(file)); return result; }

void file_values_own_shared_binary_bytes() {
    FileUpload binary("data.bin", {0, 1, 255});
    auto copy = binary; binary.filename = "other.bin";
    CHECK(copy.filename == "data.bin" && copy.bytes()[0] == 0 && copy.bytes()[2] == 255);
    CHECK(copy.bytes().data() == binary.bytes().data());
    const std::string text{"a\0b", 3}; auto file = FileUpload::text("text.txt", text);
    CHECK(file.size() == 3 && file.bytes()[1] == 0 && file.content_type == "text/plain; charset=utf-8");
    FileUpload empty; empty.filename = "empty.bin"; validate_message_options(upload(empty)); CHECK(empty.bytes().empty());
}
void upload_size_and_count_limits() {
    FileUpload maximum("max.bin", std::vector<std::uint8_t>(FileUpload::max_file_bytes));
    validate_message_options(upload(maximum));
    rejects([] { (void)FileUpload("oversize.bin", std::vector<std::uint8_t>(FileUpload::max_file_bytes + 1)); });
    FileUpload shared("a.bin", std::vector<std::uint8_t>(12 * 1024 * 1024));
    auto options = upload(shared); shared.filename = "b.bin"; options.files.push_back(shared); validate_message_options(options);
    options.files.push_back(FileUpload::text("extra.txt", "x")); rejects([&] { validate_message_options(options); });
    options.files.clear();
    for (int i = 0; i < 10; ++i) options.files.push_back(FileUpload::text(std::to_string(i) + ".txt", ""));
    validate_message_options(options); options.files.push_back(FileUpload::text("extra.txt", "")); rejects([&] { validate_message_options(options); });
}
void filenames_mime_and_metadata_are_validated() {
    for (const auto& name : {std::string{}, std::string("."), std::string(".."), std::string("../x"), std::string("a\\b"),
         std::string("C:drive"), std::string("bad\r\nname"), std::string("bad\"quote"), std::string(256, 'a'), std::string("\xff"), std::string("a\0b", 3)}) {
        auto file = FileUpload::text(name, "x"); rejects([&] { validate_message_options(upload(file)); });
    }
    auto valid = FileUpload::text("notes-\xc3\xbc.txt", "x"); valid.description = std::string(1024, 'd');
    validate_message_options(upload(valid)); valid.description += 'd'; rejects([&] { validate_message_options(upload(valid)); });
    valid.description.clear();
    for (const auto& type : {"", "noslash", "/plain", "text/", "te xt/plain", "text/plain\r\nInjected: x", "text/plain\t"}) {
        valid.content_type = type; rejects([&] { validate_message_options(upload(valid)); });
    }
    valid.content_type = "text/plain; charset=utf-8"; validate_message_options(upload(valid));
    auto options = upload(valid); options.files.push_back(valid); rejects([&] { validate_message_options(options); });
}
void edits_require_explicit_retention() {
    MessageEditOptions edit; edit.files.push_back(FileUpload::text("new.txt", "new"));
    rejects([&] { validate_message_edit(edit); });
    edit.retained_attachment_ids = std::vector<Snowflake>{}; validate_message_edit(edit);
    edit.retained_attachment_ids = std::vector<Snowflake>{Snowflake{1}, Snowflake{1}}; rejects([&] { validate_message_edit(edit); });
    edit.retained_attachment_ids = std::vector<Snowflake>{Snowflake{0}}; rejects([&] { validate_message_edit(edit); });
    edit.retained_attachment_ids = std::vector<Snowflake>{};
    for (std::uint64_t i = 1; i <= 10; ++i) edit.retained_attachment_ids->push_back(Snowflake{i});
    rejects([&] { validate_message_edit(edit); });
    edit.files.clear(); validate_message_edit(edit);
}

task<> attachment_lifecycle(Client& client, std::shared_ptr<testing::MemoryTransport> transport) {
    MessageOptions options; options.files.push_back(FileUpload::text("old.txt", "old"));
    auto task = client.send_message(Snowflake{42}, options); options.files[0].filename = "changed.txt";
    auto sent = co_await std::move(task);
    CHECK(sent.attachments.size() == 1 && sent.attachments[0].filename == "old.txt" && sent.attachments[0].size == 3);
    CHECK(transport->sent().back().files[0].filename == "old.txt");
    auto edited = co_await sent.edit("content"); CHECK(edited.attachments[0].id == sent.attachments[0].id);
    MessageEditOptions append; append.retained_attachment_ids = std::vector<Snowflake>{sent.attachments[0].id};
    append.files.push_back(FileUpload::text("new.txt", "new"));
    auto appended = co_await edited.edit(append); CHECK(appended.attachments.size() == 2 && sent.attachments.size() == 1);
    MessageEditOptions unknown; unknown.retained_attachment_ids = std::vector<Snowflake>{Snowflake{999}};
    bool missing = false;
    try { (void)co_await appended.edit(unknown); } catch (const std::out_of_range&) { missing = true; }
    CHECK(missing && (co_await appended.fetch()).attachments.size() == 2);
    MessageEditOptions clear; clear.retained_attachment_ids = std::vector<Snowflake>{};
    auto cleared = co_await appended.edit(clear); CHECK(cleared.attachments.empty() && cleared.content == "content");
    CHECK(transport->actions().back().edit_options->retained_attachment_ids->empty());
    auto response = co_await cleared.reply(upload(FileUpload::text("reply.txt", "reply")));
    CHECK(response.attachments[0].filename == "reply.txt" && transport->sent().back().reply_to == cleared.id);
}
void memory_records_upload_and_retention_intent() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    client.spawn(attachment_lifecycle(client, transport)); client.run();
}
class LegacyTransport : public MessageTransport {
public:
    int calls = 0;
    task<Message> send_message(MessageRequest) override { co_return Message{}; }
    task<Message> edit_message(Snowflake, Snowflake, std::string) override { ++calls; co_return Message{}; }
};
task<> unsupported_edit(Client& client, std::shared_ptr<LegacyTransport> transport) {
    MessageEditOptions edit; edit.retained_attachment_ids = std::vector<Snowflake>{};
    bool rejected = false;
    try { (void)co_await client.edit_message(Snowflake{42}, Snowflake{7}, edit); }
    catch (const std::logic_error&) { rejected = true; }
    CHECK(rejected && transport->calls == 0);
}
void older_transports_do_not_drop_attachment_edits() {
    auto transport = std::make_shared<LegacyTransport>(); Client client{transport};
    client.spawn(unsupported_edit(client, transport)); client.run();
}
} // namespace
int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"owned shared binary files", file_values_own_shared_binary_bytes}, {"upload count and byte bounds", upload_size_and_count_limits},
        {"header and metadata validation", filenames_mime_and_metadata_are_validated}, {"explicit retained attachment IDs", edits_require_explicit_retention},
        {"offline attachment lifecycle", memory_records_upload_and_retention_intent}, {"older edit transport", older_transports_do_not_drop_attachment_edits}
    };
    int failures = 0;
    for (const auto& [name, test] : cases) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
