cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(PhononMPV VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)

include(FeatureSummary)

# ECM
find_package(ECM 5.60 NO_MODULE)
set_package_properties(ECM PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "Extra CMake Modules"
    URL "https://api.kde.org/frameworks/extra-cmake-modules/html/index.html")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(ECMPoQmTools)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMSetupVersion)

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

# LibMPV
find_package(PkgConfig)
pkg_check_modules(MPV REQUIRED mpv>=1.101.0)

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Gui)
if(QT_MAJOR_VERSION STREQUAL "5")
    find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS X11Extras)
else()
    find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS OpenGLWidgets)
endif()

add_definitions(-DPHONON_MPV_VERSION="${PROJECT_VERSION}")
include_directories(${MPV_INCLUDE_DIR})
ecm_setup_version(PROJECT VARIABLE_PREFIX PHONON_MPV)

add_subdirectory(src)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
