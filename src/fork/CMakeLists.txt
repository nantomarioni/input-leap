# InputLeap Fork Extensions
cmake_minimum_required(VERSION 3.15)

message(STATUS "Building InputLeap Fork Extensions")

# Extension system library
add_library(inputleap_fork_extensions STATIC
    extensions/IScreenExtension.h
    extensions/ScreenExtensionRegistry.h
    extensions/ScreenExtensionRegistry.cpp
    extensions/CoreExtensionHooks.h
    extensions/CoreExtensionHooks.cpp
)

# Screen dimming extension (always included for the fork)
target_sources(inputleap_fork_extensions PRIVATE
    extensions/ScreenDimmingExtension.h
    extensions/ScreenDimmingExtension.cpp
)

target_include_directories(inputleap_fork_extensions PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_BINARY_DIR}/src/lib
)

target_link_libraries(inputleap_fork_extensions
    base
)

# Modern GUI components (always included for the fork)
add_subdirectory(gui)
