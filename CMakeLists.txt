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
    # Protocol & Codec
    src/core/Protocol.h
    src/core/PacketCodec.h
    src/core/PacketCodec.cpp
    # Data Models
    src/core/model/MessageListModel.h
    src/core/model/MessageListModel.cpp
    src/core/model/NearbyGroupModel.h
    src/core/model/NearbyGroupModel.cpp
    src/core/model/GroupMemberListModel.h
    src/core/model/GroupMemberListModel.cpp
    # LAN Discovery
    src/core/DeviceDiscovery.h
    src/core/DeviceDiscovery.cpp
    # Sessions & Managers
    src/core/session/PeerSession.h
    src/core/session/PeerSession.cpp
    src/core/session/PeerSessionTransfer.cpp
    src/core/session/TransferManager.h
    src/core/session/TransferManager.cpp
    src/core/session/GroupFileFetcher.h
    src/core/session/GroupFileFetcher.cpp
    src/core/session/GroupRoom.h
    src/core/session/GroupRoom.cpp
    src/core/session/GroupRoomPackets.cpp
    src/core/session/GroupManager.h
    src/core/session/GroupManager.cpp
    # Platform Bridges & Facade
    src/platform/AndroidNetworkBridge.h
    src/platform/AndroidNetworkBridge.cpp
    src/platform/AndroidStorageBridge.h
    src/platform/AndroidStorageBridge.cpp
    src/platform/AndroidIntentBridge.h
    src/platform/AndroidIntentBridge.cpp
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

set(LANDROP_QML_FILES
    qml/Theme.qml
    qml/Main.qml
    qml/DeviceListPane.qml
    qml/ChatPane.qml
    qml/components/AvatarBadge.qml
    qml/components/DeviceListCard.qml
    qml/components/ChatHeaderBar.qml
    qml/components/MediaCard.qml
    qml/components/FileCard.qml
    qml/components/MessageRow.qml
    qml/components/ChatInputBar.qml
    qml/components/ImageLightbox.qml
    qml/dialogs/ConfirmDialog.qml
    qml/dialogs/RenameDialog.qml
    qml/dialogs/CreateGroupDialog.qml
    qml/dialogs/JoinPinDialog.qml
    qml/dialogs/DirectConnectDialog.qml
    qml/dialogs/GroupMembersDialog.qml
)

foreach(qml_file IN LISTS LANDROP_QML_FILES)
    get_filename_component(qml_basename "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_basename}")
endforeach()

qt_add_qml_module(LanDrop
    URI LanDrop
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${LANDROP_QML_FILES}
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
