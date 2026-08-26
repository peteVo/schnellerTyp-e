# SPDX-License-Identifier: MIT
#
# schnellerTyp-e — cross-platform IME / text transformation tool.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
#
# See BUILD.md for the per-platform prerequisites.

cmake_minimum_required(VERSION 3.21)

project(schnellerTyp-e
    VERSION 0.1.0
    DESCRIPTION "Cross-platform input method editor and text transformation tool"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(SCHNELLERTYPE_BUILD_TESTS "Build the engine unit tests" ON)
option(SCHNELLERTYPE_WERROR "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# 6.4 is the floor so that the Qt shipped by Debian 12 and Ubuntu 24.04 works
# unmodified; everything newer is fine.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
qt_standard_project_setup(REQUIRES 6.4)

find_package(UIOHook REQUIRED)

# libuiohook changed its dispatch signature in 1.3 (it gained a user-data
# pointer). Detect it instead of guessing from a version number, because
# distributions patch both ways.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${UIOHOOK_INCLUDE_DIRS})
# check_cxx_source_compiles links as well as compiles, so the library has to be
# on the line — otherwise a perfectly good 1.3 install fails the probe at link
# time and the 1.2 signature gets used against a 1.3 header.
set(CMAKE_REQUIRED_LIBRARIES ${UIOHOOK_LIBRARIES})
check_cxx_source_compiles("
    #include <uiohook.h>
    static void proc(uiohook_event* const, void*) {}
    int main() { hook_set_dispatch_proc(&proc, nullptr); return 0; }
" UIOHOOK_DISPATCH_HAS_USERDATA)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

# ---------------------------------------------------------------------------
# Core — no Qt, no OS headers, fully unit-testable
# ---------------------------------------------------------------------------

add_library(schnellertype_core STATIC
    src/core/Unicode.cpp
    src/core/RuleEngineRegistry.cpp
    src/core/german/GermanRuleEngine.cpp
    src/core/vietnamese/VnTables.cpp
    src/core/vietnamese/VietnameseRuleEngine.cpp
    src/core/custom/RuleSetEngine.cpp
)
target_include_directories(schnellertype_core PUBLIC src)
set_target_properties(schnellertype_core PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)

# ---------------------------------------------------------------------------
# Hook layer — libuiohook plus one native injector per platform
# ---------------------------------------------------------------------------

set(SCHNELLERTYPE_HOOK_SOURCES
    src/hook/HookService.cpp
    src/hook/KeyMapper.cpp
    src/hook/PlatformSupport.cpp
    src/hook/TextInjector.cpp
)

if(WIN32)
    list(APPEND SCHNELLERTYPE_HOOK_SOURCES src/hook/platform/TextInjectorWindows.cpp)
elseif(APPLE)
    list(APPEND SCHNELLERTYPE_HOOK_SOURCES src/hook/platform/TextInjectorMac.cpp)
elseif(UNIX)
    list(APPEND SCHNELLERTYPE_HOOK_SOURCES src/hook/platform/TextInjectorX11.cpp)
endif()

add_library(schnellertype_hook STATIC ${SCHNELLERTYPE_HOOK_SOURCES})
target_link_libraries(schnellertype_hook PUBLIC schnellertype_core UIOHook::UIOHook)
set_target_properties(schnellertype_hook PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)

if(UIOHOOK_DISPATCH_HAS_USERDATA)
    target_compile_definitions(schnellertype_hook PRIVATE SCHNELLERTYPE_UIOHOOK_DISPATCH_USERDATA)
endif()

# --- platform definitions and native libraries ------------------------------

if(WIN32)
    target_compile_definitions(schnellertype_hook PUBLIC
        SCHNELLERTYPE_PLATFORM_WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(schnellertype_hook PUBLIC user32)

elseif(APPLE)
    target_compile_definitions(schnellertype_hook PUBLIC SCHNELLERTYPE_PLATFORM_MACOS)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
    find_library(APPLICATION_SERVICES_LIBRARY ApplicationServices REQUIRED)
    find_library(CORE_GRAPHICS_LIBRARY CoreGraphics REQUIRED)
    find_library(CORE_FOUNDATION_LIBRARY CoreFoundation REQUIRED)
    target_link_libraries(schnellertype_hook PUBLIC
        ${CARBON_LIBRARY}
        ${APPLICATION_SERVICES_LIBRARY}
        ${CORE_GRAPHICS_LIBRARY}
        ${CORE_FOUNDATION_LIBRARY}
    )

elseif(UNIX)
    target_compile_definitions(schnellertype_hook PUBLIC SCHNELLERTYPE_PLATFORM_LINUX)
    find_package(X11 REQUIRED)
    if(NOT X11_XTest_FOUND)
        message(FATAL_ERROR
            "The XTest extension development files are missing. "
            "Install libxtst-dev (Debian/Ubuntu), libXtst-devel (Fedora) or "
            "libxtst (Arch/CachyOS).")
    endif()
    target_link_libraries(schnellertype_hook PUBLIC
        X11::X11
        X11::Xtst
        ${CMAKE_THREAD_LIBS_INIT}
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(schnellertype_hook PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# QML component module
# ---------------------------------------------------------------------------

add_subdirectory(qml/components)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

qt_add_executable(schnellerTyp-e
    src/main.cpp
    src/app/AppController.cpp
    src/app/RuleSetLoader.cpp
    src/app/Settings.cpp
    src/app/TrayController.cpp
)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(schnellerTyp-e
    URI SchnellerTypE
    VERSION 1.0
    # Pinned explicitly: Qt 6.5 changed the default from "/" to "/qt/qml", and
    # main.cpp loads the window by URL so the prefix must not move underneath it.
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES qml/Main.qml
)

target_compile_definitions(schnellerTyp-e PRIVATE
    SCHNELLERTYPE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(schnellerTyp-e PRIVATE
    schnellertype_hook
    # Both halves of the QML module: the backing library and the generated
    # plugin that performs the type registration.
    schnellertype_components
    schnellertype_componentsplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

# --- warnings ---------------------------------------------------------------

if(MSVC)
    # /utf-8 is not optional here. The sources are UTF-8 and contain character
    # literals like U'\u00e4' and UTF-8 tables of Vietnamese nuclei. Without it
    # MSVC reads them in the system ANSI code page: the character literals
    # become multi-character constants (a hard error) and the tables become
    # silently wrong bytes, which is worse.
    set(SCHNELLERTYPE_WARNINGS /W4 /permissive- /utf-8)
    if(SCHNELLERTYPE_WERROR)
        list(APPEND SCHNELLERTYPE_WARNINGS /WX)
    endif()
else()
    set(SCHNELLERTYPE_WARNINGS -Wall -Wextra -Wpedantic)
    if(SCHNELLERTYPE_WERROR)
        list(APPEND SCHNELLERTYPE_WARNINGS -Werror)
    endif()
endif()

foreach(target schnellertype_core schnellertype_hook schnellerTyp-e)
    target_compile_options(${target} PRIVATE ${SCHNELLERTYPE_WARNINGS})
endforeach()

# ---------------------------------------------------------------------------
# Platform packaging
# ---------------------------------------------------------------------------

if(WIN32)
    set_target_properties(schnellerTyp-e PROPERTIES WIN32_EXECUTABLE ON)

    # libuiohook is normally built as a DLL. Windows has no rpath, so the DLL
    # has to sit next to the executable or the app dies at startup with a dialog
    # that does not name the missing file. Find it once and copy it after every
    # build so running from the build tree just works.
    get_filename_component(_uiohook_lib_dir "${UIOHOOK_LIBRARY}" DIRECTORY)
    find_file(UIOHOOK_RUNTIME_DLL
        NAMES uiohook.dll libuiohook.dll
        HINTS "${_uiohook_lib_dir}" "${_uiohook_lib_dir}/../bin"
        NO_DEFAULT_PATH
    )
    if(UIOHOOK_RUNTIME_DLL)
        add_custom_command(TARGET schnellerTyp-e POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${UIOHOOK_RUNTIME_DLL}" "$<TARGET_FILE_DIR:schnellerTyp-e>"
            COMMENT "Copying uiohook.dll next to the executable"
            VERBATIM
        )
    else()
        message(STATUS "uiohook.dll not found next to ${UIOHOOK_LIBRARY} — "
                       "assuming a static libuiohook build.")
    endif()

elseif(APPLE)
    set_target_properties(schnellerTyp-e PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.schnellertype.app"
        MACOSX_BUNDLE_BUNDLE_NAME "schnellerTyp-e"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
    )
    # Accessibility is granted per signed bundle: an ad-hoc signature is enough
    # for local use but changes identity on every rebuild, so the permission has
    # to be re-granted. See BUILD.md.
    add_custom_command(TARGET schnellerTyp-e POST_BUILD
        COMMAND codesign --force --deep
                --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/schnellerTyp-e.entitlements"
                --sign - "$<TARGET_BUNDLE_DIR:schnellerTyp-e>"
        COMMENT "Ad-hoc signing the bundle so macOS can grant it Accessibility"
        VERBATIM
    )

elseif(UNIX)
    include(GNUInstallDirs)
    install(TARGETS schnellerTyp-e RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/schnellerTyp-e.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(SCHNELLERTYPE_BUILD_TESTS)
    enable_testing()
    add_executable(schnellertype_tests tests/test_core.cpp)
    set_target_properties(schnellertype_tests PROPERTIES AUTOMOC OFF)
    target_link_libraries(schnellertype_tests PRIVATE schnellertype_core)
    target_compile_options(schnellertype_tests PRIVATE ${SCHNELLERTYPE_WARNINGS})
    add_test(NAME engines COMMAND schnellertype_tests)
endif()

# ---------------------------------------------------------------------------

message(STATUS "")
message(STATUS "schnellerTyp-e ${PROJECT_VERSION}")
message(STATUS "  Qt              : ${Qt6_VERSION}")
message(STATUS "  libuiohook      : ${UIOHOOK_LIBRARY}")
message(STATUS "  dispatch userdata: ${UIOHOOK_DISPATCH_HAS_USERDATA}")
message(STATUS "  tests           : ${SCHNELLERTYPE_BUILD_TESTS}")
message(STATUS "")
