# InputLeap Fork - Modern GUI Components
# This library contains fork-specific GUI enhancements

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(headers
    DaemonManager.h
    ModernMainWindow.h
)

set(sources  
    DaemonManager.cpp
    ModernMainWindow.cpp
)

set(ui_files
    ModernMainWindow.ui
)

# Process Qt UI files
qt6_wrap_ui(ui_sources ${ui_files})
qt6_wrap_cpp(moc_sources ${headers})

# Create modern GUI library
add_library(inputleap_fork_gui STATIC
    ${sources}
    ${ui_sources} 
    ${moc_sources}
)

target_link_libraries(inputleap_fork_gui
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    common
    base
)

target_include_directories(inputleap_fork_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/gui/src  # Allow access to GUI headers
    ${CMAKE_SOURCE_DIR}/src/lib      # Allow access to inputleap library headers
)

# Add to fork extensions
target_link_libraries(inputleap_fork_extensions
    inputleap_fork_gui
)
