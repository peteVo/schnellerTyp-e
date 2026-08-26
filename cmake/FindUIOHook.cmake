# SPDX-License-Identifier: MIT
#
# FindUIOHook.cmake — locate libuiohook (https://github.com/kwhat/libuiohook).
#
# Strategy, in order:
#   1. an imported target already created by an earlier call;
#   2. a plain header/library search, seeded with hints from pkg-config and from
#      UIOHOOK_ROOT — this is the preferred path because it yields real file
#      paths, which the caller needs for the version probe and for copying
#      uiohook.dll next to the executable on Windows;
#   3. only if that fails, the CMake package config libuiohook installs. Note
#      that upstream exports the target *unnamespaced* as `uiohook`, while some
#      repackagings namespace it, so both spellings are accepted.
#
# Defines the imported target `UIOHook::UIOHook` and the variables
# UIOHOOK_FOUND, UIOHOOK_INCLUDE_DIRS, UIOHOOK_LIBRARIES, UIOHOOK_LIBRARY.

if(TARGET UIOHook::UIOHook)
    set(UIOHOOK_FOUND TRUE)
    return()
endif()

# --- hints ------------------------------------------------------------------

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_UIOHOOK QUIET uiohook)
endif()

find_path(UIOHOOK_INCLUDE_DIR
    NAMES uiohook.h
    HINTS ${PC_UIOHOOK_INCLUDE_DIRS} ${UIOHOOK_ROOT} $ENV{UIOHOOK_ROOT}
    PATH_SUFFIXES include
)

find_library(UIOHOOK_LIBRARY
    NAMES uiohook libuiohook
    HINTS ${PC_UIOHOOK_LIBRARY_DIRS} ${UIOHOOK_ROOT} $ENV{UIOHOOK_ROOT}
    PATH_SUFFIXES lib lib64 bin
)

# --- fall back to the installed package config ------------------------------

if(NOT (UIOHOOK_INCLUDE_DIR AND UIOHOOK_LIBRARY))
    find_package(uiohook QUIET CONFIG)

    set(_uiohook_config_target "")
    if(TARGET uiohook::uiohook)
        set(_uiohook_config_target uiohook::uiohook)
    elseif(TARGET uiohook)
        set(_uiohook_config_target uiohook)
    endif()

    if(_uiohook_config_target)
        # Recover the real paths from the imported target so that everything
        # downstream — the dispatch-signature probe, the Windows DLL copy, the
        # configuration summary — works the same on this path as on the other.
        get_target_property(_uiohook_cfg_inc ${_uiohook_config_target}
                            INTERFACE_INCLUDE_DIRECTORIES)
        if(_uiohook_cfg_inc AND NOT UIOHOOK_INCLUDE_DIR)
            list(GET _uiohook_cfg_inc 0 UIOHOOK_INCLUDE_DIR)
        endif()

        foreach(_cfg IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_RELWITHDEBINFO
                     IMPORTED_LOCATION_DEBUG IMPORTED_LOCATION)
            if(NOT UIOHOOK_LIBRARY)
                get_target_property(_uiohook_cfg_lib ${_uiohook_config_target} ${_cfg})
                if(_uiohook_cfg_lib)
                    set(UIOHOOK_LIBRARY "${_uiohook_cfg_lib}")
                endif()
            endif()
        endforeach()

        # On Windows the imported location of a shared library is the DLL and
        # the import library lives in IMPORTED_IMPLIB; that is what we must link.
        get_target_property(_uiohook_implib ${_uiohook_config_target} IMPORTED_IMPLIB)
        if(_uiohook_implib)
            set(UIOHOOK_LIBRARY "${_uiohook_implib}")
        endif()

        add_library(UIOHook::UIOHook ALIAS ${_uiohook_config_target})
        set(UIOHOOK_FOUND TRUE)
        set(UIOHOOK_INCLUDE_DIRS "${UIOHOOK_INCLUDE_DIR}")
        # An imported target is a valid link item, and using it here keeps the
        # target's own interface (Advapi32 on Windows, X11 on Linux) attached.
        set(UIOHOOK_LIBRARIES ${_uiohook_config_target})
        return()
    endif()
endif()

# --- report -----------------------------------------------------------------

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UIOHook
    REQUIRED_VARS UIOHOOK_LIBRARY UIOHOOK_INCLUDE_DIR
    FAIL_MESSAGE
        "libuiohook not found. Build it from https://github.com/kwhat/libuiohook \
(it is not in vcpkg) and point -DUIOHOOK_ROOT=<install prefix> at it. See BUILD.md."
)

if(UIOHOOK_FOUND)
    set(UIOHOOK_INCLUDE_DIRS ${UIOHOOK_INCLUDE_DIR})
    set(UIOHOOK_LIBRARIES ${UIOHOOK_LIBRARY})

    add_library(UIOHook::UIOHook UNKNOWN IMPORTED)
    set_target_properties(UIOHook::UIOHook PROPERTIES
        IMPORTED_LOCATION "${UIOHOOK_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${UIOHOOK_INCLUDE_DIR}"
    )
    if(WIN32)
        # libuiohook links Advapi32 for the registry lookups it does for the
        # repeat rate; an UNKNOWN IMPORTED target carries no interface of its
        # own, so say so here.
        set_property(TARGET UIOHook::UIOHook APPEND PROPERTY
                     INTERFACE_LINK_LIBRARIES Advapi32)
    endif()
endif()

mark_as_advanced(UIOHOOK_INCLUDE_DIR UIOHOOK_LIBRARY)
