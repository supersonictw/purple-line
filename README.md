purple-line
===========

libpurple (Pidgin, Finch) protocol plugin for [LINE](http://line.me/).

![Screenshot](http://i.imgur.com/By1yLXB.png)

Where are the binaries and packages?
------------------------------------

I am not looking into "easy to install" options before I'm satisfied with the stability. I'd rather
not have people who cannot figure out how to compile software by themselves be disappointed by an
unstable plugin.

How to install
--------------

Make sure you have the required prerequisites:

* libpurple - Library that provides the core functionality of Pidgin and other compatible clients.
  Probably available from your package manager
* thrift / libthrift - Apache Thrift compiler and C++ library. May be available from your package
  manager.

To install the plugin system-wide, run:

    make
    sudo make install

The makefile supports a flag THRIFT_STATIC=true which causes it to download and build a version of
Thrift and statically link it. This should be convenient for people using one of the numerous
distributions that do not package Thrift.

You can also install the plugin for your user only by replacing `install` with `user-install`.

How to install (Arch Linux)
---------------------------

Arch Linux packages all the required dependencies, so you can install the plugin by simply typing:

    sudo pacman -S thrift libpurple
    make
    sudo make install

How to install (Ubuntu)
-----------------------

Ubuntu does not currently package Thrift so it must be obtained elsewhere, or statically linked. To
build the plugin with a statically linked Thrift library, type:

    sudo apt-get install \
        libpurple-dev \
        libboost-dev libboost-test-dev \
        libboost-program-options-dev libboost-system-dev libboost-filesystem-dev libevent-dev \
        automake libtool flex bison pkg-config g++ libssl-dev
    make THRIFT_STATIC=true
    sudo make install THRIFT_STATIC=true

The installed packages include all the suggested dependencies from Thrift's website.

Implemented
-----------

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

To do
-----

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
