#pragma once

namespace spacepenguin {

/**
 * Follows the desktop's light or dark style through the XDG desktop portal
 * (org.freedesktop.appearance color-scheme), which GNOME, KDE and others set.
 * Qt 6.8 does not read it on GNOME, so the browser would stay light there in
 * "Follow system". Linux only; elsewhere this does nothing.
 */
void watchSystemScheme();

}
