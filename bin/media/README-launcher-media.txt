NAUTITECH launcher - intro film, menu background, music and sounds
====================================================================

Already here:
  sounds\ui_*.wav        The menu sounds. Listen to them, replace them with your own
                         (same names), or delete them to get the built-in ones back.
  fonts\barlow-condensed The menu lettering.

To add yourself (they are your own films and music, so none of them are supplied):
put them in this "media" folder (the one next to the launcher), with exactly these
names. Every one is optional: while a file is missing, the launcher simply goes on
without it (no film: the menu opens at once; no music: silence).

  intro.mp4              Intro film, played full screen when the launcher starts.
                         Any key or a mouse click skips it (it fades out in half a second).
                         Without this file the menu opens straight away.
  menu_background.mp4    Film looped behind the menu, without sound
                         (for example 10 to 30 seconds of sea, bridge or radar views).
  menu_background.jpg    Still picture behind the menu (it moves slowly), used when there
    (or .png)            is no menu_background.mp4. Without it: bc5.ini launcher_image,
                         else bg_main.png. A picture without text or logos looks best,
                         the menu draws its own titles over it.
  launcher_logo.png      Logo at the top left of the menu (transparent PNG, white on
                         transparent looks best). Without it: logo_nautitech.png.
  menu_music.mp3         Music looped while the menu is shown. It stops while the
    (or .m4a .wav .wma)  launcher is behind an exercise.
  sounds/ui_hover.wav    Moving onto a menu item (short: 30 to 80 ms)
  sounds/ui_select.wav   Choosing an item (200 to 600 ms)
  sounds/ui_back.wav     Going back, Escape
  sounds/ui_open.wav     The menu appears (after the intro)
  sounds/ui_launch.wav   An application is started
                         Sounds: WAV, PCM 16 bit (or 8 bit), mono or stereo, 44.1 or 48 kHz.

Recommended format for the films
--------------------------------
  File        MP4 (.mp4)
  Video       H.264 (also called AVC), High profile, 8 bit, 4:2:0 (yuv420p)
              Not H.265/HEVC, AV1 or VP9: Windows needs paid or optional extensions
              to decode them, so they would not play on every PC.
  Picture     1920 x 1080 (best choice for most desks), or 2560 x 1440 for very large
              screens. Keep 16:9, like the screens. The film is scaled to fill the
              screen (black bars if its shape differs). 4K (3840 x 2160) works on strong
              PCs, but costs much more processor time for little visible gain.
  Frame rate  30 or 25 frames per second (up to 60 for 1080p).
  Bit rate    1080p: 8 to 15 Mbit/s. 1440p: 15 to 25 Mbit/s.
              (Or constant quality: CRF 18 to 20 with x264.)
  Sound       AAC-LC, 48 kHz (or 44.1), stereo, 192 to 256 kbit/s (intro only).
  Length      Intro: 20 to 60 seconds. Background loop: 10 to 30 seconds, with the
              last picture close to the first so that the loop does not jump.
  Disk size   About 60 to 110 MB per minute at 1080p.

Converting with ffmpeg (free, https://ffmpeg.org):

  Intro, with sound:
    ffmpeg -i my_film.mov -c:v libx264 -profile:v high -pix_fmt yuv420p -preset slow -crf 19
           -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2"
           -r 30 -c:a aac -b:a 192k -ar 48000 -movflags +faststart intro.mp4

  Menu background loop, no sound: the same, with "-an" in place of "-c:a aac -b:a 192k -ar 48000",
  and menu_background.mp4 as the name.

  From Adobe Premiere / Media Encoder, DaVinci Resolve or HandBrake: H.264, MP4, 1080p,
  30 fps, AAC audio ("YouTube 1080p" presets are a good starting point).

Options and troubleshooting
---------------------------
  In the launcher, PARAMETRES: intro film on/off, interface sounds, menu music,
  full screen. They are kept in launcher.ini, in the user folder
  (%APPDATA%\Simulateur de Navigation Maritime on Windows).
  If a film or the music does not play, the reason is written in launcher.log in the
  same folder.
  F11 switches between full screen and a window.
  Command line: --no-intro (no film this time), --windowed (in a window this time).

Fonts: media/fonts/barlow-condensed (Barlow Condensed, SIL Open Font License, see OFL.txt).
