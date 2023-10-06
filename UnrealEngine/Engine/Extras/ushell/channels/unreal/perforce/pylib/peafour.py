# Copyright Epic Games, Inc. All Rights Reserved.

import types
import marshal
import threading
import subprocess as sp
from pathlib import PurePath

#-------------------------------------------------------------------------------
class _P4Result(object):
    def __init__(self, result):
        super().__setattr__("_result", result)

    def __str__(self):                 return str(self._result)
    def __contains__(self, key):       return (key in self._result) or (key + "0" in self._result)
    def as_dict(self):                 return {k.decode():v.decode() for k,v in self._result.items()}
    def __setattr__(self, key, value): self._result[key.encode()] = str(value).encode()

    def __getattr__(self, key):
        if (key + "0").encode() in self._result:
            def as_list():
                index = 0;
                while (key + str(index)).encode() in self._result:
                    indexed_key = (key + str(index)).encode()
                    yield self._result[indexed_key].decode()
                    index += 1
            return as_list()

        ret = self._result.get(str.encode(key))
        if ret == None: raise AttributeError(key)
        return ret.decode(errors="replace")

#-------------------------------------------------------------------------------
class _P4Command(object):
    @staticmethod
    def _read_args(*args, **kwargs):
        for k,v in kwargs.items():
            if isinstance(v, bool):
                if v:
                    yield "-" + k
            elif v != None:
                yield f"-{k}={v}" if len(k) > 1 else f"-{k}{v}"

        for arg in args:
            if isinstance(arg, str) or isinstance(arg, PurePath):
                yield str(arg)

    def __init__(self, **options):
        opt_iter = _P4Command._read_args(**options)
        self._command = ["p4", "-Qutf8", "-G"]
        self._command += (x for x in opt_iter)

    def start(self, command, *args, **kwargs):
        self._stdin_args = []
        for arg in args:
            if isinstance(arg, str) or isinstance(arg, PurePath):
                continue
            if not hasattr(arg, "__iter__"):
                raise TypeError("P4 arguments can be only strings or sequences")
            self._stdin_args.append(arg)

        self._proc = None

        arg_iter = _P4Command._read_args(*args, **kwargs)

        if self._stdin_args:
            self._command.append("-x-")
        self._command.append(command)
        self._command += (x for x in arg_iter)

    def __del__(self):              self._close_proc()
    def __str__(self):              return " ".join(self._command)
    def __iter__(self):             yield from self._iter()
    def __getattr__(self, name):    return getattr(self.run(), name)

    def run(self, **kwargs):
        return next(self._iter(**kwargs), None)

    def read(self, **kwargs):
        yield from self._iter(**kwargs)

    def _close_proc(self):
        if self._proc:
            if self._proc.stdin:
                self._proc.stdin.close()
            self._proc.stdout.close()
            self._proc = None

    def _iter(self, input_data=None, on_error=True, on_info=None, on_text=None):
        stdin = None
        if input_data != None:
            if self._stdin_args:
                raise _P4.Error("It is unsupported to have both generator-type arguments and input data")

            if isinstance(input_data, dict):
                input_data = {str(k).encode():str(v).encode() for k,v in input_data.items()}
            else:
                raise _P4.Error("Unsupported input data type; " + type(input_data).__name__)
            stdin = sp.PIPE

        if self._stdin_args:
            stdin = sp.PIPE

        proc = sp.Popen(self._command, stdout=sp.PIPE, stdin=stdin)
        self._proc = proc

        if stdin:
            def stdin_thread_entry():
                try:
                    if input_data:
                        marshal.dump(input_data, proc.stdin, 0)
                    for args in self._stdin_args:
                        for arg in args:
                            arg = str(arg).encode() + b"\n"
                            proc.stdin.write(arg)
                except (BrokenPipeError, OSError):
                    pass
                finally:
                    try: proc.stdin.close()
                    except: pass

            stdin_thread = threading.Thread(target=stdin_thread_entry)
            stdin_thread.start()

        while True:
            try: result = marshal.load(proc.stdout)
            except EOFError: break

            code = result[b"code"]
            del result[b"code"]

            if code == b"error":
                if isinstance(on_error, bool):
                    if on_error:
                        raise _P4.Error(result[b"data"].decode()[:-1])
                    continue

                try:
                    on_error(_P4Result(result))
                except:
                    proc.terminate()
                    raise
                continue

            if code == b"stat":
                yield _P4Result(result)
                continue

            if code == b"text" and on_text:
                try:
                    data = result.get(b"data", b"")
                    on_text(data)
                    continue
                except:
                    proc.terminate()
                    raise

            if code == b"info" and on_info:
                try:
                    on_info(_P4Result(result))
                    continue
                except:
                    proc.terminate()
                    raise

        if stdin:
            stdin_thread.join()
        self._close_proc()

#-------------------------------------------------------------------------------
class _P4(object):
    class Error(Exception):
        def __init__(self, msg):
            super().__init__("Perforce: " + msg)

    def __init__(self, **options):
        self._options = options

    def __getattr__(self, command):
        if command == "Error":
            return _P4.Error

        def inner(*args, **kwargs):
            instance = _P4Command(**self._options)
            instance.start(command, *args, **kwargs)
            return instance

        return inner

    def __call__(self, **options):
        return _P4(**options)

P4 = _P4()
