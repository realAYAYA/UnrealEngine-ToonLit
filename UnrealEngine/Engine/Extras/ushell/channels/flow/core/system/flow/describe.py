# Copyright Epic Games, Inc. All Rights Reserved.

from . import _backtrace

#-------------------------------------------------------------------------------
class Tool(object):
    class _Bundle(object):
        def __init__(self):                    self._payloads = []
        def payload(self, url, filetype=None): self._payloads.append((url, filetype))

    def __init__(self):
        self._bin_paths = []
        self._bundles = {}
        self._root_dir = None
        self._sha1 = None
        self._version = None
        self._platform = None
        self._source = (None, None)

    def sha1(self, sha1):                  self._sha1 = sha1
    def version(self, version):            self._version = version
    def platform(self, platform):          self._platform = platform
    def source(self, url, ver_regex):      self._source = (url, ver_regex)
    def bin(self, bin_path):               self._bin_paths.append(bin_path)
    def root_dir(self, root_dir):          self._root_dir = root_dir
    def bundle(self, name="$"):            return self._bundles.setdefault(name, Tool._Bundle())
    def payload(self, url, filetype=None): self.bundle().payload(url, filetype)

    # Implemented elsewhere.
    #def extract(self, src_path, dest_dir):
        #pass

#-------------------------------------------------------------------------------
class Command(object):
    def __init__(self):
        self._path = None
        self._prefix = "."
        self._py_path = None
        self._py_class = None

    def prefix(self, prefix):
        self._prefix = prefix

    def invoke(self, *path):
        self._path = path

    def source(self, script_path, cmd_class):
        self._py_path = script_path
        self._py_class = cmd_class

#-------------------------------------------------------------------------------
class Channel(object):
    def __init__(self):
        self._parent = ""
        self._pips = []
        self._version = None

    def parent(self, parent):   self._parent = parent
    def version(self, version): self._version = version

    def _pip_deprecated_(self, name):
        # Please do not use! Pips come with security and licensing headaches so
        # support for them has been removed.
        self._pips.append(name)
