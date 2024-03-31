# phonon-mpv
Phonon Backend using MPV Player(libmpv)

## Definition
This is a fork of phonon-vlc, rewritten to work with libmpv instead of libVLC.
libmpv supports less features than VLC but they are only related to memory streams and audio/video dumps.
This backend should be an lightweight alternative to libVLC with less dependencies,

## Configuration
During startup this backend searches for a config file at ``~/.config/Phonon/mpv.conf``.  
It's the same that mpv itself uses so copy/symlink a existing mpv config is possible and allows to control properties that are not touched by this backend(e.g. ``hwdec`` etc.).

## Requirements
- cmake >= 3.5
- Phonon >= 4.11
- Qt5 or Qt6
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
