# Copyright Epic Games, Inc. All Rights Reserved.

import os
import subprocess as sp
from os.path import exists, join, basename

#-------------------------------------------------------------------------------
if os.name == "nt":
    import struct
    def _is_gui_binary(binary_path):
        with open(binary_path, "rb") as exe_file:
            # DOS header
            dos_header = exe_file.read(64)
            dos_header = struct.unpack("H" + ("H" * 29) + "I", dos_header)
            pe_offset = dos_header[-1]
            assert dos_header[0] == 0x5a4d

            # NT header
            exe_file.seek(pe_offset)
            pe_magic, = struct.unpack("I", exe_file.read(4))
            assert pe_magic == 0x4550 # 'PE'

            # File header
            file_header = struct.unpack("HHIIIHH", exe_file.read(20))
            optional_header_size = file_header[-2] - (8 * 16)

            # Optional header
            optional_header = exe_file.read(optional_header_size)
            if file_header[0] == 0x8664: opt_unpack = "HBBIIIIIQIIHHHHHHIIIIHHQQQQII"
            else: opt_unpack = "HBBIIIIIIIIIHHHHHHIIIIHHIIIIII"
            optional_header = struct.unpack(opt_unpack, optional_header)
            subsystem = optional_header[-8]
            return subsystem == 2

    def _read_args_impl(*args):
        for arg in (os.fspath(x) for x in args):
            if " " in arg:
                arg = arg.replace('"', r'\"')
                yield rf'"{arg}"'
            else:
                yield str(arg)
else:
    def _is_gui_binary(binary_path):
        return False

    def _read_args_impl(*args):
        yield from (os.fspath(x) for x in args)



#-------------------------------------------------------------------------------
class Runnable(object):
    def __init__(self, exe, *args, env=None):
        self._proc = None
        self._ret = None
        self._env = env

        exe = str(os.fspath(exe))
        if not exists(exe) and "/" not in exe.replace("\\", "/"):
            paths = os.getenv("PATH", "").split(os.pathsep)
            x = next((x for x in paths if exists(join(x, exe))), None)
            exe = join(x, exe) if x else exe
        self._exe = exe
        self._args = [x for x in args if x != None]

    def __add__(self, arg):
        if arg != None:
            self._args.append(arg)
        return self

    def __iter__(self):
        stdout = self.launch(stdout=True)
        return (x.decode().rstrip() for x in iter(stdout.readline, b""))

    def get_exe_path(self):
        return self._exe

    def read_args(self):
        yield from _read_args_impl(self.get_exe_path(), *self._args)

    def is_gui(self):
        return _is_gui_binary(self._exe)

    def set_env(self, env):
        self._env = env

    def launch(self, stdin=None, stdout=None, suspended=False, new_term=False):
        create_flags = 0
        if suspended: create_flags |= 0x4 # CREATE_SUSPENDED
        if new_term:  create_flags |= sp.CREATE_NEW_CONSOLE

        kwargs = {
            "env" : self._env,
            "stdin" : stdin,
            "stdout" : sp.PIPE if stdout else None if stdout == None else sp.DEVNULL,
            "stderr" : sp.STDOUT if stdout else None,
            "creationflags" : create_flags,
        }

        if os.name == "nt":
            cmd = " ".join(self.read_args())
        else:
            cmd = (self.get_exe_path(), *self._args)

        self._proc = sp.Popen(cmd, **kwargs)
        return self._proc.stdout

    def wait(self):
        if not self._proc:
            return

        # Wait for the process.
        if self._proc.stdin:
            self._proc.communicate()
            self._proc.stdin.close()
        self._proc.wait()

        # Close pipes.
        if self._proc.stdout: self._proc.stdout.close()
        if self._proc.stderr: self._proc.stderr.close()

        self._ret = self._proc.returncode
        self._proc = None
        return self._ret

    def get_pid(self):
        return self._proc.pid if self._proc else 0

    def get_return_code(self):
        return self._ret if self._ret else self.wait()

    def kill(self):
        if self._proc:
            self._proc.kill()
        self._proc = None
        self._ret = None

    def run(self, **kwargs):
        self.launch(**kwargs)
        self.wait()
        return self._ret

    def run2(self):
        self.launch(stdout=True)
        output = self._proc.communicate()[0].decode()
        self._ret = self._proc.returncode
        self._proc = None
        return self._ret, output

