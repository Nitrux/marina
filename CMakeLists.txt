cmake_minimum_required(VERSION 3.21)

project(marina VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Marina supports Linux builds only.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(REQUIRED_QT_VERSION 6.9)
set(REQUIRED_KF_VERSION 6.13.0)
set(REQUIRED_MAUIKIT_VERSION 4.0.4)

include(CheckCXXCompilerFlag)
include(FeatureSummary)
include(GNUInstallDirs)

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" MARINA_HOST_PROCESSOR)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" MARINA_TARGET_PROCESSOR)

if(NOT CMAKE_CROSSCOMPILING
   AND MARINA_HOST_PROCESSOR MATCHES "^(x86_64|amd64)$"
   AND MARINA_TARGET_PROCESSOR MATCHES "^(x86_64|amd64)$")
    check_cxx_compiler_flag("-march=x86-64-v3" MARINA_HAS_X86_64_V3_CXX)
    if(MARINA_HAS_X86_64_V3_CXX)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-march=x86-64-v3>)
    endif()
endif()

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE
    COMPONENTS Core Gui Network Qml Quick QuickControls2)
find_package(KF6CoreAddons ${REQUIRED_KF_VERSION} REQUIRED)
find_package(KF6I18n ${REQUIRED_KF_VERSION} REQUIRED)
find_package(MauiKit4 ${REQUIRED_MAUIKIT_VERSION} REQUIRED)
find_package(LayerShellQt 6.0 REQUIRED)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/org.maui.marina.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/org.maui.marina.desktop"
    @ONLY)

add_subdirectory(src)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.maui.marina.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)
    add_custom_target(uninstall
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
