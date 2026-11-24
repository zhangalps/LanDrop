cmake_minimum_required(VERSION 3.21)

project(LanDrop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
)

qt_standard_project_setup(REQUIRES 6.5)

set(LANDROP_CORE_SOURCES
    src/core/Protocol.h
    src/core/PacketCodec.h
    src/core/PacketCodec.cpp
    src/core/DeviceDiscovery.h
    src/core/DeviceDiscovery.cpp
    src/core/TransferSession.h
    src/core/TransferSession.cpp
    src/core/GroupSession.h
    src/core/GroupSession.cpp
    src/platform/AndroidHelper.h
    src/platform/AndroidHelper.cpp
)

add_library(LanDropCore STATIC ${LANDROP_CORE_SOURCES})

target_include_directories(LanDropCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LanDropCore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
)

if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
    target_link_libraries(LanDropCore PRIVATE Qt6::CorePrivate)
endif()

set(LANDROP_APP_SOURCES
    src/main.cpp
)
if(WIN32)
    list(APPEND LANDROP_APP_SOURCES resources/landrop.rc)
endif()

qt_add_executable(LanDrop
    ${LANDROP_APP_SOURCES}
)

set_source_files_properties(resources/icons/landrop.png PROPERTIES QT_RESOURCE_ALIAS landrop.png)
qt_add_resources(LanDrop "landrop_icons"
    PREFIX "/icons"
    FILES
        resources/icons/landrop.png
)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/DeviceListPane.qml PROPERTIES QT_RESOURCE_ALIAS DeviceListPane.qml)
set_source_files_properties(qml/ChatPane.qml PROPERTIES QT_RESOURCE_ALIAS ChatPane.qml)

qt_add_qml_module(LanDrop
    URI LanDrop
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/DeviceListPane.qml
        qml/ChatPane.qml
)

target_link_libraries(LanDrop PRIVATE
    LanDropCore
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(LanDrop PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER org.landrop.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(ANDROID)
    set_property(TARGET LanDrop APPEND PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()

include(GNUInstallDirs)
install(TARGETS LanDrop
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Unit tests (optional when Qt6::Test is available)
option(LANDROP_BUILD_TESTS "Build LanDrop unit tests" ON)
if(LANDROP_BUILD_TESTS AND NOT ANDROID)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        add_executable(LanDropTests tests/test_landrop.cpp)
        target_link_libraries(LanDropTests PRIVATE
            LanDropCore
            Qt6::Test
        )
        add_test(NAME LanDropTests COMMAND LanDropTests)
    endif()
endif()
