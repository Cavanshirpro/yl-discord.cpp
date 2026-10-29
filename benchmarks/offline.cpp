// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

using namespace yl::discord;
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t first_id = 100'000'000'000'000'000ULL;
struct Result { double seconds; std::vector<double> latency_us; };
Result measure(std::size_t count, std::size_t batch) {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    std::vector<Clock::time_point> starts(count);
    std::vector<double> latencies;
    latencies.reserve(count);
    bot.command("ping", [&](Context ctx) -> task<> {
        const auto index = static_cast<std::size_t>(ctx.message.id.value() - first_id);
        auto sent = co_await ctx.reply("Pong!");
        if (sent.content != "Pong!") throw std::runtime_error("Invalid benchmark reply");
        latencies.push_back(std::chrono::duration<double, std::micro>(Clock::now() - starts.at(index)).count());
    });
    double seconds = 0;
    for (std::size_t offset = 0; offset < count; offset += batch) {
        const auto size = (std::min)(batch, count - offset);
        std::vector<Message> inputs;
        inputs.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            Message message;
            message.id = Snowflake{first_id + offset + i}; message.channel_id = Snowflake{42};
            message.author = User{Snowflake{7}, "fixture-user", false}; message.content = "!ping";
            inputs.push_back(std::move(message));
        }
        const auto begin = Clock::now();
        for (std::size_t i = 0; i < size; ++i) {
            starts[offset + i] = Clock::now(); bot.dispatch(std::move(inputs[i]));
        }
        bot.run();
        seconds += std::chrono::duration<double>(Clock::now() - begin).count();
    }
    if (latencies.size() != count || transport->sent().size() != count)
        throw std::runtime_error("Incomplete benchmark dispatch/reply count");
    std::sort(latencies.begin(), latencies.end());
    return {seconds, std::move(latencies)};
}
std::uint64_t peak_memory() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters)) return counters.PeakWorkingSetSize;
    return 0;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}
double percentile(const std::vector<double>& values, double fraction) {
    return values[static_cast<std::size_t>(std::ceil(values.size() * fraction)) - 1];
}
} // namespace
int main(int argc, char** argv) {
    try {
        const std::size_t count = argc > 1 ? convert_argument<std::size_t>(argv[1]) : 10000;
        const std::size_t batch = argc > 2 ? convert_argument<std::size_t>(argv[2]) : 128;
        if (argc > 3 || count < 1 || count > 200000 || batch < 1 || batch > 4096) throw std::invalid_argument("Use count 1..200000, batch 1..4096");
        (void)measure((std::min)(count, std::size_t{256}), batch);
        const auto result = measure(count, batch);
        std::cout << std::setprecision(10) << "{\"implementation\":\"yl-discord.cpp\",\"version\":" << std::quoted(std::string(version))
            << ",\"compiler\":" << std::quoted(
#if defined(__clang__)
                std::string("Clang ") + __clang_version__
#elif defined(_MSC_VER)
                std::string("MSVC ") + std::to_string(_MSC_VER)
#else
                std::string("GCC ") + __VERSION__
#endif
            ) << ",\"configuration\":" << std::quoted(YL_BENCHMARK_CONFIG)
            << ",\"iterations\":" << count << ",\"batch_size\":" << batch
            << ",\"dispatch_seconds\":" << result.seconds << ",\"commands_per_second\":" << count / result.seconds
            << ",\"latency_us\":{\"p50\":" << percentile(result.latency_us, .5) << ",\"p95\":" << percentile(result.latency_us, .95)
            << ",\"p99\":" << percentile(result.latency_us, .99) << "},\"peak_resident_bytes\":" << peak_memory() << "}\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
