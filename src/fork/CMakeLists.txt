# InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
# Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


set(FORK_SERVER_SOURCES
    lib/base/QuitReason.cpp
    lib/server/ServerExtension.cpp
    lib/server/ServerExtension.h
    lib/server/BaseClientProxyExtension.cpp
    lib/server/BaseClientProxyExtension.h
    lib/server/ConfigExtension.cpp
    lib/server/ConfigExtension.h
    lib/server/PrimaryClientExtension.cpp
    lib/server/PrimaryClientExtension.h
    lib/client/ClientExtension.cpp
    lib/client/ClientExtension.h
    lib/client/ServerProxyExtension.cpp
    lib/client/ServerProxyExtension.h
    lib/inputleap/option_types_extension.h
    lib/inputleap/protocol_types_extension.cpp
    lib/inputleap/protocol_types_extension.h
    lib/inputleap/ScreenExtension.cpp
    lib/inputleap/ScreenExtension.h
    lib/inputleap/PlatformScreenLoggingWrapperExtension.cpp
    lib/inputleap/PlatformScreenLoggingWrapperExtension.h
    lib/platform/PlatformScreenExtension.cpp
    lib/platform/PlatformScreenExtension.h
)

# MSWindows-specific extension: only compile on Windows hosts.
if(WIN32)
    list(APPEND FORK_SERVER_SOURCES
        lib/platform/MSWindowsScreenExtension.cpp
        lib/platform/MSWindowsScreenExtension.h
        lib/base/LogExtension.cpp
        lib/base/LogExtension.h
    )
endif()

# macOS-specific extensions
if(APPLE)
    list(APPEND FORK_SERVER_SOURCES
        lib/platform/OSXScreenExtension.mm
        lib/platform/OSXScreenExtension.h
    lib/base/LogExtension_posix.cpp
    )
endif()

if(LINUX)
    list(APPEND FORK_SERVER_SOURCES
        lib/platform/XWindowsScreenExtension.cpp
        lib/platform/XWindowsScreenExtension.h
    lib/base/LogExtension_posix.cpp
    )
endif()

add_library(inputleap_fork_server STATIC ${FORK_SERVER_SOURCES})

target_include_directories(inputleap_fork_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/lib ${CMAKE_BINARY_DIR}/src/lib ${CMAKE_SOURCE_DIR}/ext/gulrak-filesystem/include)

set_target_properties(inputleap_fork_server PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Ensure fork code uses the bundled gulrak-filesystem on platforms where
# std::filesystem is not available (older macOS SDKs). This keeps the
# compatibility change confined to fork code only.
target_compile_definitions(inputleap_fork_server PRIVATE INPUTLEAP_USE_GULRAK_FILESYSTEM=1)
