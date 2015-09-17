purple-line
===========

libpurple (Pidgin, Finch) protocol plugin for [LINE](http://line.me/).

![Screenshot](http://i.imgur.com/By1yLXB.png)

Install via package manager (Ubuntu/Debian)
-------------------------------------------

An APT repository is available for installing the plugin. The repository contains the purple-line
package itself, and the required Apache Thrift packages which are not properly packaged by either
distribution.

* http://debian.altrepo.eu/ (main)
* http://debian.surlinter.net/ (mirror)

For instructions for adding a custom repository on Ubuntu, see
[the Ubuntu wiki](https://help.ubuntu.com/community/Repositories/Ubuntu).

For Debian, see [the Debian wiki](https://www.debian.org/releases/), or just add the following line
to your `sources.list` file:

    deb http://debian.altrepo.eu/ stable main

In order to validate package signatures you need to add
[this public key](http://debian.altrepo.eu/altrepo_eu.pub) to the APT key list. It can be done
using the following command:

    wget -qO - http://debian.altrepo.eu/altrepo_eu.pub | sudo apt-key add -

On either distribution, after adding the repository and key, run the following commands to install
the plugin:

    sudo apt-get update
    sudo apt-get install purple-line

Install from source (Arch Linux)
--------------------------------

Arch Linux packages all the required dependencies, so you can install the plugin by simply typing:

    sudo pacman -S thrift libpurple libgcrypt
    make
    sudo make install

Install from source (any distribution)
--------------------------------------

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

Features implemented
--------------------

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

Features not yet implemented
----------------------------

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
