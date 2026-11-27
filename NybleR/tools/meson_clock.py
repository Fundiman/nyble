#!/usr/bin/env python3
"""Local Meson wrapper that tolerates Windows future file timestamps.

Meson aborts configuration when a regeneration dependency such as
`meson-private/coredata.dat` is newer than the current clock. Windows filesystems
can report timestamps a fraction of a second in the future, which makes setup
fail before the build can even start. Clamp reported mtimes to shortly before
"now" for the Meson process only; no source file is modified. The margin is
configurable with NYBLER_MESON_CLOCK_BACKDATE and defaults to five seconds.
"""

import os
import sys
import time

_real_getmtime = os.path.getmtime
_real_time = time.time
_backdate = float(os.environ.get("NYBLER_MESON_CLOCK_BACKDATE", "5"))


def _clamped_getmtime(path):
    # Leave a margin so a timestamp cannot drift into the future between
    # Meson's "now" sample and this getmtime() call.
    return min(_real_getmtime(path), _real_time() - _backdate)


os.path.getmtime = _clamped_getmtime

from mesonbuild import mesonmain  # noqa: E402


if __name__ == "__main__":
    sys.exit(mesonmain.main())
