cmake_minimum_required(VERSION 3.20)
project(translator-runner
    VERSION 0.1.0
    DESCRIPTION "KRunner plugin for translating text via translate-shell"
    LANGUAGES CXX
)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
    message(STATUS "Using ccache: ${CCACHE_FOUND}")
endif()

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config
    Runner
    CoreAddons
    I18n
    Notifications
)

kcoreaddons_add_plugin(translatorrunner
    SOURCES
        src/translatorrunner.cpp
        src/translatorrunner.h
    INSTALL_NAMESPACE "kf6/krunner"
)

target_link_libraries(translatorrunner PRIVATE
    Qt6::Core
    Qt6::Widgets
    KF6::Runner
    KF6::CoreAddons
    KF6::I18n
    KF6::Notifications
)

target_include_directories(translatorrunner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(FILES src/translatorrunner.notifyrc
    DESTINATION ${KDE_INSTALL_DATADIR}/knotifications6
)

message(STATUS "─────────────────────────────────────────")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Qt6: ${Qt6_VERSION}")
message(STATUS "KF6: ${KF6_VERSION}")
message(STATUS "Plugin install: ${KDE_INSTALL_PLUGINDIR}/kf6/krunner/")
message(STATUS "─────────────────────────────────────────")
