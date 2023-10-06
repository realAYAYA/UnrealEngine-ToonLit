# Copyright Epic Games, Inc. All Rights Reserved.

import re
from pathlib import Path
from typing import Iterator

# The Unreal Engine's parsing of command line arguments is unconventional. As
# a user's given arguments flow through Python and C runtimes, their quoting is
# canonicalised into a conventional-but-not-UE-compatible form. This module
# attempts to format arguments in a UE-compatible way, wrapping them in a str-
# derived class that deliberately fails '" " in s' and 'for x in s if x == " "'
# style code such that the likes of subprocess.run do not add surrounding quotes

_re = None

#-------------------------------------------------------------------------------
class _IterValue(str):
    def __init__(self, inner):  self._inner = inner
    def __eq__(self, rhs):      return False
    def __str__(self):          return self._inner

class _Iter(object):
    def __init__(self, inner):  self._inner = inner
    def __next__(self):         return _IterValue(next(self._inner))

class _Shim(str):
    def __init__(self, value):  self._value = value
    def __iter__(self):         return _Iter(iter(str(self._value)))
    def __contains__(self, k):  return False
    def __str__(self):          return str(self._value)
    def __fspath__(self):       return self

#-------------------------------------------------------------------------------
def _cache_re() -> re.Pattern:
    return re.compile(r'^\s*-([a-zA-Z0-9]+)="*([^"]+)"*$')

#-------------------------------------------------------------------------------
def _ueify_impl(arg:str) -> str:
    try: m = _re.match(arg)
    except TypeError: return arg

    if m and " " in m.group(2):
        return _Shim(rf'-{m.group(1)}="{m.group(2)}"')
    return arg

#-------------------------------------------------------------------------------
def read_ueified(*args:str) -> Iterator[str]:
    global _re
    _re = _re or _cache_re()
    yield from (_ueify_impl(x) for x in args)

#-------------------------------------------------------------------------------
def ueified(*args:str) -> str:
    return " ".join(read_ueified(*args))

#-------------------------------------------------------------------------------
def escape(arg):
    if not isinstance(arg, _Shim):
        return fr'"{arg}"' if " " in arg else arg
    inner = str(arg._value)
    inner = inner.replace('"', r'\"')
    return _Shim(inner)
