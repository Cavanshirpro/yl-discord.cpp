# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
# Used in both source builds and installed package discovery. Never fetch here.
if(NOT TARGET asio::asio)
    find_package(asio 1.30.2 CONFIG QUIET)
    if(NOT TARGET asio::asio)
        find_path(YL_DISCORD_ASIO_INCLUDE_DIR asio.hpp)
        if(YL_DISCORD_ASIO_INCLUDE_DIR)
            file(STRINGS "${YL_DISCORD_ASIO_INCLUDE_DIR}/asio/version.hpp" _yl_asio_version_line
                REGEX "^#define ASIO_VERSION [0-9]+")
            string(REGEX MATCH "[0-9]+" _yl_asio_version "${_yl_asio_version_line}")
            if(_yl_asio_version GREATER_EQUAL 103002)
                add_library(asio::asio INTERFACE IMPORTED)
                set_target_properties(asio::asio PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${YL_DISCORD_ASIO_INCLUDE_DIR}"
                    INTERFACE_COMPILE_DEFINITIONS "ASIO_STANDALONE")
            endif()
        endif()
    endif()
endif()
