include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DPHONON_LIB_INSTALL_DIR="${LIB_INSTALL_DIR}")

if(${PHONON_VERSION} VERSION_GREATER "4.9.50")
    message(STATUS "Building against Phonon 4.10 API")
    set(BACKEND_VERSION_DEFINE -DPHONON_BACKEND_VERSION_4_10)
elseif(${PHONON_VERSION} VERSION_GREATER "4.8.50")
    message(STATUS "Building against Phonon 4.9 API")
    set(BACKEND_VERSION_DEFINE -DPHONON_BACKEND_VERSION_4_9)
elseif(${PHONON_VERSION} VERSION_GREATER "4.6.50")
    message(STATUS "Building against Phonon 4.7 API")
    set(BACKEND_VERSION_DEFINE -DPHONON_BACKEND_VERSION_4_7)
elseif(${PHONON_VERSION} VERSION_GREATER "4.1.99")
    message(STATUS "Building against Phonon 4.2 API")
    set(BACKEND_VERSION_DEFINE -DPHONON_BACKEND_VERSION_4_2)
else()
    message(STATUS "Building against Phonon 4.0 API")
    set(BACKEND_VERSION_DEFINE)
endif()

add_definitions(${BACKEND_VERSION_DEFINE})
set(AUTOMOC_MOC_OPTIONS ${BACKEND_VERSION_DEFINE})

set(phonon_mpv_SRCS
    audio/audiooutput.cpp
    audio/audiodataoutput.cpp
    audio/volumefadereffect.cpp
    backend.cpp
    effect.cpp
    effectmanager.cpp
    mediacontroller.cpp
    mediaobject.cpp
    sinknode.cpp
    video/videowidget.cpp
    utils/debug.cpp
)

add_definitions(-DPHONON_NO_GRAPHICSVIEW)

automoc4_add_library(phonon_mpv MODULE ${phonon_mpv_SRCS})
qt5_use_modules(phonon_mpv Core Gui Widgets)

set_target_properties(phonon_mpv PROPERTIES
    PREFIX ""
    AUTOMOC_MOC_OPTIONS ${AUTOMOC_MOC_OPTIONS}
)
target_link_libraries(phonon_mpv
    ${PHONON_LIBRARY}
    ${MPV_LIBRARIES}
)

if(Qt5Gui_FOUND)
    add_definitions(-DWAYLAND_SUPPORT)
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    target_link_libraries(phonon_mpv Qt5::Gui)
endif()

if(Qt5X11Extras_FOUND)
    add_definitions(-DX11_SUPPORT)
    target_link_libraries(phonon_mpv Qt5::X11Extras)
endif()

install(TARGETS phonon_mpv DESTINATION ${BACKEND_INSTALL_DIR})

if(PHONON_BUILD_PHONON4QT5)
    # Instead of desktop files we are embedding the information into the plugin itself.
    # We have no KDE technology to help with finding the actual libraries anyway, so
    # we need to have the library path anyway.
    # Also see qtplugin/Q_PLUGIN_METADATA documentation.
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon-mpv.json.in
                   ${CMAKE_CURRENT_BINARY_DIR}/phonon-mpv.json @ONLY)
else()
    message(FATAL_ERROR "Only Qt5 is supported")
endif()

