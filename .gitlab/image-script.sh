#!/usr/bin/bash

dnf builddep -y glib \
    && git clone --depth 1 https://gitlab.gnome.org/GNOME/glib.git \
    && cd glib \
    && meson setup _build -Dintrospection=disabled -Dtests=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \

dnf builddep -y gobject-introspection \
    && git clone --depth 1 --recurse-submodules -j8 https://gitlab.gnome.org/GNOME/gobject-introspection.git \
    && cd gobject-introspection \
    && meson setup _build --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf gobject-introspection

dnf builddep -y glib \
    && cd glib \
    && meson setup --wipe _build -Dtests=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf glib