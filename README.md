# purple-line

libpurple (Pidgin, Finch) protocol plugin for [LINE](http://line.me/).

## WARNING: Line Corporation is currently banning people for using 3rd party clients

There are multiple reports of accounts being permanently banned for using 3rd party clients such
as Pidgin. What exactly triggers the ban is unknown, but it's probably best to stick to the
official clients now.

For this reason I've broken the build on purpose. If you still want to test purple-line, you
should be able to figure out how to fix it yourself. Use at your own risk.

## Install from source (any distribution)

Make sure you have the required prerequisites installed:

* libpurple - Library that provides the core functionality of Pidgin and other compatible clients.
  Probably available from your package manager.
* thrift - Apache Thrift compiler. May be available from your package manager.
* libthrift - Apache Thrift C++ library. May be available from your package manager.
* libgcrypt - Crypto library. Probably available from your package manager.

To install the plugin system-wide, run:

    make
    sudo make install

The makefile supports a flag THRIFT_STATIC=true which causes it to download and build a version of
Thrift and statically link it. This should be convenient for people using one of the numerous
distributions that do not package Thrift.

You can also install the plugin for your user only by replacing `install` with `user-install`.

### Install on Windows

Pre-built binaries for Windows are available at [Eion Robb's website](http://eion.robbmob.com/line/).

If you want to use purple-line with Pidgin or Finch, copy libline.dll into `C:\Program Files (x86)\Pidgin\plugins\`
and libgcrypt-20.dll & libgpg-error-0.dll into `C:\Program Files (x86)\Pidgin\`.

## Features implemented

* Logging in
  * Authentication
  * Fetching user profile
  * Account icon
  * Syncing friends, groups and chats
* Send and receive messages in IM, groups and chats
* Fetch recent messages
  * For groups and chats
  * For IMs
* Synchronize buddy list on the fly
  * Adding friends
  * Blocking friends
  * Removing friends
  * Joining chats
  * Leaving chats
  * Group invitations
  * Joining groups
  * Leaving groups
* Buddy icons
* Editing buddy list
* Removing friends
* Leaving chats
* Leaving groups
* Message types
  * Text (send/receive)
  * Sticker (send via command/receive)
  * Image (send/receive)
  * Audio (send/receive)
  * Location (receive)

## Features not yet implemented

* Only fetch unseen messages, let a log plugin handle already seen messages
* Implement timeouts for faster reconnections
* Synchronize buddy list on the fly
  * Sync group/chat users more gracefully, show people joining/leaving
* Editing buddy list
  * Adding friends (needs search function)
  * Creating chats
  * Inviting people to chats
  * Creating groups
  * Updating groups
  * Inviting people to groups
* Changing your account icon
* Message types
  * Audio/Video (send) File transfer API for sending?
  * Figure out what the other 15 message types mean...
* Emoji (is it possible to tap into the smiley system for sending too?)
* Companion Pidgin plugin
  * "Show more history" button
  * Sticker list
  * Open image messages
  * Open audio messages
  * Open video messages
* Sending/receiving "message read" notifications
* Check builds on more platforms
* Packaging

## SuperSonic's Develop Note

I had thought that re-build a project called "PidginLine" 2 years ago since the "purple-line" had was discontinued...

Anyway, the original project was updated last year!

So, I gave up it instead of updating the "purple-line".

Umm...I will keep updating its struct to connect LINE Services, nice to meet~
