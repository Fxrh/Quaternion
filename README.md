# Quaternion

<a href='https://matrix.org'><img src='https://matrix.org/docs/projects/images/made-for-matrix.png' alt='Made for Matrix' height=64 target=_blank /></a>

[![license](https://img.shields.io/github/license/quotient-im/quaternion.svg)](https://github.com/quotient-im/Quaternion/blob/master/COPYING)
![status](https://img.shields.io/badge/status-beta-yellow.svg)
[![release](https://img.shields.io/github/release/quotient-im/quaternion/all.svg)](https://github.com/quotient-im/Quaternion/releases/latest)
[![](https://img.shields.io/matrix/quotient:matrix.org.svg)](https://matrix.to/#/#quotient:matrix.org)
[![](https://img.shields.io/cii/percentage/1663.svg?label=CII%20best%20practices)](https://bestpractices.coreinfrastructure.org/projects/1663/badge)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/quotient-im/Quaternion.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/quotient-im/Quaternion/context:cpp)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

Quaternion is a cross-platform desktop IM client for the [Matrix](https://matrix.org) protocol. This file contains general information about application usage and settings. See [BUILDING.md](./BUILDING.md) for building instructions.

## Contacts
Most of talking around Quaternion happens in the room of its parent project,
Quotient: [#quotient:matrix.org](https://matrix.to/#/#quotient:matrix.org).
`#quaternion:matrix.org` currently points to the old version of the Quotient
room - if you ended up there, just go to the upgraded room instead
(using Quaternion 0.0.9.4 or later, or Riot).

You can file issues at
[the project's issue tracker](https://github.com/quotient-im/Quaternion/issues).
If you find what looks like a security issue, please use instructions
in SECURITY.md.

## Download and install
The latest release (with links to cross-platform source code archives, as well
as archived binaries for Windows and macOS) can be found on the
[GitHub Releases page](https://github.com/quotient-im/Quaternion/releases/latest).

For those who want to run the bleeding edge code, automatic builds from
continuous integration pipelines can be found in the
[Quaternion CI repo at bintray](https://bintray.com/quotient/ci/Quaternion/#files).
All these builds come with fairly recent Qt bundled. Beware: these builds may
tell you obscenities in your local language, steal your smartphone and
share your private photos. Jokes aside: unless you are ready for very bad and
sweeping surprises, do NOT run those. Before you do, make sure you have your
backups in order.

If you want to build Quaternion from sources, see [BUILDING.md](./BUILDING.md).
Packagers are still scarce, so please step up and support your favourite system!

### Requirements
Quaternion 0.0.9.5 packages on Linux need Qt version 5.10 or higher;
Debian Buster, Ubuntu Disco Dingo and Fedora 28 are new enough.
For Windows, macOS, AppImage, Flatpak all needed Qt libraries are included
in the packages. On Windows, you need to separately install OpenSSL
(see the next section).

#### Windows
Since we can't rely on package management on Windows, Qt libraries and
a C++ runtime are packaged/installed together with Quaternion. However,
OpenSSL libraries are not installed automatically because of export restrictions.
Unless you already have them around (e.g., they are a part of any
Qt development installation), you should install OpenSSL yourself.
[OpenSSL's Wiki](https://wiki.openssl.org/index.php/Binaries) lists a few links
to OpenSSL installers. They come in different build configurations; Quaternion
archives need OpenSSL made with/for Visual Studio (not MinGW), and the version
should be chosen as follows:
* If the Quaternion archive is produced before 11-08-2019 (including older
  CI builds and releases 0.0.9.4 and older), download OpenSSL 1.0.x; be aware
  that this version is only supported until the end of 2019.
* If the archive is made on 11-08-2019 or later, download OpenSSL 1.1.x.

The deciding point is the used Qt version. To avoid any doubt, go to the folder
you unpacked Quaternion to and check the version of Qt5Network.dll (it's shown
in "Properties" dialog box that you can get from the context menu). If it's
Qt 5.12.3 or older, you need OpenSSL 1.0; if it's newer, take OpenSSL 1.1.

#### macOS
You can download the latest release from [GitHub](https://github.com/quotient-im/Quaternion/releases/latest).

Alternatively, you can install Quaternion from [Homebrew Cask](https://brew.sh)
```
brew cask install quaternion
```

#### Linux and others
Quaternion is packaged for many distributions, including various versions of
Debian, Ubuntu and OpenSUSE, as well as Arch Linux, NixOS and FreeBSD.
A pretty comprehensive list can be found at
[Repology](https://repology.org/project/quaternion/versions).

Flatpaks for Quaternion are available from Flathub. To install, use:
```
flatpak install https://flathub.org/repo/appstream/com.github.quaternion.flatpakref
```
While generally working well, Flatpak support is still a bit experimental.
Please file issues at https://github.com/flathub/com.github.quaternion
if you believe there's a problem specific to Flatpak.

## Running
Just start the executable in your most preferred way - either from the build
directory or from the installed location. If you're interested in tweaking
configuration beyond what's available in the UI, read the "Configuration"
section further below.

## Translation
Quaternion uses [Lokalise.co](https://lokalise.co) for the translation effort.
It's easy to participate:
[join the project at Lokalise.co](https://lokalise.co/public/730769035bbc328c31e863.62506391/),
ask to add your language (either in #quotient:matrix.org or in
the Lokalise project chat) and start translating! Many languages are still
longing for contributors.

## Configuration
The only non-trivial command-line option available so far is `--locale` - it
allows you to override the locale Quaternion uses (an equivalent of setting
`LC_ALL` variable on UNIX-based systems). As of version 0.0.9.3, Quaternion has
no translations to other languages, so you will hardly notice the difference in
messages; number and date formats will be following the setting though. Version
0.0.9.4 gains German, Polish, and Russian translations.

Quaternion stores its configuration in a way standard for Qt applications. It will read and write the configuration in the user-specific location (creating it if non-existent) and will only read the system-wide location with reasonable defaults if the configuration is nowhere to be found.

- Linux:
  - system-wide: `$XDG_CONFIG_DIR/Quotient/quaternion` or
    `/etc/xdg/Quotient/quaternion`
  - user-specific: `$HOME/.config/Quotient/quaternion.conf`
- macOS:
  - system-wide: `/Library/Preferences/im.quotient.quaternion.plist`
  - user-specific: `$HOME/Library/Preferences/im.quotient.quaternion.plist`
- Windows: registry keys under
  - system-wide: `HKEY_LOCAL_MACHINE\Software\Quotient\quaternion`
  - user-specific: `HKEY_CURRENT_USER\Software\Quotient\quaternion`

ALL settings listed below reside in `UI` section of the configuration file
or (for Windows) registry.

Some settings exposed in the user interface (Settings and View menus):

- `notifications` - a general setting whether Quaternion should distract
  the user with notifications and how.
  - `none` suppresses notifications entirely (rooms and messages are still
    hightlighted but the tray icon is muted);
  - `non-intrusive` allows the tray icon show notification popups;
  - `intrusive` (default) adds to that activation of Quaternion window
    (i.e. the application blinking in the task bar, or getting raised,
    or otherwise demands attention in an environment-specific way).
- `timeline_layout` - this allows to choose the timeline layout. If this is
  set to "xchat", Quaternion will show the author to the left of each message,
  in a xchat/hexchat style (this was also the only available layout on
  Quaternion before 0.0.9.2). Any other value will select the "default" layout,
  with author labels above blocks of messages.
- `use_shuttle_dial` - Quaternion will use a shuttle dial instead of
  a classic scrollbar for the timeline's vertical scrolling control. Shuttle
  dials usually control change velocity instead of value; in this case,
  moving the dial away from the neutral position increases the speed of
  scrolling. This is more convenient if you need to quickly move around without
  knowing position relative to the edges, as is the case of a Matrix timeline;
  however, the control is unusual and not all people like it. Shuttle scrollbar
  is enabled by default; set this to false (or 0) to use the classic scrollbar.
- `autoload_images` - whether full-size images should be loaded immediately
  once the message is shown on the screen. The default is to automatically load
  full-size images; set this to false (or 0) to disable that and only load
  a thumbnail initially.
- `show_noop_events` - set this to 1 to show state events that do not alter
  the state (you'll see "(repeated)" next to most of those).
- `RoomsDock/tags_order` - allows to alter the order of tags in the room
  list. This is a comma-separated list of tags/namespaces;
  a few characters have special meaning as described below.If a tag is
  not mentioned and does not fit any namespace, it will be put at the end of
  the room list in lexicographic order. Tags within the same namespace are
  also ordered lexicographically.
  
  `.*` (only recognised at the end of the string) means the whole namespace,
  other strings are treated as fully specified tags.
  
  `-` in front of the tag/namespace means it should not be used for grouping;
  e.g., if you don't want People group you can add `-im.quotient.direct`
  anywhere in the list. `im.quotient.none` ("Rooms") always exists and
  cannot be disabled. 
  
  The default tags order is as follows:
  `m.favourite,u.*,im.quotient.direct,im.quotient.none,m.lowpriority`,
  meaning: Favourites, followed by all user custom tags, then People,
  rooms with no enabled tags (the "Rooms" group) and finally Low priority
  rooms. If Quaternion doesn't find the setting in the configuration it
  will write down this line to the configuration so that you don't need
  to enter it from scratch.

Settings not exposed in UI:

- `condense_chat` - set this to 1 to make the timeline rendered tighter,
  eliminating vertical gaps between messages as much as possible.
- `show_author_avatars` - set this to 1 (or true) to show author avatars in
  the timeline (default if the timeline layout is set to default); setting this
  to 0 (or false) will suppress avatars (default for the XChat timeline layout).
- `suppress_local_echo` - set this to 1 (or true) to suppress showing local
  echo (events sent from the current application but not yet confirmed by
  the server). By default local echo is shown.
- `animations_duration_ms` - defines the base duration (in milliseconds) of
  animation effects in the timline. The default is 400; set it to 0 to disable
  animation.
- `outgoing_color` - set this to the color name you prefer for text you sent;
  HTML color names and SVG `#codes` are supported; by default it's `#204A87`
  (navy blue).
- `highlight_color` - set this to the color name you prefer for highlighted
  rooms/messages; HTML color names and SVG `#codes` are supported;
  by default it's `orange`.
- `highlight_mode` - set this to `text` if you prefer to use the highlight
  color as the text color (the only option available until 0.0.9.3); the new
  default is to use the background for highlighting.
- `use_human_friendly_dates` - set this to false (or 0) if you do NOT want
  usage of human-friendly dates ("Today", "Monday" instead of the standard
  day-month-year triad) in the UI; the default is true.
- `quote_style` - the quote template. The `\\1` means the quoted string.
  By default it's `> \\1\n`.
- `quote_regex` - set to `^([\\s\\S]*)` to add `UI/quote_style` only at
  the beginning and end of the quote. By default it's `(.+)(?:\n|$)`.
- `Fonts/render_type` - select how to render fonts in Quaternion timeline;
  possible values are "NativeRendering" (default) and "QtRendering".
- `Fonts/family` - override the font family for the whole application.
  If not specified, the default font for your environment is used.
- `Fonts/pointSize` - override the font size (in points) for the whole
  application. If not specified, the default size for your environment is used.
- `Fonts/timeline_family` - font family (for example `Monospace`) to
  display messages in the timeline. If not specified, the application-wide font
  family is used.
- `Fonts/timeline_pointSize` - font size (in points) to display messages
  in the timeline. If not specified, the application-wide point size is used.
- `maybe_read_timer` - threshold time interval in milliseconds for a displayed
  message to be considered as read.
- `use_keychain` - set this to false (or 0) if you explicitly do NOT want to
  use keychain but prefer to store access token in a dedicated file instead (see
  the next paragraph); the default is true.
- `hyperlink_users` - set this to false (or 0) if you do NOT want to
  hyperlink matrix user IDs in messages. By default it's true.
- `auto_markdown` - since version 0.0.9.5, and only if built with Qt 5.14
  or newer (builds for Windows at GitHub, as well as Flatpaks, use Qt 5.14),
  setting this to `true` makes Quaternion parse all messages that you send
  as Markdown, without having to prepend them with `/md` command. By default,
  this is `false` now since it cannot be enabled in builds with older Qt and
  since the whole functionality is experimental. If you have it enabled, feel
  free to submit bug reports at the usual place.

Since version 0.0.9.5, all Quaternion binaries at GitHub are compiled with
Qt Keychain support. It means that Quaternion will try to store your access
token(s) in the secure storage configured for your platform. If the storage or
Qt Keychain are not available, Quaternion will try to store your access token(s)
in a dedicated file with restricted access rights so that only the owner can
access them (this doesn't really work on Windows - see below) and with the name
matching your user id in the following directory:

- Linux: `$HOME/.local/share/Quotient/quaternion`
- macOS: `$HOME/Library/Application Support/Quotient/quaternion`
- Windows: `%LOCALAPPDATA%/Quotient/quaternion`

Unfortunately, Quaternion cannot enforce proper access rights on Windows;
you'll see a warning about it and will be able to either refuse saving your
access token in that case or agree and setup file permissions outside Quaternion.

Quaternion caches the rooms state and user/room avatars on the file system
in a conventional location for your platform, as follows:

- Linux: `$HOME/.cache/Quotient/quaternion`
- macOS: `$HOME/Library/Cache/Quotient/quaternion`
- Windows: `%LOCALAPPDATA%/Quotient/quaternion/cache`

Cache files are safe to delete at any time but Quaternion only looks for them
when starting up and overwrites them regularly while running; so it only
makes sense to delete cache files when Quaternion is not running. If Quaternion
doesn't find or cannot fully load cache files at startup it downloads
the whole state from Matrix servers. It tries to optimise this process by
lazy-loading if the server supports it; in an unlucky case when the server
cannot do lazy-loading, initial sync can take much time (up to a minute and
even more, depending on the number of rooms and the number of users in them).
Deleting cache files may help with problems such as missing avatars,
rooms stuck in a wrong state etc.

## Troubleshooting

libQuotient has its own section on troubleshooting - make sure to look into its README.md too.

#### Continuously reconnecting though the network is fine
If Quaternion starts displaying the message that it couldn't connect to the server and retries more than a couple of times without success, while you're sure you have the network connection - double-check that you don't have Qt bearer management libraries around, as they cause issues with some WiFi networks. To do that, try to find "bearer" directory where your Qt is installed (on Windows it's next to Quaternion executable; on Linux it's a part of Qt installation, usually in `/usr/lib/qt5/plugins`). Then delete or rename it (on Windows) or delete the package that this directory is in (on Linux).

#### No messages in the timeline
If Quaternion runs but you can't see any messages in the chat (though you can
type them in) - you have either of two problems with Qt Quick (or if you are
extremely unlucky, both):

- You might not have Qt Quick libraries and/or plugins installed. On Linux,
  this may be a case when you are not using the official packages for your
  distro. Check the stdout/stderr logs, they are quite clear in such cases.
  On Windows and Mac, just open an issue (see "Contacts" in the beginning of
  this README), because most likely not all necessary Qt parts were installed
  along with Quaternion (which is a bug).
- If the logs confirm that QML is up and running but there's still nothing
  for the timeline, you might have hit an issue with QML view stacking order,
  such as #355/#356. If you use Qt 5.12 or newer (as is the case on Windows
  and macOS recently), please file a bug: it should not happen with
  recent Qt at all. If you are on Linux and have to use older Qt, you have
  to build Quaternion from sources, passing `-DUSE_QQUICKWIDGET=ON` to CMake.
  Note that it's prone to crashing on some platforms so it's best to still
  find a way to run Quaternion with Qt 5.12 (using AppImage, e.g.).

#### SSL problems
Especially on Windows, if Quaternion starts up but upon an attempt to connect
returns a message like "Failed to make SSL context" - correct SSL libraries
are not reachable by the Quaternion binary. Re-read the chapter "Requirements",
section "Windows" in the beginning of this file and do as it advises. Make sure
you use correct version OpenSSL.

#### DLL hell
If you have troubles with dynamic libraries on Windows, [the Dependencies Walker tool aka depends.exe](http://www.dependencywalker.com/) helps a lot in navigating the DLL hell - especially when you have a mixed 32/64-bit environment or have different versions of the same library scattered around. OpenSSL, in particular, is notoriously often dragged along by all kinds of software; and you may have other copies of Qt around which you didn't even know about - e.g., with CMake GUI.

#### Logging
If you run Quaternion from a console on Windows and want to see log messages,
set `QT_LOGGING_TO_CONSOLE=1` so that the output is redirected to the console.

When chasing bugs and investigating crashes, it helps to increase the debug level. Thanks to [@eang:matrix.org](https://matrix.to/#/@eang:matrix.org]), libQuotient uses Qt logging categories - the "Troubleshooting" section of the library's `README.md` elaborates on how to setup logging. Note that Quaternion itself doesn't use Qt logging categories yet, only the library does.

You may also want to set `QT_MESSAGE_PATTERN` to make logs slightly more informative (see https://doc.qt.io/qt-5/qtglobal.html#qSetMessagePattern for the format description). My (@kitsune's) `QT_MESSAGE_PATTERN` looks as follows:
`%{time h:mm:ss.zzz}|%{category}|%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}|%{message}` (the scary `%{if}`s are just encoding the logging level into its initial letter).

## Screenshot
![Screenshot](quaternion.png)
