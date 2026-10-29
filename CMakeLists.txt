# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
cmake_minimum_required(VERSION 3.25)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" YL_DISCORD_VERSION LIMIT_COUNT 1)
project(yl_discord VERSION ${YL_DISCORD_VERSION} DESCRIPTION "Coroutine-first Discord bot library"
    HOMEPAGE_URL "https://github.com/Cavanshirpro/yl-discord.cpp" LANGUAGES CXX)

option(YL_DISCORD_BUILD_EXAMPLES "Build offline and live examples" ${PROJECT_IS_TOP_LEVEL})
option(YL_DISCORD_BUILD_TESTS "Build offline tests" ${PROJECT_IS_TOP_LEVEL})
option(YL_DISCORD_FETCH_ASIO "Download pinned Asio if it is not installed" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

find_package(Threads REQUIRED)
find_package(CURL 7.86 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
include(cmake/ylDiscordAsio.cmake)
if(NOT TARGET asio::asio)
    if(NOT YL_DISCORD_FETCH_ASIO)
        message(FATAL_ERROR "Asio not found. Set YL_DISCORD_ASIO_INCLUDE_DIR or enable YL_DISCORD_FETCH_ASIO.")
    endif()
    include(FetchContent)
    FetchContent_Declare(yl_asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG 42a93679dc4c8c5caf3d3082542f1bfa2438271d # asio-1-30-2
        GIT_PROGRESS FALSE)
    FetchContent_MakeAvailable(yl_asio)
    set(YL_DISCORD_ASIO_INCLUDE_DIR "${yl_asio_SOURCE_DIR}/asio/include")
    include(cmake/ylDiscordAsio.cmake)
endif()

configure_file(include/yl/discord/version.hpp.in generated/yl/discord/version.hpp @ONLY)
add_library(yl_discord src/client.cpp src/commands.cpp src/models.cpp src/transport.cpp src/events.cpp src/memory_transport.cpp
    src/curl_engine.cpp src/rest.cpp src/gateway.cpp src/protocol.cpp)
add_library(yl::discord ALIAS yl_discord)
set_target_properties(yl_discord PROPERTIES EXPORT_NAME discord
    VERSION ${PROJECT_VERSION} SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
if(PROJECT_VERSION_MAJOR EQUAL 0)
    # Experimental object layouts may change; do not load a new DLL into an old consumer.
    set_target_properties(yl_discord PROPERTIES SOVERSION "${PROJECT_VERSION}")
    if(WIN32 AND BUILD_SHARED_LIBS)
        set_target_properties(yl_discord PROPERTIES RUNTIME_OUTPUT_NAME "yl_discord-${PROJECT_VERSION}")
    endif()
endif()
include(GenerateExportHeader)
generate_export_header(yl_discord EXPORT_FILE_NAME generated/yl/discord/export.hpp
    EXPORT_MACRO_NAME YL_DISCORD_EXPORT STATIC_DEFINE YL_DISCORD_STATIC_DEFINE)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(yl_discord PUBLIC YL_DISCORD_STATIC_DEFINE)
    set(YL_DISCORD_PC_STATIC_DEFINE "-DYL_DISCORD_STATIC_DEFINE")
endif()
target_compile_features(yl_discord PUBLIC cxx_std_20)
target_compile_definitions(yl_discord PUBLIC ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_include_directories(yl_discord PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(yl_discord PUBLIC asio::asio Threads::Threads)
target_link_libraries(yl_discord PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
if(WIN32)
    target_compile_definitions(yl_discord PUBLIC _WIN32_WINNT=0x0A00)
    target_link_libraries(yl_discord PUBLIC ws2_32 mswsock)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS yl_discord EXPORT ylDiscordTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "*.in" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/yl/discord/version.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/yl/discord/export.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yl/discord)
install(EXPORT ylDiscordTargets NAMESPACE yl:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ylDiscord)
configure_package_config_file(cmake/ylDiscordConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ylDiscordConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ylDiscord)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ylDiscordConfigVersion.cmake"
    COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ylDiscordConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ylDiscordConfigVersion.cmake" cmake/ylDiscordAsio.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ylDiscord)
install(FILES LICENSE COPYING NOTICE THIRD_PARTY.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/yl-discord.cpp)
install(DIRECTORY third_party/licenses/ DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/yl-discord.cpp/third-party)
if(WIN32)
    set(YL_DISCORD_PC_SYSTEM_LIBS "-lws2_32 -lmswsock")
    set(YL_DISCORD_PC_PLATFORM_DEFINES "-D_WIN32_WINNT=0x0A00")
else()
    set(YL_DISCORD_PC_SYSTEM_LIBS "-pthread")
endif()
file(RELATIVE_PATH YL_DISCORD_PC_PREFIX_RELATIVE
    "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
configure_file(cmake/yl-discord.pc.in yl-discord.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/yl-discord.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
if(MSVC)
    target_compile_options(yl_discord PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(yl_discord PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(YL_DISCORD_BUILD_EXAMPLES)
    foreach(example IN ITEMS offline_bot class_bot ping_bot command_bot wait_for_bot)
        add_executable(yl_${example} examples/${example}.cpp)
        target_link_libraries(yl_${example} PRIVATE yl::discord)
    endforeach()
endif()

if(YL_DISCORD_BUILD_TESTS)
    enable_testing()
    add_executable(yl_discord_tests tests/core.cpp)
    target_link_libraries(yl_discord_tests PRIVATE yl::discord)
    add_test(NAME yl_discord_core COMMAND yl_discord_tests)
    set_tests_properties(yl_discord_core PROPERTIES TIMEOUT 20)
    add_executable(yl_discord_command_tests tests/commands.cpp)
    target_link_libraries(yl_discord_command_tests PRIVATE yl::discord)
    add_test(NAME yl_discord_commands COMMAND yl_discord_command_tests)
    set_tests_properties(yl_discord_commands PROPERTIES TIMEOUT 30)
    add_executable(yl_discord_event_tests tests/events.cpp)
    target_link_libraries(yl_discord_event_tests PRIVATE yl::discord)
    add_test(NAME yl_discord_events COMMAND yl_discord_event_tests)
    set_tests_properties(yl_discord_events PROPERTIES TIMEOUT 30)
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    add_executable(yl_discord_network_tests tests/network.cpp)
    target_link_libraries(yl_discord_network_tests PRIVATE yl::discord)
    add_test(NAME yl_discord_loopback COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/loopback.py $<TARGET_FILE:yl_discord_network_tests>)
    set_tests_properties(yl_discord_loopback PROPERTIES TIMEOUT 90)
endif()
