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
MessageOptions embedded(Embed embed) { MessageOptions result; result.embeds.push_back(std::move(embed)); return result; }

void content_and_unicode_boundaries() {
    MessageOptions options; options.content = std::string(2000, 'a'); validate_message_options(options);
    options.content += 'a'; rejects([&] { validate_message_options(options); });
    options.content.clear();
    for (int i = 0; i < 1000; ++i) options.content += "\xf0\x9f\x98\x80";
    validate_message_options(options); options.content += "\xf0\x9f\x98\x80";
    rejects([&] { validate_message_options(options); });
    for (auto text : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80", "\xe2x"}) {
        options.content = text; rejects([&] { validate_message_options(options); });
    }
    options.content = "line one\nline two"; validate_message_options(options);
    rejects([] { validate_message_options(MessageOptions{}); });
}

void embed_limits_and_structure() {
    Embed embed; embed.title = std::string(256, 't'); validate_message_options(embedded(embed));
    embed.title += 't'; rejects([&] { validate_message_options(embedded(embed)); });
    embed = {}; embed.description = std::string(4096, 'd'); validate_message_options(embedded(embed));
    embed.description += 'd'; rejects([&] { validate_message_options(embedded(embed)); });
    embed = {}; embed.title = "fields"; embed.fields.assign(25, EmbedField{"name", "value", true}); validate_message_options(embedded(embed));
    embed.fields.push_back({"extra", "field", false}); rejects([&] { validate_message_options(embedded(embed)); });
    embed.fields = {{std::string(256, 'n'), std::string(1024, 'v'), false}}; validate_message_options(embedded(embed));
    embed.fields[0].value += 'v'; rejects([&] { validate_message_options(embedded(embed)); });
    embed.fields = {{"", "value", false}}; rejects([&] { validate_message_options(embedded(embed)); });
    embed.fields.clear(); embed.footer = EmbedFooter{std::string(2048, 'f'), {}}; validate_message_options(embedded(embed));
    embed.footer->text += 'f'; rejects([&] { validate_message_options(embedded(embed)); });
    embed.footer.reset(); embed.author = EmbedAuthor{std::string(256, 'n'), {}, {}}; validate_message_options(embedded(embed));
    embed.author->name += 'n'; rejects([&] { validate_message_options(embedded(embed)); });
    embed.author.reset(); embed.color = 0; validate_message_options(embedded(embed));
    embed.color = 0x1000000; rejects([&] { validate_message_options(embedded(embed)); });
    embed = {}; rejects([&] { validate_message_options(embedded(embed)); });
}

void aggregate_limits_and_urls() {
    MessageOptions options;
    Embed embed; embed.description = std::string(3000, 'd'); options.embeds.assign(2, embed); validate_message_options(options);
    options.embeds.push_back(Embed{.title = "x"}); rejects([&] { validate_message_options(options); });
    options.embeds.assign(10, Embed{.title = "x"}); validate_message_options(options);
    options.embeds.push_back(Embed{.title = "x"}); rejects([&] { validate_message_options(options); });
    embed = {}; embed.title = "URL"; embed.url = "https://example.invalid/";
    embed.image_url = "attachment://image.png"; validate_message_options(embedded(embed));
    embed.url = "javascript:alert(1)"; rejects([&] { validate_message_options(embedded(embed)); });
    embed.url = "https://example.invalid/bad path"; rejects([&] { validate_message_options(embedded(embed)); });
    embed.url.clear(); embed.image_url = "https://example.invalid/\r\nheader"; rejects([&] { validate_message_options(embedded(embed)); });
}

void mention_policies_are_explicit_and_validated() {
    MessageOptions options; options.content = "mentions"; validate_message_options(options);
    CHECK(options.allowed_mentions == AllowedMentions{});
    options.allowed_mentions = AllowedMentions::all(); validate_message_options(options);
    options.allowed_mentions.users.push_back(Snowflake{1}); rejects([&] { validate_message_options(options); });
    options.allowed_mentions = {};
    for (std::uint64_t i = 1; i <= 100; ++i) options.allowed_mentions.users.push_back(Snowflake{i});
    validate_message_options(options); options.allowed_mentions.users.push_back(Snowflake{101}); rejects([&] { validate_message_options(options); });
    options.allowed_mentions.users = {Snowflake{1}, Snowflake{1}}; rejects([&] { validate_message_options(options); });
    options.allowed_mentions.users = {Snowflake{0}}; rejects([&] { validate_message_options(options); });
    options.allowed_mentions = {}; options.allowed_mentions.all_users = true; options.allowed_mentions.roles = {Snowflake{7}};
    validate_message_options(options);
    options.allowed_mentions.all_roles = true; rejects([&] { validate_message_options(options); });
}

task<> rich_message_lifecycle(Client& client, std::shared_ptr<testing::MemoryTransport> transport) {
    MessageOptions options; options.content = "original"; options.embeds = {Embed{.title = "before"}};
    options.allowed_mentions.users = {Snowflake{7}};
    auto pending = client.send_message(Snowflake{42}, options); options.embeds[0].title = "after";
    auto sent = co_await std::move(pending); CHECK(sent.embeds[0].title == "before");
    CHECK(transport->sent().back().allowed_mentions.users == std::vector<Snowflake>{Snowflake{7}});
    auto edited = co_await sent.edit("changed"); CHECK(edited.embeds[0].title == "before" && sent.content == "original");
    MessageEditOptions clear; clear.embeds = std::vector<Embed>{};
    auto cleared = co_await edited.edit(clear); CHECK(cleared.embeds.empty() && cleared.content == "changed");
    CHECK(transport->actions().back().edit_options->embeds->empty() && !transport->actions().back().edit_options->content);
    MessageEditOptions replace; replace.content = ""; replace.embeds = std::vector<Embed>{Embed{.description = "embed only"}};
    auto replaced = co_await client.edit_message(Snowflake{42}, sent.id, replace); CHECK(replaced.content.empty() && replaced.embeds.size() == 1);
    MessageOptions reply; reply.embeds = {Embed{.title = "reply"}};
    auto response = co_await replaced.reply(reply); CHECK(response.content.empty() && response.embeds[0].title == "reply");
    CHECK(transport->sent().back().reply_to == sent.id);
}
void rich_snapshots_and_edit_intent() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    client.spawn(rich_message_lifecycle(client, transport)); client.run();
    MessageEditOptions missing; rejects([&] { validate_message_edit(missing); });
    MessageEditOptions clear; clear.content = ""; validate_message_edit(clear);
    const auto sent = transport->sent().size();
    client.spawn(client.send_message(Snowflake{42}, MessageOptions{}));
    rejects([&] { client.run(); }); CHECK(transport->sent().size() == sent);
}

class LegacyEditingTransport : public MessageTransport {
public:
    int edits = 0;
    task<Message> send_message(MessageRequest) override { co_return Message{}; }
    task<Message> edit_message(Snowflake channel, Snowflake id, std::string content) override {
        ++edits; Message result; result.id = id; result.channel_id = channel; result.content = std::move(content); co_return result;
    }
};
task<> legacy_edit(Client& client, std::shared_ptr<LegacyEditingTransport> transport) {
    auto message = co_await client.edit_message(Snowflake{42}, Snowflake{7}, "old overload"); CHECK(message.content == "old overload");
    MessageEditOptions rich; rich.embeds = std::vector<Embed>{Embed{.title = "unsupported"}};
    bool unsupported = false;
    try { (void)co_await message.edit(rich); } catch (const std::logic_error&) { unsupported = true; }
    CHECK(unsupported && transport->edits == 1);
}
void legacy_edit_overrides_keep_working() {
    auto transport = std::make_shared<LegacyEditingTransport>(); Client client{transport};
    client.spawn(legacy_edit(client, transport)); client.run();
}
} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"Unicode content bounds", content_and_unicode_boundaries}, {"embed schema limits", embed_limits_and_structure},
        {"aggregate and URL limits", aggregate_limits_and_urls}, {"mention policy", mention_policies_are_explicit_and_validated},
        {"rich snapshots and edit intent", rich_snapshots_and_edit_intent}, {"legacy edit compatibility", legacy_edit_overrides_keep_working}
    };
    int failures = 0;
    for (const auto& [name, test] : cases) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
