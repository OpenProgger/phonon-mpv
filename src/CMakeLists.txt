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

    audio/audiooutput.h
    audio/audiodataoutput.h
    audio/volumefadereffect.h
    backend.h
    effect.h
    effectmanager.h
    mediacontroller.h
    mediaobject.h
    sinknode.h
    video/videowidget.h
    utils/debug.h
)

ecm_create_qm_loader(phonon_mpv_SRCS phonon_mpv_qt)
add_library(phonon_mpv_qt${QT_MAJOR_VERSION} MODULE ${phonon_mpv_SRCS})
if(QT_MAJOR_VERSION STREQUAL 5)
    set_target_properties(phonon_mpv_qt${QT_MAJOR_VERSION} PROPERTIES INSTALL_NAME "phonon_mpv")
endif()

target_link_libraries(phonon_mpv_qt${QT_MAJOR_VERSION}
    Phonon::phonon4qt${QT_MAJOR_VERSION}
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    ${MPV_LIBRARIES}
)

if(QT_MAJOR_VERSION STREQUAL "5")
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    if(Qt5X11Extras_FOUND)
        add_definitions(-DX11_SUPPORT)
        target_link_libraries(phonon_mpv_qt${QT_MAJOR_VERSION} Qt${QT_MAJOR_VERSION}::X11Extras)
    endif()
else()
    target_link_libraries(phonon_mpv_qt${QT_MAJOR_VERSION} Qt${QT_MAJOR_VERSION}::OpenGLWidgets)
    if(Qt6XcbQpaPrivate_FOUND)
        add_definitions(-DX11_SUPPORT)
    endif()
endif()

if(PHONON_EXPERIMENTAL)
    target_link_libraries(phonon_mpv_qt${QT_MAJOR_VERSION} Phonon::phonon4qt${QT_MAJOR_VERSION}experimental)
endif()

install(TARGETS phonon_mpv_qt${QT_MAJOR_VERSION} DESTINATION ${PHONON_BACKEND_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon-mpv.json.in
                ${CMAKE_CURRENT_BINARY_DIR}/phonon-mpv.json @ONLY)

