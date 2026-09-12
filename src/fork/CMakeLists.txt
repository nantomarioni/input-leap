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


# --- fork_base: dependency-free helpers (logging, quit tracing) ------------
# Split out so upstream `base` can link it without dragging in the
# server/client extension objects (the gui links base but must not need
# Server/BaseClientProxy symbols).
set(FORK_BASE_SOURCES
    lib/base/QuitReason.cpp
    lib/base/LogExtension.h
)
if(WIN32)
    list(APPEND FORK_BASE_SOURCES lib/base/LogExtension.cpp)
else()
    list(APPEND FORK_BASE_SOURCES lib/base/LogExtension_posix.cpp)
endif()

add_library(inputleap_fork_base STATIC ${FORK_BASE_SOURCES})
target_include_directories(inputleap_fork_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(inputleap_fork_base PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# --- fork server/client extensions -----------------------------------------

set(FORK_SERVER_SOURCES
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
    )
endif()

# macOS-specific extensions
if(APPLE)
    list(APPEND FORK_SERVER_SOURCES
        lib/platform/OSXScreenExtension.mm
        lib/platform/OSXScreenExtension.h
    )
endif()

if(LINUX)
    list(APPEND FORK_SERVER_SOURCES
        lib/platform/XWindowsScreenExtension.cpp
        lib/platform/XWindowsScreenExtension.h
    )
endif()

add_library(inputleap_fork_server STATIC ${FORK_SERVER_SOURCES})
target_link_libraries(inputleap_fork_server PUBLIC inputleap_fork_base)

target_include_directories(inputleap_fork_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/lib ${CMAKE_BINARY_DIR}/src/lib ${CMAKE_SOURCE_DIR}/ext/gulrak-filesystem/include)

set_target_properties(inputleap_fork_server PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
