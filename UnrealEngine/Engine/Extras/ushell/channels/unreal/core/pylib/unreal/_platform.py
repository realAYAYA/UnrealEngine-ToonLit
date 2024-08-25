# Copyright Epic Games, Inc. All Rights Reserved.

import re
import os
import sys

#-------------------------------------------------------------------------------
def _optional_api(func):
    def inner(self, *args, **kwargs):
        if not hasattr(self, "_" + func.__name__):
            raise NotImplementedError(f"Platform '{self.get_name()}' does not implement method '{func.__name__}()'")
        return func(self, *args, **kwargs)

    return inner



#-------------------------------------------------------------------------------
class Platform(object):
    @staticmethod
    def get_host():
        if sys.platform == "linux":  return "Linux"
        if sys.platform == "darwin": return "Mac"
        if sys.platform == "win32":  return "Win64"
        raise EnvironmentError("Unknown host platform; " + sys.platform)

    @staticmethod
    def get_sdks_dir():
        env = os.getenv("UE_SDKS_ROOT")
        if env:
            return env + "/Host" + Platform.get_host() + "/"

    def __init__(self, context):
        self._context = context
        self._version = None
        self._sdks_dir = None

    @staticmethod
    def _get_version_helper_ue4(dot_cs, var_name):
        needle = re.compile(var_name + r'\s+=\s+("|[\w.]+\("|)([^"]+)"?\)?\s*;')
        try:
            with open(dot_cs, "rt") as lines:
                for line in (x for x in lines if var_name in x):
                    m = needle.search(line)
                    if m: return m.group(2)
        except FileNotFoundError:
            pass

    def _get_version_helper_ue5(self, dot_cs, func_name="GetMainVersion"):
        dot_cs = self._context.get_engine().get_dir() / dot_cs
        try:
            import re
            with open(dot_cs, "rt") as cs_in:
                lines = iter(cs_in)
                next(x for x in lines if f"override string {func_name}" in x or "override string GetDesiredVersion()" in x)
                next(lines) # {
                for i in range(5):
                    line = next(lines)
                    if m := re.search(r'return "(.+)"', line):
                        return m.group(1)
        except (FileNotFoundError, StopIteration):
            pass

    def get_name(self):
        return type(self).name

    def get_config_name(self):
        return getattr(type(self), "config_name", None) or self.get_name()

    def get_autosdk_name(self):
        return type(self).autosdk_name if hasattr(self, "autosdk_name") else self.get_name()

    def get_unreal_context(self):
        return self._context

    def _get_version_json(self):
        def conform_json(inp):
            # As the time of writing the SDK.json files have trailing commas
            # and C-style comments. So we'll try and conform it
            ret = ""
            for line in inp:
                try: line = line[:line.index("//")]
                except ValueError: pass
                line = line.strip()
                if not line:
                    continue

                line = line[:-1] if line.endswith(",") else line
                sep = "" if ret[-1:] in "[{" else ","
                sep = "" if line[0] in "]}" else sep

                ret += sep + line
            return ret

        def read_sdk_json(path):
            if not path.is_file():
                return

            import json
            with path.open("rt") as inp:
                data = conform_json(inp)
                try:
                    sdk_info = json.loads(data)
                except:
                    return

            if version := sdk_info.get("AutoSDKDirectory", None):
                return version

            if version := sdk_info.get("MainVersion", None):
                return version

            if next_path := sdk_info.get("ParentSDKFile", None):
                return read_sdk_json(path.parent / next_path)

        name = self.get_config_name()
        engine_dir = self._context.get_engine().get_dir()
        json_path = engine_dir / f"Platforms/{name}/Config/{name}_SDK.json"
        if version := read_sdk_json(json_path):
            return version

        json_path = engine_dir / f"Config/{name}/{name}_SDK.json"
        return read_sdk_json(json_path)

    def get_version(self):
        if not (version := self._version or self._get_version_json()):
            engine_version = self._context.get_engine().get_version_major()
            self._version = self._get_version_ue5() if engine_version > 4 else self._get_version_ue4()
            version = self._version or ""
        return version

    def read_env(self):
        class Value(object):
            def __init__(self, k, v):   self.key = k; self._value = v
            def __str__(self):          return self.validate();
            def __fspath__(self):       return str(self)
            def get(self):              return self._value or "SDK_NOT_FOUND"
            def validate(self):
                value = self.get()
                if not os.path.isdir(value):
                    raise EnvironmentError(f"Invalid directory '{value}' for '{self.key}'")
                return value

        sdks_dir = Platform.get_sdks_dir() or ""
        for key, value in self._read_env():
            if value: value = os.path.join(sdks_dir, value)
            yield Value(key, value)

    @_optional_api
    def get_cook_form(self, target):
        form = self._get_cook_form(target)
        if form:
            return form
        raise ValueError(f"No known cook form for target '{target}'")

    def get_cook_flavor(self):
        if hasattr(self, "_get_cook_flavor"):
            return self._get_cook_flavor()

    @_optional_api
    def launch(self, exec_context, stage_dir, binary_path, args):
        return self._launch(exec_context, stage_dir, binary_path, args)

    @_optional_api
    def kill(self, target=None):
        return self._kill(target)
