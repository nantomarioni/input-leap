# InputLeap -- mouse and keyboard sharing utility
# Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)
# Copyright (C) 2012-2016 Symless Ltd.
# Copyright (C) 2011 Nick Bolton
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

# The Qt Quick GUI (docs/ui-redesign.md): app/ is the C++ application layer,
# qml/ the window, res/ the icons.

find_package(Qt6 6.8 COMPONENTS Core Gui Widgets Network Qml Quick QuickControls2 Svg REQUIRED)
if (INPUTLEAP_BUILD_TESTS)
    find_package(Qt6 COMPONENTS Test REQUIRED)
endif()
message(STATUS "Building the GUI with Qt ${Qt6_VERSION}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
qt_policy(SET QTP0001 NEW)   # QML resources live under :/qt/qml
qt_policy(SET QTP0004 NEW)   # one qmldir per QML subdirectory

if (WIN32)
    set(GUI_RC_FILES res/win/InputLeap.rc)
    set_property(SOURCE res/win/InputLeap.rc PROPERTY COMPILE_FLAGS /nologo)
    set(GUI_PLATFORM_SOURCES app/Platform_win.cpp)
elseif (APPLE)
    set(GUI_PLATFORM_SOURCES app/Platform_mac.cpp)
else()
    set(GUI_PLATFORM_SOURCES app/Platform_other.cpp)
endif()

# The parts that need nothing from Qt Quick; the unit tests build them too.
set(GUI_MODEL_SOURCES
    app/Action.cpp
    app/Action.h
    app/AppSettings.cpp
    app/AppSettings.h
    app/CoreEventParser.cpp
    app/CoreEventParser.h
    app/Hotkey.cpp
    app/Hotkey.h
    app/KeySequence.cpp
    app/KeySequence.h
    app/LayoutModel.cpp
    app/LayoutModel.h
    app/LogModel.cpp
    app/LogModel.h
)

qt_add_executable(input-leap WIN32
    app/main.cpp
    app/AppController.cpp
    app/AppController.h
    app/CoreProcess.cpp
    app/CoreProcess.h
    app/Discovery.cpp
    app/Discovery.h
    app/ElevateMode.h
    app/Ipc.cpp
    app/Ipc.h
    app/IpcClient.cpp
    app/IpcClient.h
    app/IpcReader.cpp
    app/IpcReader.h
    app/KeyCaptureItem.cpp
    app/KeyCaptureItem.h
    app/Platform.h
    app/SingleInstance.cpp
    app/SingleInstance.h
    app/SyncService.cpp
    app/SyncService.h
    app/TlsIdentity.cpp
    app/TlsIdentity.h
    app/TrayController.cpp
    app/TrayController.h
    app/ZeroconfBrowser.cpp
    app/ZeroconfBrowser.h
    app/ZeroconfRecord.h
    app/ZeroconfRegister.cpp
    app/ZeroconfRegister.h
    app/ZeroconfResolver.cpp
    app/ZeroconfResolver.h
    app/ZeroconfThread.cpp
    app/ZeroconfThread.h
    ${GUI_PLATFORM_SOURCES}
    ${GUI_MODEL_SOURCES}
    ../fork/gui/NotificationRouter.cpp
    ../fork/gui/NotificationRouter.h
    ../fork/gui/OverlayNotification.cpp
    ../fork/gui/OverlayNotification.h
    ../fork/gui/UpdateChecker.cpp
    ../fork/gui/UpdateChecker.h
    ${GUI_RC_FILES}
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(input-leap
    URI InputLeap.Ui
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/AppHeader.qml
        qml/components/AppStatusBar.qml
        qml/components/Banner.qml
        qml/components/Card.qml
        qml/components/EmptyState.qml
        qml/components/HotkeyEditor.qml
        qml/components/InfoRow.qml
        qml/components/KeyCaptureField.qml
        qml/components/LayoutCanvas.qml
        qml/components/LogDrawer.qml
        qml/components/NavRail.qml
        qml/components/NumberBox.qml
        qml/components/RailItem.qml
        qml/components/RoleOption.qml
        qml/components/RoleSwitch.qml
        qml/components/ScreenSettingsPanel.qml
        qml/components/SectionTitle.qml
        qml/components/SettingRow.qml
        qml/components/StatusPill.qml
        qml/pages/OverviewPage.qml
        qml/pages/LayoutPage.qml
        qml/pages/ServerPage.qml
        qml/pages/ComputersPage.qml
        qml/pages/SecurityPage.qml
        qml/pages/ProcessesPage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        qml/icons/logo.svg
        qml/icons/overview.svg
        qml/icons/layout.svg
        qml/icons/server.svg
        qml/icons/computers.svg
        qml/icons/security.svg
        qml/icons/processes.svg
        qml/icons/settings.svg
)

qt_add_resources(input-leap "inputleap_tray"
    PREFIX "/tray"
    BASE res
    FILES
        res/icons/128x128/input-leap-connected.png
        res/icons/128x128/input-leap-connected-mask.png
        res/icons/128x128/input-leap-disconnected.png
        res/icons/128x128/input-leap-disconnected-mask.png
        res/icons/128x128/input-leap-transfering.png
        res/icons/128x128/input-leap-transfering-mask.png
        res/icons/256x256/input-leap.png
)

target_compile_definitions(input-leap PRIVATE
    INPUTLEAP_VERSION_STAGE="${INPUTLEAP_VERSION_STAGE}"
    INPUTLEAP_REVISION="${INPUTLEAP_REVISION}")
# the generated QML type registration includes the headers by basename
target_include_directories(input-leap PRIVATE app)
target_link_libraries(input-leap PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
    net arch base io common ${libs} OpenSSL::SSL OpenSSL::Crypto)
if (BUILD_MSWINDOWS)
    target_include_directories(input-leap PRIVATE $ENV{BONJOUR_SDK_HOME}/Include)
    find_library(DNSSD_LIB dnssd.lib HINTS ENV BONJOUR_SDK_HOME PATH_SUFFIXES "Lib/x64")
    set_target_properties(input-leap PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT")
    target_link_libraries(input-leap PRIVATE ${DNSSD_LIB})
endif()
if (BUILD_CARBON)
    find_library(APPSERVICES_LIB ApplicationServices)
    target_link_libraries(input-leap PRIVATE ${APPSERVICES_LIB})
endif()
if (BUILD_XWINDOWS OR BUILD_LIBEI)
    target_link_libraries(input-leap PRIVATE dns_sd Threads::Threads)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    install(TARGETS input-leap DESTINATION ${INPUTLEAP_BUNDLE_BINARY_DIR})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    install(TARGETS input-leap DESTINATION bin)
elseif (WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    endif()
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    add_custom_command(
        TARGET input-leap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/qtDeploy"
        COMMAND ${QT_DEPLOY_TOOL} --no-compiler-runtime --no-system-d3d-compiler
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            --dir "${CMAKE_BINARY_DIR}/qtDeploy" $<TARGET_FILE:input-leap>
    )
    install(TARGETS input-leap DESTINATION .)
    install(
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/
        DESTINATION .
        FILES_MATCHING PATTERN "*.*"
    )
endif()

if (INPUTLEAP_BUILD_TESTS)
    add_executable(guiunittests
        test/main.cpp
        test/KeySequenceTests.cpp
        test/HotkeyTests.cpp
        test/CoreEventParserTests.cpp
        test/LayoutModelTests.cpp
        test/AppSettingsTests.cpp
        test/SyncServiceTests.cpp
        app/SyncService.cpp
        ${GUI_MODEL_SOURCES}
    )
    target_include_directories(guiunittests PRIVATE app ../../ext)
    # the sync test generates certificates the way the core does
    target_link_libraries(guiunittests gtest gmock
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Qml Qt6::Test
        net arch base io common ${libs} OpenSSL::SSL OpenSSL::Crypto)
    add_test(NAME guiunittests COMMAND guiunittests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()
