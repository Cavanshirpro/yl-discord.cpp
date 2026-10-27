# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Conan 2 recipe; Asio is a public header dependency."""
import os
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load

required_conan_version = ">=2.21"


class YlDiscordConan(ConanFile):
    name = "yl-discord-cpp"
    description = "Coroutine-first C++20 Discord bot library"
    license = "LGPL-3.0-or-later"
    url = "https://github.com/Cavanshirpro/yl-discord.cpp"
    homepage = url
    topics = ("discord", "cpp20", "coroutines", "asio")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True,
                       "libcurl/*:with_websockets": True,
                       "libcurl/*:with_ssl": "openssl", "libcurl/*:with_ca_fallback": True}
    exports = "VERSION"
    exports_sources = (
        "VERSION", "CMakeLists.txt", "cmake/*", "include/*", "src/*",
        "LICENSE", "COPYING", "NOTICE", "THIRD_PARTY.md",
        "third_party/licenses/*",
    )

    def set_version(self):
        self.version = load(self, os.path.join(self.recipe_folder, "VERSION")).strip()

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("asio/1.30.2", transitive_headers=True)
        self.requires("libcurl/8.23.0")
        self.requires("nlohmann_json/3.12.0")

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, "20")
        if not self.dependencies["libcurl"].options.with_websockets or not self.dependencies["libcurl"].options.with_ssl:
            raise ConanInvalidConfiguration("libcurl needs WebSockets and TLS support")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.variables["YL_DISCORD_BUILD_TESTS"] = False
        toolchain.variables["YL_DISCORD_BUILD_EXAMPLES"] = False
        toolchain.variables["YL_DISCORD_FETCH_ASIO"] = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        for name in ("LICENSE", "COPYING", "NOTICE", "THIRD_PARTY.md"):
            copy(self, name, self.source_folder, os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.libs = ["yl_discord"]
        self.cpp_info.defines = ["ASIO_STANDALONE", "ASIO_NO_DEPRECATED"]
        if not self.options.shared:
            self.cpp_info.defines.append("YL_DISCORD_STATIC_DEFINE")
        self.cpp_info.set_property("cmake_file_name", "ylDiscord")
        self.cpp_info.set_property("cmake_target_name", "yl::discord")
        self.cpp_info.set_property("pkg_config_name", "yl-discord")
        if self.settings.os == "Windows":
            self.cpp_info.defines.append("_WIN32_WINNT=0x0A00")
            self.cpp_info.system_libs = ["ws2_32", "mswsock"]
        elif self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread"]
