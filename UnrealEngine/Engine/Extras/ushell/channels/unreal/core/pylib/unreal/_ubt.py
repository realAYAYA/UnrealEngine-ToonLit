# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import sys
import enum
import tempfile
from pathlib import Path

#-------------------------------------------------------------------------------
class _BuildConfigLevel(enum.Enum):
    INTERNAL = 0
    BRANCH   = 1
    GLOBAL   = 2
    LEGACY   = 3

#-------------------------------------------------------------------------------
class _BuildConfigXml(object):
    def __init__(self):
        self._depth = 0
        self._comment_count = 0
        self._root = {}
        self._ns_skip = 0

    def start(self, tag, attribs):
        tag = tag[self._ns_skip + 2:] if tag.startswith("{") else tag
        if self._depth == 1:   self._category = self._root.setdefault(tag, {})
        elif self._depth == 2: self._prop = self._category.setdefault(tag, {"value":""})
        self._depth += 1

    def end(self, tag): self._depth -= 1
    def start_ns(self, prefix, uri): self._ns_skip = len(uri)
    def end_ns(self, prefix):        self._ns_skip = 0

    def data(self, data):
        if self._depth == 3:
            if data := data.strip():
                self._prop["value"] = data

    def comment(self, data):
        target = None
        if self._depth == 1:   target = self._root
        elif self._depth == 2: target = self._category
        elif self._depth == 3: target = self._prop
        if target != None:
            x = f"$comment" + str(self._comment_count)
            target[x] = data
            self._comment_count += 1

    def print(self, file=None):
        indent = ""
        def spam(*args):        print(indent, *args, sep="", file=file)
        def spam_comment(data): spam("<!--", data, "-->")

        spam("<?xml version='1.0' encoding='utf-8'?>")
        spam(r'<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">')
        indent = "  "
        for category_name, category in self._root.items():
            if category_name.startswith("$comment"):
                spam_comment(category)
                continue

            if not len(category):
                continue

            spam(f"<{category_name}>")
            indent = "    "
            for prop_name, prop in category.items():
                if prop_name.startswith("$comment"):
                    spam_comment(prop)
                    continue

                for key, value in prop.items():
                    if key.startswith("$comment"):
                        spam_comment(value)
                        continue

                    spam(f"<{prop_name}>{value}</{prop_name}>")
            indent = "  "
            spam(f"</{category_name}>")
        indent = ""
        spam("</Configuration>")

    def load(self, path):
        import xml.etree.ElementTree as et
        with open(path, "rb") as file:
            parser = et.XMLParser(target=self)
            try:
                parser.feed(file.read())
            except et.ParseError as e:
                raise IOError(str(e))
            parser.close()

    def save(self, path):
        with open(path, "wt") as file:
            self.print(file=file)

    def read_values(self):
        for category, i in (x for x in self._root.items() if not x[0].startswith("$")):
            for prop, j in (x for x in i.items() if not x[0].startswith("$")):
                yield category, prop, j["value"]

    def get_value(self, category, name):
        if parent := self._root.get(category):
            if prop := parent.get(name):
                return prop["value"]

        raise LookupError(f"No such property '{category}.{name}'")

    def set_value(self, category, name, value):
        category = self._root.setdefault(category, {})
        prop = category.setdefault(name, {})
        prop["value"] = value

    def clear_value(self, category, name):
        if parent := self._root.get(category):
            if name in parent:
                del parent[name]
                return

        raise LookupError(f"No such property '{category}.{name}'")

#-------------------------------------------------------------------------------
class _BuildConfig(object):
    def __init__(self, path, level):
        self._path = path
        self._level = level
        self._root = None

    def _get_root(self):
        if self._root:
            return self._root

        self._root = _BuildConfigXml()
        if self.exists():
            self._root.load(self._path)
        return self._root

    def get_path(self):                         return self._path
    def get_level(self):                        return self._level
    def exists(self):                           return os.path.isfile(self._path)
    def save(self):                             self._get_root().save(self.get_path())
    def read_values(self):                      yield from self._get_root().read_values()
    def get_value(self, category, name):        return self._get_root().get_value(category, name)
    def set_value(self, category, name, value): return self._get_root().set_value(category, name, value)
    def clear_value(self, category, name):      return self._get_root().clear_value(category, name)

#-------------------------------------------------------------------------------
class _SchemaNode(object):
    def __init__(self, root):
        self._root = root

    def get_name(self):
        if isinstance(self._root, str): return self._root
        else: return self._root.get("name")

    def read_children(self):
        if isinstance(self._root, str):
            return

        ns = { "x" : "http://www.w3.org/2001/XMLSchema" }
        child = self._root.find("x:simpleType/x:restriction", ns)
        if child:
            yield from (_SchemaNode(x.get("value")) for x in child)
            return

        child = self._root.find("x:complexType/x:all", ns)
        if child:
            yield from (_SchemaNode(x) for x in child)
            return

        if self._root.get("type", None) == "boolean":
            yield from (_SchemaNode("true"), _SchemaNode("false"))

    def get_node(self, *ancestor_names):
        node = self
        for ancestor in ancestor_names:
            for child in node.read_children():
                if child.get_name() == ancestor:
                    node = child
                    break
            else:
                return

        return node

#-------------------------------------------------------------------------------
class _BuildConfigSchema(_SchemaNode):
    def __init__(self, path):
        self._path = path

        import xml.etree.ElementTree as et
        xml = et.parse(self._path)
        root = next(iter(xml.getroot()))
        super().__init__(root)

    def get_path(self):
        return path



#-------------------------------------------------------------------------------
class _BuildToolBase(object):
    def __init__(self, engine):
        self._update_check = True
        self._engine = engine

    def _get_bin_path(self):
        engine_dir = self._engine.get_dir()
        for script, update_check in self._get_script_paths():
            script = engine_dir / script
            if script.is_file():
                self._update_check = self._update_check and update_check
                return script

    def read_configurations(self):
        engine_dir = self._engine.get_dir()

        suffix = "UnrealBuildTool/BuildConfiguration.xml"
        yield _BuildConfig(engine_dir / "Restricted/NotForLicensees/Programs" / suffix, _BuildConfigLevel.INTERNAL)
        yield self.get_configuration(True)
        if os.name == "nt":
            for var_name in ("ProgramData", "APPDATA", "LOCALAPPDATA"):
                prefix = os.getenv(var_name, "!notset")
                yield _BuildConfig(prefix + "/Unreal Engine/" + suffix, _BuildConfigLevel.GLOBAL)
            user_profile = os.getenv("USERPROFILE", "userprofile-notset")
            yield _BuildConfig(user_profile + "/Documents/Unreal Engine/" + suffix, _BuildConfigLevel.LEGACY)
        else:
            config_dir = os.getenv("XDG_CONFIG_HOME", "~/.config")
            config_dir = os.path.expanduser(config_dir)
            yield _BuildConfig(config_dir + "/Unreal Engine/" + suffix, _BuildConfigLevel.LEGACY)

    def get_configuration(self, branch=False, *, create=False):
        suffix = "UnrealBuildTool/BuildConfiguration.xml"
        if branch:
            cfg_path = self._engine.get_dir() / "Saved" / suffix
        else:
            if os.name == "nt": prefix = os.getenv("LOCALAPPDATA", "appdata")
            else:               prefix = os.path.expanduser("~")
            cfg_path = Path(prefix) / "Unreal Engine" / suffix

        if create and not cfg_path.is_file():
            cfg_path.parent.mkdir(parents=True, exist_ok=True)
            with cfg_path.open("wt") as out:
                print("<?xml version='1.0' encoding='utf-8'?>", file=out)
                print(r'<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">', file=out)
                print("</Configuration>", file=out)

        level = _BuildConfigLevel.BRANCH if branch else _BuildConfigLevel.GLOBAL
        return _BuildConfig(cfg_path, level)

    def get_configuration_schema(self):
        engine_dir = self._engine.get_dir()
        xsd_path = engine_dir / "Saved/UnrealBuildTool/BuildConfiguration.Schema.xsd"
        if xsd_path.is_file():
            return _BuildConfigSchema(xsd_path)

    def read_actions(self, *args):
        cmd = self._get_bin_path()
        if not (cmd and cmd.is_file()):
            raise IOError("No UnrealBuildTool scripts found. Incomplete branch?")

        if cmd.suffix == ".bat":
            # cmd.exe insists on asking you if you want to terminate when Ctrl-C
            # is pressed. Often it doesn't work or takes N goes. It seems that
            # using devnull for stdin (<nul) makes it more tolerable. Messy :(
            args = ("/d/s/c", "\"", cmd, *args, "<nul\"")
            cmd = "cmd.exe"

        if self._update_check:
            self._update_check = False
            yield from self._read_build_ubt_actions()

        yield cmd, args

#-------------------------------------------------------------------------------
class _BuildToolWin64(_BuildToolBase):
    def __init__(self, *args):
        super().__init__(*args)
        self._msbuild_response_path = None

    def __del__(self):
        if self._msbuild_response_path:
            try: os.unlink(self._msbuild_response_path)
            except: pass

    def _read_build_ubt_actions(self):
        # Find msbuild.exe that MS have buried away.
        vswhere = (
            "vswhere.exe",
            "-latest",
            "-find MSBuild/*/Bin/amd64/MSBuild.exe",
        )
        import subprocess as sp
        proc = sp.Popen(" ".join(vswhere), stdout=sp.PIPE)
        msbuild_exe = next(iter(proc.stdout.readline, b""), b"").decode().rstrip()
        proc.stdout.close()

        if not msbuild_exe:
            msbuild_exe = "msbuild.exe"

        engine_dir = self._engine.get_dir()
        csproj_path = engine_dir / "Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj"

        args = (
            str(csproj_path),
            "/property:Configuration=Development;Platform=AnyCPU",
            "/verbosity:m",
            "/nologo",
            "/target:Build",
        )

        yield msbuild_exe, args

    def _get_script_paths(self):
        return (
            ("Build/BatchFiles/RunUBT.bat", False),
            ("Build/BatchFiles/Build.bat", True),
        )

#-------------------------------------------------------------------------------
class _BuildToolLinux(_BuildToolBase):
    def _read_build_ubt_actions(self):
        return iter(())

    def _get_script_paths(self):
        return (
            ("Build/BatchFiles/RunUBT.sh", False),
            ("Build/BatchFiles/Linux/Build.sh", True),
        )

#-------------------------------------------------------------------------------
class _BuildToolMac(_BuildToolBase):
    def _get_ubt_shim_script(self):
        engine_dir = self._engine.get_dir()
        script_path = engine_dir / "Intermediate/Build/ubt_ushell.sh"
        if script_path.is_file():
            return script_path

        script_path.parent.mkdir(parents=True, exist_ok=True)
        with script_path.open("wt") as out:
            out.write("#!/bin/sh\n")
            out.write(f"source {engine_dir}/Build/BatchFiles/Mac/SetupMono.sh\n")
            out.write("xbuild \"$@\"\n")
        script_path.chmod(0o777)
        return script_path

    def _read_build_ubt_actions(self):
        try:    script_path = self._get_ubt_shim_script()
        except: script_path = "xbuild"

        engine_dir = self._engine.get_dir()
        csproj_path = engine_dir / "Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj"
        yield str(script_path), (
            csproj_path,
            "/property:Configuration=Development",
            "/verbosity:quiet",
            "/nologo",
            "/property:NoWarn=1591",
        )

    def _get_script_paths(self):
        return (
            ("Build/BatchFiles/RunUBT.sh", False),
            ("Build/BatchFiles/Mac/Build.sh", True),
        )

#-------------------------------------------------------------------------------
if sys.platform == "win32":     BuildTool = _BuildToolWin64
elif sys.platform == "linux":   BuildTool = _BuildToolLinux
elif sys.platform == "darwin":  BuildTool = _BuildToolMac
