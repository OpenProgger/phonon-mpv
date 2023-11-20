if(${PHONON_VERSION} VERSION_GREATER "4.9.50")
    message(STATUS "Building against Phonon 4.10 API")
    set(BACKEND_VERSION_DEFINE -DPHONON_BACKEND_VERSION_4_10)
endif()

add_definitions(${BACKEND_VERSION_DEFINE})

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

add_library(phonon_mpv MODULE ${phonon_mpv_SRCS})

target_link_libraries(phonon_mpv
    Phonon::phonon4qt${QT_MAJOR_VERSION}
    ${MPV_LIBRARIES}
)

add_definitions(-DWAYLAND_SUPPORT)
add_definitions(-DX11_SUPPORT)
if(QT_MAJOR_VERSION STREQUAL "5")
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    target_link_libraries(phonon_mpv Qt::Gui Qt::X11Extras)
else()
    target_link_libraries(phonon_mpv Qt::Core Qt::Gui Qt::OpenGLWidgets)
endif()

if(PHONON_EXPERIMENTAL)
    target_link_libraries(phonon_mpv Phonon::phonon4qt${QT_MAJOR_VERSION}experimental)
endif()

install(TARGETS phonon_mpv DESTINATION ${PHONON_BACKEND_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon-mpv.json.in
                ${CMAKE_CURRENT_BINARY_DIR}/phonon-mpv.json @ONLY)

