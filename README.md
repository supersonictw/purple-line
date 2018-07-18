# WARNING: Line Corporation is currently banning people for using 3rd party clients

There are multiple reports of accounts being permanently banned for using 3rd party clients such
as Pidgin. What exactly triggers the ban is unknown, but it's probably best to stick to the
official clients now.

For this reason I've broken the build on purpose. If you still want to test purple-line, you
should be able to figure out how to fix it yourself. Use at your own risk.

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

The mirror is also available via SSL for secure access by using a **https://** url.

For instructions for adding a custom repository on Ubuntu, see
[the Ubuntu wiki](https://help.ubuntu.com/community/Repositories/Ubuntu).

For Debian, see [the Debian wiki](https://www.debian.org/releases/), or just add the following line
to your `sources.list` file:

<pre><code>deb http://debian.altrepo.eu/ <b>code_name</b> main</code></pre>

Replace **code_name** with your distribution specific codename. The codenames are listed in the
[debian.altrepo.eu wiki](http://altrepo.eu/git/debian.altrepo.eu/wikis/home#note).
In order to validate package signatures you need to add
[this public key](http://debian.altrepo.eu/altrepo_eu.pub) to the APT key list. It can be done
using the following command:

    wget -qO - http://debian.altrepo.eu/altrepo_eu.pub | sudo apt-key add -

On either distribution, after adding the repository and key, run the following commands to install
the plugin:

    sudo apt-get update
    sudo apt-get install purple-line

Install from source (Ubuntu/Debian)
-----------------------------------

apt-build enables you to install from source easily. You need to add a source entry to
/etc/apt/sources.list. Run the following commands to install from source:

<pre><code>sudo apt-get install apt-build
echo "deb-src http://debian.altrepo.eu/ <b>code_name</b> main" | sudo tee -a /etc/apt/sources.list
echo "deb http://debian.altrepo.eu/ <b>code_name</b> main" | sudo tee -a /etc/apt/sources.list
sudo apt-get update
sudo apt-build install purple-line
</code></pre>

Note that apt-build is **not** an official APT family program. If you install via source, your
purple-line version will be up to date with the git repository. Note that the current git version
may not have been tested on Ubuntu/Debian. The package will install known build dependencies, but if
the git version requires new dependencies, compilation will fail.

Install from source (Arch Linux)
--------------------------------

Arch Linux packages all the required dependencies and there's a PKGBUILD available (not yet in AUR).
You can install the plugin by simply typing:

    sudo pacman -S base-devel
    curl -O http://altrepo.eu/git/arch.altrepo.eu/raw/master/purple-line/PKGBUILD
    makepkg -cis

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

Install on Windows
------------------

Pre-built binaries for Windows are available at [Eion Robb's website](http://eion.robbmob.com/line/).

If you want to use purple-line with Pidgin or Finch, copy libline.dll into `C:\Program Files (x86)\Pidgin\plugins\`
and libgcrypt-20.dll & libgpg-error-0.dll into `C:\Program Files (x86)\Pidgin\`.

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
