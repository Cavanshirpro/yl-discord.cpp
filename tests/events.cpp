// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <asio/co_spawn.hpp>
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <iostream>
#include <thread>

using namespace yl::discord;
using namespace std::chrono_literals;

namespace {
void check(bool result, const char* expression) { if (!result) throw std::runtime_error(expression); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
Message incoming(std::string content) {
    Message message; message.id = Snowflake{100}; message.channel_id = Snowflake{42};
    message.author = User{Snowflake{7}, "tester", false}; message.content = std::move(content); return message;
}
task<> emit_messages(Client& client) {
    co_await sleep_for(1ms);
    client.dispatch(incoming("ignore")); client.dispatch(incoming("match"));
}
task<> emit_typed(Client& client) {
    co_await sleep_for(1ms);
    client.dispatch(Ready{User{Snowflake{1}, "bot", true}});
    client.dispatch(RawEvent{"CUSTOM", "{}", 7});
}
task<> stop_later(Client& client) { co_await sleep_for(1ms); client.stop(); }
task<> close_later(Client& client) { co_await sleep_for(2ms); client.close(); }

void listener_handles_preserve_queued_callbacks() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Client client{transport};
    int first = 0, second = 0;
    auto marker = std::make_shared<int>(42); std::weak_ptr<int> weak = marker;
    ListenerConnection first_connection, second_connection;
    first_connection = client.subscribe_message([&, marker](Message) -> task<> {
        ++first; first_connection.disconnect(); second_connection.disconnect();
        co_await sleep_for(1ms); CHECK(*marker == 42);
    });
    second_connection = client.subscribe_message([&](Message) -> task<> { ++second; co_return; });
    marker.reset();
    CHECK(first_connection && second_connection);
    client.dispatch(incoming("first")); client.run();
    CHECK(first == 1 && second == 1 && !first_connection && !second_connection && weak.expired());
    client.dispatch(incoming("second")); client.run(); CHECK(first == 1 && second == 1);
    first_connection.disconnect();
    bool invalid = false;
    try { (void)client.subscribe_message({}); } catch (const std::invalid_argument&) { invalid = true; }
    CHECK(invalid);
}

void dropped_and_stale_handles_are_safe() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    ListenerConnection stale;
    int calls = 0;
    {
        Client client{transport};
        { auto handle = client.subscribe_message([&](Message) -> task<> { ++calls; co_return; }); }
        stale = client.subscribe_ready([](Ready) -> task<> { co_return; });
        client.dispatch(incoming("still subscribed")); client.run(); CHECK(calls == 1 && stale.connected());
    }
    CHECK(!stale.connected()); stale.disconnect();
}

task<> await_match(Client& client, int& matched) {
    auto message = co_await client.wait_for<Message>(1s, [](const Message& value) { return value.content == "match"; });
    CHECK(message.author.id == Snowflake{7});
    ++matched; co_await message.reply("received");
}
void waits_filter_broadcast_and_preserve_binding() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int matched = 0, observed = 0;
    client.listen_message([&](Message) -> task<> { ++observed; co_return; });
    for (int i = 0; i < 3; ++i) client.spawn(await_match(client, matched));
    client.spawn(emit_messages(client)); client.run();
    CHECK(matched == 3 && observed == 2 && transport->sent().size() == 3);
}

task<> await_ready(Client& client, int& count) {
    auto ready = co_await client.wait_for<Ready>(1s); CHECK(ready.user.bot); ++count;
}
task<> await_raw(Client& client, int& count) {
    auto raw = co_await client.wait_for<RawEvent>(1s, [](const RawEvent& event) { return event.name == "CUSTOM"; });
    CHECK(raw.sequence == 7); ++count;
}
void typed_events_support_waits_and_subscriptions() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int waited = 0, listened = 0;
    auto ready = client.subscribe_ready([&](Ready) -> task<> { ++listened; co_return; });
    auto raw = client.subscribe_raw([&](RawEvent) -> task<> { ++listened; co_return; });
    client.spawn(await_ready(client, waited)); client.spawn(await_raw(client, waited)); client.spawn(emit_typed(client)); client.run();
    CHECK(waited == 2 && listened == 2);
    ready.disconnect(); raw.disconnect(); client.spawn(emit_typed(client)); client.run(); CHECK(listened == 2);
}

task<> expect_timeout(Client& client, int& count, std::function<bool(const Message&)> predicate = {}) {
    bool expired = false;
    try { (void)co_await client.wait_for<Message>(2ms, std::move(predicate)); }
    catch (const EventTimeout&) { expired = true; }
    CHECK(expired); ++count;
}
void deadlines_release_predicates() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int count = 0;
    auto marker = std::make_shared<int>(1); std::weak_ptr<int> weak = marker;
    client.spawn(expect_timeout(client, count, [marker](const Message&) { return *marker == 0; })); marker.reset();
    client.run(); CHECK(count == 1 && weak.expired());
    client.dispatch(incoming("after timeout")); client.run(); CHECK(count == 1);
}

task<> await_cancel(Client& client, std::stop_token token, int& count) {
    bool cancelled = false;
    try { (void)co_await client.wait_for<Message>(60s, {}, token); }
    catch (const asio::system_error& error) { cancelled = error.code() == asio::error::operation_aborted; }
    CHECK(cancelled); ++count;
}
void stop_tokens_cancel_before_and_during_waits() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int count = 0; std::stop_source pre; pre.request_stop();
    client.spawn(await_cancel(client, pre.get_token(), count)); client.run(); CHECK(count == 1);
    std::stop_source source;
    client.spawn(await_cancel(client, source.get_token(), count));
    std::thread worker([source] { std::this_thread::sleep_for(10ms); source.request_stop(); });
    client.run(); worker.join(); CHECK(count == 2);
}

template <typename Event> task<> await_close(Client& client, int& count) {
    bool cancelled = false;
    try { (void)co_await client.wait_for<Event>(60s); }
    catch (const asio::system_error& error) { cancelled = error.code() == asio::error::operation_aborted; }
    CHECK(cancelled); ++count;
}
void close_cancels_all_wait_types() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int count = 0;
    client.spawn(await_close<Message>(client, count)); client.spawn(await_close<Ready>(client, count));
    client.spawn(await_close<RawEvent>(client, count)); client.spawn(close_later(client)); client.run(); CHECK(count == 3);
    client.spawn(await_close<Message>(client, count)); client.run(); CHECK(count == 4);
}

task<> failing_predicate(Client& client, int& count) {
    bool failed = false;
    try { (void)co_await client.wait_for<Message>(1s, [](const Message&) -> bool { throw std::runtime_error("predicate failure"); }); }
    catch (const std::runtime_error& error) { failed = std::string(error.what()) == "predicate failure"; }
    CHECK(failed); ++count;
}
void predicate_failures_are_isolated() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int failed = 0, matched = 0;
    client.spawn(failing_predicate(client, failed)); client.spawn(await_match(client, matched));
    client.spawn(emit_messages(client)); client.run(); CHECK(failed == 1 && matched == 1 && client.take_errors().empty());
}

task<> stop_during_predicate(Client& client, std::stop_source& source, bool& matched) {
    auto result = co_await client.wait_for<Message>(1s, [&](const Message& message) {
        if (message.content != "match") return false;
        source.request_stop(); return true;
    }, source.get_token());
    matched = result.content == "match";
}
void first_processed_completion_wins() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    std::stop_source source; bool matched = false;
    client.spawn(stop_during_predicate(client, source, matched)); client.spawn(emit_messages(client)); client.run(); CHECK(matched);
}

void paused_destruction_releases_pending_waits() {
    std::weak_ptr<int> weak;
    {
        auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
        auto marker = std::make_shared<int>(42); weak = marker;
        client.spawn(client.wait_for<Message>(60s, [marker](const Message&) { return *marker == 0; }));
        marker.reset(); client.spawn(stop_later(client)); client.run(); CHECK(!weak.expired());
    }
    CHECK(weak.expired());
}

task<> invalid_timeout(Client& client, std::chrono::steady_clock::duration duration, int& count) {
    bool rejected = false;
    try { (void)co_await client.wait_for<Message>(duration); }
    catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected); ++count;
}
void invalid_deadlines_and_foreign_executors_are_rejected() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int count = 0; client.spawn(invalid_timeout(client, 0s, count)); client.spawn(invalid_timeout(client, 25h, count));
    client.run(); CHECK(count == 2);
    asio::io_context other; bool rejected = false;
    asio::co_spawn(other, client.wait_for<Message>(1s), [&](std::exception_ptr error, Message) {
        CHECK(error != nullptr);
        try { std::rethrow_exception(error); } catch (const std::logic_error&) { rejected = true; }
    });
    other.run(); CHECK(rejected);
}

void unstarted_waits_do_not_register() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    auto marker = std::make_shared<int>(42); std::weak_ptr<int> weak = marker;
    { auto dropped = client.wait_for<Message>(60s, [marker](const Message&) { return *marker == 42; }); marker.reset(); }
    CHECK(weak.expired()); client.dispatch(incoming("unobserved")); client.run();
}

void paused_time_still_counts_toward_deadlines() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int count = 0;
    client.spawn(expect_timeout(client, count)); client.spawn(stop_later(client)); client.run();
    std::this_thread::sleep_for(10ms);
    client.dispatch(incoming("late")); client.run(); CHECK(count == 1);
}

task<> wait_and_modify_listeners(Client& client, ListenerConnection& connection, int& added) {
    auto message = co_await client.wait_for<Message>(1s, [&](const Message& event) {
        if (event.content == "ignore") {
            (void)client.subscribe_message([counter = &added](Message) -> task<> { ++*counter; co_return; });
            return false;
        }
        connection.disconnect();
        return true;
    });
    CHECK(message.content == "match");
}
void predicates_cannot_invalidate_listener_snapshots() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    int original = 0, added = 0;
    auto connection = client.subscribe_message([&](Message) -> task<> { ++original; co_return; });
    client.spawn(wait_and_modify_listeners(client, connection, added)); client.spawn(emit_messages(client)); client.run();
    CHECK(original == 2 && added == 1 && !connection);
    client.dispatch(incoming("next")); client.run(); CHECK(original == 2 && added == 2);
}

task<> emit_cancellation(asio::cancellation_signal& signal) {
    co_await sleep_for(1ms); signal.emit(asio::cancellation_type::terminal);
}
void asio_cancellation_releases_wait_state() {
    auto transport = std::make_shared<testing::MemoryTransport>(); Client client{transport};
    asio::cancellation_signal signal; bool cancelled = false;
    auto marker = std::make_shared<int>(42); std::weak_ptr<int> weak = marker;
    auto work = client.wait_for<Message>(60s, [marker](const Message&) { return *marker == 0; }); marker.reset();
    asio::co_spawn(client.executor(), std::move(work), asio::bind_cancellation_slot(signal.slot(), [&](std::exception_ptr error, Message) {
        CHECK(error != nullptr);
        try { std::rethrow_exception(error); }
        catch (const asio::system_error& failure) { cancelled = failure.code() == asio::error::operation_aborted; }
    }));
    client.spawn(emit_cancellation(signal)); client.run(); CHECK(cancelled && weak.expired());
}
} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"listener disconnect ownership", listener_handles_preserve_queued_callbacks},
        {"dropped and stale handles", dropped_and_stale_handles_are_safe},
        {"filtered broadcast and binding", waits_filter_broadcast_and_preserve_binding},
        {"Ready and RawEvent", typed_events_support_waits_and_subscriptions},
        {"deadline predicate release", deadlines_release_predicates},
        {"cross-thread stop tokens", stop_tokens_cancel_before_and_during_waits},
        {"close cancellation", close_cancels_all_wait_types},
        {"predicate error isolation", predicate_failures_are_isolated},
        {"completion race", first_processed_completion_wins},
        {"paused destruction", paused_destruction_releases_pending_waits},
        {"deadline/executor validation", invalid_deadlines_and_foreign_executors_are_rejected},
        {"lazy unstarted wait", unstarted_waits_do_not_register},
        {"paused timeout", paused_time_still_counts_toward_deadlines},
        {"predicate listener mutation", predicates_cannot_invalidate_listener_snapshots},
        {"Asio cancellation", asio_cancellation_releases_wait_state}
    };
    int failures = 0;
    for (const auto& [name, test] : cases) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
