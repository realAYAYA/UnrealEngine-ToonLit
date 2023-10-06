# Copyright Epic Games, Inc. All Rights Reserved.

import sys

_enabled = sys.stdout.isatty() and sys.stderr.isatty()

#-------------------------------------------------------------------------------
class _Deferred(object):
    def __init__(self, builder, string):
        def _deferred_pre(): return builder._get_tag() or ""
        def _deferred_post(): return builder._get_prev_tag() or ""
        self.actions = [_deferred_pre, string, _deferred_post]

    def __mod__(self, rhs):
        if not isinstance(rhs, tuple): rhs = rhs,
        self.actions.append(rhs)
        return self

    def __add__(self, rhs):
        self.actions.append(rhs)
        return self

    def __radd__(self, rhs):
        self.actions.insert(0, rhs)
        return self

    def __str__(self):
        out = ""
        for action in self.actions:
            if hasattr(action, "__call__"): out += action()
            elif isinstance(action, tuple): out = out % action
            else: out += str(action)
        return out

#-------------------------------------------------------------------------------
class _Builder(object):
    _prev = None
    _colours = {
        "_"             : None,
        "black"         : 0,
        "red"           : 1,
        "green"         : 2,
        "yellow"        : 3,
        "blue"          : 4,
        "magenta"       : 5,
        "cyan"          : 6,
        "light_grey"    : 7,
        "grey"          : 8,
        "light_red"     : 9,
        "light_green"   : 10,
        "light_yellow"  : 11,
        "light_blue"    : 12,
        "light_magenta" : 13,
        "light_cyan"    : 14,
        "white"         : 15,
    }

    def __init__(self, fg=None):
        self.prev = None
        self.bg = None
        try:
            self.fg = self._colours[fg]
        except KeyError:
            raise SyntaxError("Invalid foreground colour '%s'" % fg)

    def __getattr__(self, name):
        try:
            self.bg = self._colours[name]
            return self
        except KeyError:
            pass

        try:
            return self.__dict__[name]
        except KeyError:
            raise SyntaxError("Invalid background colour '%s'" % name)

    def __call__(self, string):
        return _Deferred(self, string)

    def __enter__(self):
        self.prev = _Builder._prev
        _Builder._prev = self

        tag = self._get_tag()
        if tag:
            sys.stdout.write(tag)

    def __exit__(self, *args):
        _Builder._prev = self.prev

        if self.fg or self.bg:
            tag = self._get_prev_tag()
            if tag:
                sys.stdout.write(tag)

    def _get_tag(self):
        return self._get_tag_impl(self.fg, self.bg)

    def _get_prev_tag(self):
        prev = self.prev or _Builder._prev
        if prev:
            return prev._get_tag()
        elif _enabled:
            return "\x1b[0m"

    @staticmethod
    def _get_tag_impl(fg=None, bg=None):
        if not _enabled:
            return

        if fg == None and bg == None:
            return "\x1b[0m"

        codes = []
        if fg != None and fg >= 0:
            bias = 90 if fg > 7 else 30
            codes.append(bias + (fg & 7))

        if bg != None and bg >= 0:
            bias = 100 if bg > 7 else 40
            codes.append(bias + (bg & 7))

        if len(codes):
            return "\x1b[" + ";".join((str(x) for x in codes)) + "m"

#-------------------------------------------------------------------------------
class _Text(object):
    def __getattr__(self, attr_name):
        return _Builder(attr_name)

text = _Text()
