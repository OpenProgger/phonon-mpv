cmake_minimum_required(VERSION 2.8.9)
project(PhononMPV)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(PHONON_BUILD_PHONON4QT5 ON)
set(CMAKE_CXX_STANDARD 17)

# Phonon
find_package(Phonon REQUIRED)
macro_ensure_version("4.7.0" ${PHONON_VERSION} PHONON_FOUND)
macro_log_feature(PHONON_FOUND "Phonon" "Core Library" "git://anongit.kde.org/phonon.git" TRUE "4.7.0")

# LibMPV
find_package(PkgConfig)
pkg_check_modules(MPV REQUIRED mpv>=1.101.0)
macro_log_feature(MPV_FOUND "LibMPV" "MPV C API Library" "http://github.com/mpv-player/mpv" TRUE "1.101.0")

find_package(Qt5X11Extras)
find_package(Qt5Gui)

if(PHONON_FOUND_EXPERIMENTAL)
  add_definitions(-DPHONON_EXPERIMENTAL)
else(PHONON_FOUND_EXPERIMENTAL)
  add_definitions(-DPHONON_NO_EXPERIMENTAL)
endif(PHONON_FOUND_EXPERIMENTAL)

add_definitions(${QT_DEFINITIONS})

include_directories(${PHONON_INCLUDES} ${QT_INCLUDES} ${MPV_INCLUDE_DIR})

set(PHONON_MPV_MAJOR_VERSION "0")
set(PHONON_MPV_MINOR_VERSION "0")
set(PHONON_MPV_PATCH_VERSION "6")
set(PHONON_MPV_VERSION "${PHONON_MPV_MAJOR_VERSION}.${PHONON_MPV_MINOR_VERSION}.${PHONON_MPV_PATCH_VERSION}")
add_definitions(-DPHONON_MPV_VERSION="${PHONON_MPV_VERSION}")

add_subdirectory(src)

macro_display_feature_log()
