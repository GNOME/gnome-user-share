# User Sharing

gnome-user-share is a small package that binds together various free
software projects to bring easy to use user-level file sharing to the
masses.

The program is meant to run in the background when the user is logged
in, and when file sharing is enabled a webdav server is started that
shares the ~/Public folder. The share is then published to all
computers on the local network using mDNS/rendezvous, so that it shows
up in the Network location in GNOME.


## Dependencies

The dav server used is Apache, so you need that installed. Avahi or
Howl is used for mDNS support, so you need to have that installed and
mDNSResolver running.

## How to report issues

Report issues to the GNOME [issue tracking system](https://gitlab.gnome.org/GNOME/gnome-user-share/issues).

## Contributing

To build the development version:

```
git clone https://gitlab.gnome.org/GNOME/gnome-user-share.git
cd gnome-user-share
meson setup _build
meson compile -C _build
```

Commit messages should follow the expected format [detailed here](https://handbook.gnome.org/development/commit-messages.html).
