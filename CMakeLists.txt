cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" UQBIT_VERSION LIMIT_COUNT 1)
project(uqbit VERSION ${UQBIT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(UQBIT_TESTS "Build the core tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 DBus Network)
find_package(ZLIB REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

# Everything that needs no window: settings, the engine, the Web API client, the torrent list,
# .torrent/magnet parsing, the default app, the CLI. The tests use only this.
add_library(uqbit_core STATIC
    src/config.cpp src/config.h
    src/engine.cpp src/engine.h
    src/client.cpp src/client.h
    src/torrents.cpp src/torrents.h
    src/torrentfile.cpp src/torrentfile.h
    src/defaultapp.cpp src/defaultapp.h
    src/notify.cpp src/notify.h
    src/theme.h src/uqtheme.cpp src/uqtheme.h
    src/uqdiag.cpp src/uqdiag.h
    src/privacy.cpp src/privacy.h
    src/autostart.cpp src/autostart.h
    src/trackers.cpp src/trackers.h
    src/cli.cpp src/cli.h src/cli_util.h src/cli_status.cpp src/cli_trackers.cpp
)
target_include_directories(uqbit_core PUBLIC src)
target_compile_definitions(uqbit_core PUBLIC UQBIT_VERSION="${PROJECT_VERSION}")
target_link_libraries(uqbit_core PUBLIC Qt6::Core Qt6::Network Qt6::DBus ZLIB::ZLIB)

qt_add_executable(uqbit
    src/main.cpp
    src/app.cpp src/app.h
    src/adding.cpp src/protect.cpp src/search.cpp
    src/single.cpp src/single.h
    src/tray.cpp src/tray.h
)

# Must be set before the module is defined, or qmldir lists Theme as a plain type.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(uqbit
    URI Uqbit
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Sidebar.qml
        qml/TorrentRow.qml
        qml/DetailsPanel.qml
        qml/AddSheet.qml
        qml/RemoveDialog.qml
        qml/LinkDialog.qml
        qml/EmptyState.qml
        qml/IconButton.qml
        qml/PillButton.qml
        qml/ProgressLine.qml
        qml/Toast.qml
        qml/SearchView.qml
        qml/PluginsSheet.qml
        qml/PrivacySheet.qml
        qml/ThemedSwitch.qml
        qml/ThemedCheck.qml
        qml/logic.js
    RESOURCES
        icons/uqbit.svg icons/add.svg icons/link.svg icons/open.svg icons/close.svg
        icons/play.svg icons/pause.svg icons/remove.svg icons/folder.svg icons/magnet.svg
        icons/down.svg icons/up.svg icons/all.svg icons/check.svg icons/seed.svg
        icons/stop.svg icons/error.svg icons/search.svg icons/refresh.svg icons/file.svg
        icons/shield.svg icons/shield-off.svg icons/plugin.svg icons/globe.svg
)

target_link_libraries(uqbit PRIVATE uqbit_core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2)

include(GNUInstallDirs)
install(TARGETS uqbit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/desktop/io.github.asafelobotomy.uqbit.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/desktop/io.github.asafelobotomy.uqbit.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES packaging/systemd/uqbit-engine.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user)
install(FILES icons/uqbit.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME io.github.asafelobotomy.uqbit.svg)
install(DIRECTORY uq/pages/uqbit DESTINATION ${CMAKE_INSTALL_DATADIR}/uq/settings/pages)
# uQbit's problem codes, for uqctl explain and uQ Settings' Problems page.
install(FILES uq/diag/uqbit.toml DESTINATION ${CMAKE_INSTALL_DATADIR}/uq/diag)
install(DIRECTORY uq/widgets/uqbit uq/widgets/uqbit-quick DESTINATION ${CMAKE_INSTALL_DATADIR}/uq/shell/widgets)

if(UQBIT_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tst_core tests/core/tst_core.cpp)
    target_link_libraries(tst_core PRIVATE uqbit_core Qt6::Test)
    add_test(NAME core COMMAND tst_core)
    add_executable(tst_privacy tests/core/tst_privacy.cpp)
    target_link_libraries(tst_privacy PRIVATE uqbit_core Qt6::Test)
    add_test(NAME privacy COMMAND tst_privacy)
endif()
