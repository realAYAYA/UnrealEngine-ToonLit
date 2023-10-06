# Copyright Epic Games, Inc. All Rights Reserved.

import re

#-------------------------------------------------------------------------------
def _parse_struct(line):
    if not line.startswith("("):
        return line
    ret = _Struct()
    m = re.findall(r'(\w+)\s*=\s*(?:([^\s,"]+)|"(\\.|[^"]*)")', line[1:-1])
    for key, value, temp in m:
        ret.add_item(key, value or temp)
    return ret

#-------------------------------------------------------------------------------
class _Value(object):
    def __init__(self):
        self._data = None
        self._parsed = False

    def get_inner(self):
        self._parse()
        return self._data

    def _set(self, value):
        self._data = value

    def _append(self, value, dedupe):
        if not isinstance(self._data, list):
            self._data = []
        if not dedupe or value not in self._data:
            self._data.append(value)

    def _parse(self):
        if self._parsed or not self._data:
            return
        self._parsed = True
        if isinstance(self._data, list):
            self._data = [_parse_struct(x) for x in self._data]
        else:
            self._data = _parse_struct(self._data)

    def __bool__(self):
        return self._data is not None

    def __repr__(self):
        self._parse()
        return str(self._data)

    def __iter__(self):
        self._parse()
        if isinstance(self._data, list):
            yield from self._data
        elif self._data is not None:
            yield self._data

    def __getattr__(self, name):
        self._parse()
        if isinstance(self._data, _Struct):
            return getattr(self._data, name) or _Value()
        return _Value()

#-------------------------------------------------------------------------------
class _Struct(object):
    def __init__(self):
        self._data = {}

    def get_inner(self):
        return self._data

    def add_item(self, key, value, cmd=""):
        if cmd == "":
            if key not in self._data:
                self._data[key] = out = _Value()
                out._set(value)
            return

        out = self._data.setdefault(key, _Value())
        if   cmd == "+": out._append(value, True)
        elif cmd == ".": out._append(value, False)

    def __repr__(self):
        return repr(self._data)

    def __getattr__(self, name):
        return self._data.get(name) or _Value()

    def __iter__(self):
        return iter(self._data.items())

#-------------------------------------------------------------------------------
class Ini(object):
    def __init__(self):
        self._sections = {}

    def _add_section(self, name):
        return self._sections.setdefault(name, _Struct())

    @classmethod
    def _lazy_re_compile(self):
        if hasattr(self, "_lazied"):
            return
        self._comment_re = re.compile(r"\s*[#;]")
        self._section_re = re.compile(r"\s*\[(.+)\]")
        self._entry_re = re.compile(r"^\s*([+-.!@*]?)([^=]+)\s*=\s*(.+)\s*$")
        self._lazied = True

    def load(self, file):
        self._lazy_re_compile()
        section = None
        for line in (x.strip() for x in file):
            if m := Ini._comment_re.match(line):
                continue
            elif m := Ini._section_re.match(line):
                section = self._add_section(m.group(1))
            elif m := Ini._entry_re.match(line):
                section.add_item(m.group(2), m.group(3), m.group(1))

    def __getattr__(self, name):
        return self._sections.get(name) or _Struct()

    def __iter__(self):
        return iter(self._sections.items())
