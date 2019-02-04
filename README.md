# phonon-mpv
Phonon Backend using MPV Player(libmpv)

## Definition
This is a fork of phonon-vlc, rewritten to work with libmpv instead of libVLC.
libmpv supports less features than VLC but they are only related to memory streams and audio/video dumps.
This backend should be an lightweight alternative to libVLC with less dependencies,

## Requirements
- cmake
- Phonon >= 4.9
- Qt5
- libmpv >= 0.29

## Build and Install
Run this commands as root (or with sudo):

```
  # mkdir build
  # cd build
  # cmake -DCMAKE_INSTALL_PREFIX=/usr <source directory>
  # make
  # make install
```
