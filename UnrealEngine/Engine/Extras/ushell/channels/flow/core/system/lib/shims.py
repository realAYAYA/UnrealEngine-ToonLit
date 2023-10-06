# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import fsutils

#-------------------------------------------------------------------------------
class _BuilderBase(object):
    def __init__(self, shims_dir):
        self._out_dir = shims_dir

    def commit(self):
        self._out_dir.keep()

    def create_python_shim(self, name, *args):
        shim_path = self._out_dir + name
        if os.path.exists(shim_path):
            return

        self._create_python_shim_impl(shim_path, *args)

        # A '.' prefix was chosen to "namespace" commands in the host shell in a
        # manner that's easy to access (e.g. typing <dot><tab>). It turns out
        # however that Windows is picky about ".foobar". So we'll create a fallback
        if name[0] == ".":
            shim_path = f"{self._out_dir}_{name[1:]}"
            self._create_python_shim_impl(shim_path, *args)

    def create_shim(self, name, exe, *args):
        shim_path = self._out_dir + name
        if os.path.exists(shim_path):
            return

        self._create_shim_impl(shim_path, exe, *args)

#-------------------------------------------------------------------------------
class _BuilderNt(_BuilderBase):
    def clean(self):
        # Clean up the existing shims dir. This can fail if Windows has locked it
        dead_shims_dir = self._out_dir + "../_deadshims/"
        try:
            if os.path.isdir(self._out_dir):
                try: os.makedirs(dead_shims_dir)
                except FileExistsError: pass
                os.rename(self._out_dir, dead_shims_dir + str(os.getpid()))
            fsutils.rmdir(dead_shims_dir)
        except (FileNotFoundError, PermissionError):
            #_log.write("Failed to clean dead shims directory")
            pass

        self._out_dir = fsutils.WorkPath(self._out_dir)

    def _create_python_shim_impl(self, shim_path, *args):
        if not shim_path.endswith(".exe"):
            shim_path += ".exe"

        import exeshim
        exeshim.create_py(shim_path, *args)

    def _create_shim_impl(self, shim_path, exe, *args):
        if not shim_path.endswith(".exe"):
            shim_path += ".exe"

        args = ('"' + '" "'.join(args) + '"') if args else ""
        import exeshim
        exeshim.create(shim_path, exe, args)

#-------------------------------------------------------------------------------
class _BuilderPosix(_BuilderBase):
    def clean(self):
        fsutils.rmdir(self._out_dir)
        self._out_dir = fsutils.WorkPath(self._out_dir)

    def _create_python_shim_impl(self, shim_path, *args):
        exe = os.path.abspath(sys.executable)
        return self._create_shim_impl(shim_path, exe, *args)

    def _create_shim_impl(self, shim_path, exe, *args):
        import shlex
        with open(shim_path, "wt") as out:
            out.write("#!/bin/sh\n")
            items = (shlex.quote(x) for x in (exe, *args))
            out.write(" ".join(items))
            out.write(" \"$@\"\n")

        import stat
        os.chmod(shim_path, stat.S_IRWXU)

Builder = _BuilderNt if os.name == "nt" else _BuilderPosix
