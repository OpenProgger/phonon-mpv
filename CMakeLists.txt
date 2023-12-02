cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(PhononMPV VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)

option(PHONON_BUILD_QT5 "Build for Qt5" ON)
option(PHONON_BUILD_QT6 "Build for Qt6" OFF)

set(VERSIONS)
if(PHONON_BUILD_QT5)
    list(APPEND VERSIONS "5")
endif()
if(PHONON_BUILD_QT6)
    list(APPEND VERSIONS "6")
    set(BUILD_WITH_QT6 ON) # necessary on older ECMs
endif()

set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)
set(QT_NO_CREATE_VERSIONLESS_FUNCTIONS ON)

include(FeatureSummary)

# ECM
find_package(ECM 5.90 NO_MODULE)
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set_package_properties(ECM PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "Extra CMake Modules"
    URL "https://api.kde.org/frameworks/extra-cmake-modules/html/index.html")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

# LibMPV
find_package(PkgConfig)
pkg_check_modules(MPV REQUIRED mpv>=1.101.0)

add_definitions(-DPHONON_MPV_VERSION="${PROJECT_VERSION}")
include_directories(${MPV_INCLUDE_DIR})

foreach(version ${VERSIONS})
    set(QT_MAJOR_VERSION ${version})

    include(KDEInstallDirs${QT_MAJOR_VERSION})
    include(ECMPoQmTools)
    include(KDECMakeSettings)
    include(KDECompilerSettings)
    include(ECMSetupVersion)

    find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Gui)
    if(QT_MAJOR_VERSION STREQUAL "5")
        find_package(Qt5X11Extras)
    else()
        find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS OpenGLWidgets)
        find_package(Qt6XcbQpaPrivate)
    endif()

    # Phonon
    find_package(Phonon4Qt${QT_MAJOR_VERSION} 4.10.60 NO_MODULE)
    set_package_properties(Phonon4Qt${QT_MAJOR_VERSION} PROPERTIES
        TYPE REQUIRED
        DESCRIPTION "Phonon core library"
        URL "https://api.kde.org/phonon/html/index.html")

    find_package(Phonon4Qt${QT_MAJOR_VERSION}Experimental 4.10.60 NO_MODULE)
    set_package_properties(Phonon4Qt${QT_MAJOR_VERSION}Experimental PROPERTIES
        TYPE OPTIONAL
        DESCRIPTION "Phonon experimental library"
        URL "https://api.kde.org/phonon/html/index.html")
    if(Phonon4Qt${QT_MAJOR_VERSION}Experimental_FOUND)
        set(PHONON_EXPERIMENTAL TRUE)
    endif()

    ecm_setup_version(PROJECT VARIABLE_PREFIX PHONON_MPV)
    add_subdirectory(src src${version})
    unset(QUERY_EXECUTABLE CACHE)
endforeach()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
