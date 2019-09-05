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
    Phonon::phonon4qt5
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

if(PHONON_EXPERIMENTAL)
    target_link_libraries(phonon_mpv Phonon::phonon4qt5experimental)
endif()

install(TARGETS phonon_mpv DESTINATION ${PHONON_BACKEND_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon-mpv.json.in
                ${CMAKE_CURRENT_BINARY_DIR}/phonon-mpv.json @ONLY)

