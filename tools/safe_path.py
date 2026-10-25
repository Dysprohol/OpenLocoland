"""os.path.relpath for paths that may be on different drives (Windows).

`os.path.relpath` raises ValueError when the paths are on different mounts, e.g. a Steam
library on D: and the install folder on C:. There is no relative path then, so the absolute
path is the useful answer: the callers only use the result to name a file in a log or to
compare it with a path built from the same root.
"""
import os


def safe_relpath(path, start):
    try:
        return os.path.relpath(path, start)
    except ValueError:   # different drives: no relative path exists
        return os.path.abspath(path)
