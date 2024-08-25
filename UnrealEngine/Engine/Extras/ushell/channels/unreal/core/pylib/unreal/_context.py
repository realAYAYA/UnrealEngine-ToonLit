# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
from enum import Enum, Flag
from typing import Iterator
from pathlib import Path, WindowsPath, PosixPath

from . import _ini as ini
from ._ubt import BuildTool
from ._platform import Platform

# {{{1 misc --------------------------------------------------------------------

#-------------------------------------------------------------------------------
def _resolve_association_nt(association:str) -> Path|None:
    import winreg

    if association[0] == "{":
        hive = winreg.HKEY_CURRENT_USER
        key_path = r"SOFTWARE\Epic Games\Unreal Engine\Builds"
        value_name = association
    else:
        hive = winreg.HKEY_LOCAL_MACHINE
        key_path = r"SOFTWARE\EpicGames\Unreal Engine\\" + association
        value_name = "InstalledDirectory"

    try:
        with winreg.OpenKey(hive, key_path) as key:
            value = winreg.QueryValueEx(key, value_name)
    except OSError:
        return

    if value[1] == 1:
        return Path(value[0])

def _resolve_association_posix(association:str) -> Path|None:
    import configparser, sys

    # GUID values might be stored with or without curly braces, depending on platform and engine version
    candidates = [association]
    if association.startswith("{") and association.endswith("}"):
        candidates.append(association[1:-1])
    else:
        candidates.append(f"{{{association}}}")

    # Official engine releases are prefixed with "UE_" in Install.ini under Linux, but not in .uproject files
    candidates.append(f"UE_{association}")

    config_dir = "~/.config" if sys.platform == "linux" else "~/Library/Application Support"
    ini_file = Path(config_dir).expanduser() / "Epic" / "UnrealEngine" / "Install.ini"

    config = configparser.ConfigParser()
    config.read([ini_file])

    for candidate in candidates:
        value = config.get("Installations", candidate, fallback=None)
        if value is not None:
            return Path(value)

def _resolve_association(association:str) -> Path|None:
    if os.name == "nt":
        return _resolve_association_nt(association)
    else:
        return _resolve_association_posix(association)



# {{{1 enums -------------------------------------------------------------------

#-------------------------------------------------------------------------------
class Variant(Enum):
    DEBUG       = 0
    DEBUGGAME   = 1
    DEVELOPMENT = 2
    TEST        = 3
    SHIPPING    = 4

    def ueify(self):
        return "DebugGame" if self == Variant.DEBUGGAME else self.name.title()

    @staticmethod
    def parse(external:str) -> "Variant":
        try: return Variant[external.upper()]
        except: raise ValueError(f"Unknown build variant '{external}'")

#-------------------------------------------------------------------------------
class TargetType(Enum):
    UNKNOWN     = 0x00
    PROGRAM     = 0x01
    EDITOR      = 0x02
    GAME        = 0x03
    CLIENT      = 0x04
    SERVER      = 0x05

    @staticmethod
    def parse(external:str) -> "TargetType":
        try: return TargetType[external.upper()]
        except: raise ValueError(f"Unknown target type '{external}'")

#-------------------------------------------------------------------------------
class ContextType(Enum):
    BRANCH      = 0 # context with no active project
    PROJECT     = 1 # active project with Engine/ up the tree
    FOREIGN     = 2 # active project with Engine/ located elsewhere (i.e. installed)



# {{{1 ufs-item-----------------------------------------------------------------
class _UfsItem(Path):
    _mount:"_UfsMount"

    def __new__(cls, mount:"_UfsMount", *args) -> "_UfsItem":
        cls = _WindowsUfsItem if os.name == "nt" else _PosixUfsItem
        instance = super().__new__(cls, *args)
        instance._mount = mount
        return instance

    def get_mount(self) -> "_UfsMount":
        return self._mount

class _WindowsUfsItem(WindowsPath, _UfsItem):
    pass

class _PosixUfsItem(PosixPath, _UfsItem):
    pass



# {{{1 ufs_mount ---------------------------------------------------------------
class _UfsMount(object):
    _dir    :Path
    _node   :"_UfsNode"

    def __init__(self, dir:Path, node:"_UfsNode"):
        self._dir = dir
        self._node = node

    def __fspath__(self):
        return str(self._dir)

    def get_dir(self) -> Path:
        return self._dir

    def get_node(self) -> "_UfsNode":
        return self._node

    def glob(self, pattern:str) -> Iterator[_UfsItem]:
        for x in self.get_dir().glob(pattern):
            yield _UfsItem(self, x)

    def get_item(self, file_path:str) -> _UfsItem|None:
        dest = self.get_dir() / file_path
        if dest.is_file():
            return _UfsItem(self, dest)



# {{{1 build -------------------------------------------------------------------
class _Build(object):
    _receipt_path   :Path
    _variant        :Variant
    _ufs_mount      :_UfsMount
    _platform       :str

    def __init__(self, receipt_path:Path, variant:Variant, platform:str, ufs_mount:_UfsMount):
        self._receipt_path = receipt_path
        self._variant = variant
        self._platform = platform
        self._ufs_mount = ufs_mount

    def get_variant(self) -> Variant:
        return self._variant

    def get_platform(self) -> str:
        return self._platform

    def get_binary_path(self) -> Path|None:
        with self._receipt_path.open("rt") as x:
            try:
                import json
                target = json.load(x)
            except:
                return

        binary_path = target.get("Launch", None)
        if not binary_path:
            return

        eng_node = proj_node = self._ufs_mount.get_node()
        while parent_node := eng_node.get_parent_node():
            eng_node = parent_node

        binary_path = binary_path.replace("$(ProjectDir)", str(proj_node.get_dir()))
        binary_path = binary_path.replace("$(EngineDir)", str(eng_node.get_dir()))

        ret = Path(binary_path)
        return ret if ret.exists() else None



# {{{1 target ------------------------------------------------------------------
class _Target(object):
    _type       :TargetType
    _name       :str
    _ufs_mount  :_UfsMount

    def __init__(self, name:str, type:TargetType, ufs_mount:_UfsMount):
        self._name = name
        self._type = type
        self._ufs_mount = ufs_mount

    def get_name(self) -> str:
        return self._name

    def get_type(self) -> TargetType:
        return self._type

    def is_project_target(self) -> bool:
        return isinstance(self._ufs_mount.get_node(), _Project)

    def get_build(self, variant:Variant=Variant.DEVELOPMENT, platform:str="") -> _Build|None:
        platform = platform or Platform.get_host()

        path = f"Binaries/{platform}/{self.get_name()}"
        if variant != Variant.DEVELOPMENT:
            path += "-" + platform + "-" + variant.ueify()
        path += ".target"

        if receipt := next(self._ufs_mount.glob(path), None):
            return _Build(receipt, variant, platform, self._ufs_mount)



# {{{1 ufs_node ----------------------------------------------------------------
class _UfsNode(object):
    _parent :"_UfsNode"
    _dir    :Path

    def __init__(self, dir:Path, parent:"_UfsNode"=None):
        self._parent = parent
        self._dir = dir

    def get_parent_node(self) -> "_UfsNode|None":
        return self._parent

    def get_dir(self) -> Path:
        return self._dir

    def get_root_mount(self) -> _UfsMount:
        return _UfsMount(self._dir, self)

    def read_mounts(self, *, ancestors:bool=True, shallow:bool=False, platforms:bool=True) -> Iterator[_UfsMount]:
        if not shallow:
            for dir in self._dir.glob("Restricted/*"):
                yield _UfsMount(dir, self)
                if platforms:
                    yield from (_UfsMount(x, self) for x in dir.glob("Platforms/*"))

            if platforms:
                yield from (_UfsMount(x, self) for x in self._dir.glob("Platforms/*"))

        yield self.get_root_mount()

        if ancestors and self._parent:
            yield from self._parent.read_mounts(ancestors=ancestors, shallow=shallow, platforms=platforms)

    def glob(self, pattern:str, *, ancestors:bool=True, shallow:bool=False) -> Iterator[_UfsItem]:
        for mount in self.read_mounts(ancestors=ancestors, shallow=shallow):
            yield from mount.glob(pattern)

    def _read_targets(self, targets_dir:str, **kwargs) -> Iterator[_Target]:
        def parse_target_cs(target_cs):
            def find_line(regex, line_iter):
                for line in line_iter:
                    m = re.search(regex, line)
                    if m: return m.group(1)

            with target_cs.open("rt") as cs:
                line_iter = (x for x in cs)
                name = find_line(r"class\s+(\S+)Target[\s:]", line_iter)
                if name:
                    type = find_line(r"=\s*TargetType\.(.+)\s*;", line_iter)
                    return name, type or "UNKNOWN"

        pattern = str(Path(targets_dir) / "*.Target.cs")
        for target_cs in self.glob(pattern, **kwargs):
            if info := parse_target_cs(target_cs):
                name, type = info
                type = TargetType.parse(type)
                yield _Target(name, type, target_cs.get_mount())

    def get_target_by_type(self, target_type:TargetType) -> _Target|None:
        if target_type.value <= TargetType.PROGRAM.value:
            return

        for target in self._read_targets("Source"):
            if target.get_type() == target_type:
                return target

    def get_target_by_name(self, target_name:str) -> _Target|None:
        lower_name = target_name.lower()

        for dir in ("Source/Programs/*", "Source"):
            for target in self._read_targets(dir):
                if lower_name == target.get_name().lower():
                    return target

        node = self
        while next_node := node.get_parent_node():
            node = next_node

        return _Target(target_name, TargetType.UNKNOWN, node.get_root_mount())



# {{{1 config ------------------------------------------------------------------
class _Config(object):
    _ufs:_UfsNode

    def __init__(self, ufs:_UfsNode):
        self._ufs = ufs

    def _load_hives(self, category:str, *, override:str="") -> ini.Ini:
        ret = ini.Ini()

        if not override:
            hive_names = (
                f"Default{category}.ini",
                f"Base{category}.ini",
            )
            visit_platforms = False
        else:
            hive_names = (
                f"{override}{category}.ini",
                f"{override}{category}.ini",
                f"Base{override}{category}.ini",
            )
            visit_platforms = True

        node = self._ufs
        for hive_name in iter(hive_names):
            for mount in node.read_mounts(ancestors=False, platforms=visit_platforms):
                if ufs_item := mount.get_item("Config/" + hive_name):
                    with ufs_item.open("rt") as ini_file:
                        ret.load(ini_file)

            if next_node := node.get_parent_node():
                node = next_node

        return ret

    def get(self, category:str, section:str="", key:str="", *, override:str="") -> ini.Ini|ini._Struct|ini._Value|None:
        ret = self._load_hives(category, override=override)

        if section:
            if (ret := getattr(ret, section, None)) and key:
                ret = getattr(ret, key, None)

        return ret



# {{{1 engine ------------------------------------------------------------------
class _Engine(_UfsNode):
    def __init__(self, dir:Path):
        super().__init__(dir)

    def get_ubt(self) -> BuildTool:
        return BuildTool(self)

    def get_info(self) -> dict:
        ret = {
            "MajorVersion" : self.get_version_major(),
            "MinorVersion" : 0,
            "PatchVersion" : 0,
            "BranchName"   : "Error:Build.Version",
            "Changelist"   : 0,
        }

        build_version_path = self._dir / "Build/Build.version"
        if build_version_path.is_file():
            with build_version_path.open() as x:
                try:
                    import json
                    ret.update(json.load(x))
                except:
                    pass

        return ret

    def get_version_major(self) -> int:
        key_path = self._dir / "Source/Developer/Windows/WindowsNoEditorTargetPlatform/WindowsNoEditorTargetPlatform.Build.cs"
        return 4 if key_path.is_file() else 5

    def get_version_full(self) -> tuple:
        info = self.get_info()
        return (info["MajorVersion"], info["MinorVersion"], info["PatchVersion"])



# {{{1 project -----------------------------------------------------------------
class _Project(_UfsNode):
    _path   :Path
    _engine :_Engine

    def __init__(self, path:Path, engine:_Engine):
        super().__init__(path.parent, engine)
        self._path = path
        self._engine = engine

    def get_path(self) -> Path:
        return self._path

    def get_name(self) -> Path:
        return self._path.stem

    def get_engine(self) -> _Engine:
        return self._engine

    def get_target_by_type(self, target_type:TargetType, *, create_temp:bool=True) -> _Target|None:
        if target_type.value <= TargetType.PROGRAM.value:
            return

        for target in self._read_targets("Source", ancestors=False, shallow=True):
            if target.get_type() == target_type:
                return target

        type_name = target_type.name.title()
        target_name = self.get_name()
        if target_type != TargetType.GAME:
            target_name += type_name

        target = _Target(target_name, target_type, self.get_root_mount())
        if create_temp and Platform.get_host() != "Mac":
            self._create_temp_target(target)

        return target

    def _create_temp_target(self, target:_Target) -> None:
        int_dir = self.get_dir() / "Source"
        int_dir.mkdir(parents=True, exist_ok=True)

        type_name = target.get_type().name.title()
        target_name = target.get_name()

        dot_cs_path = int_dir / (target_name + ".Target.cs")
        if dot_cs_path.is_file():
            return

        game_target = "UnrealGame"
        if self._engine.get_version_major() <= 4:
            game_target = "UE4Game"

        with dot_cs_path.open("wt") as dot_cs:
            dot_cs.write(
                "// Created by ushell to make blueprint projects work end to end.\n"
                "// Placed in Source/ because UAT cleans Intermediate/Source/\n"
                "using UnrealBuildTool;\n"
                "public class " + target_name + "Target : TargetRules {\n"
                "public " + target_name + "Target(TargetInfo Target) : base(Target) {\n"
                "Type = TargetType." + type_name + ";\n"
                "ExtraModuleNames.AddRange(new string[] {\"" + game_target + "\"} ); }}\n"
            )



# {{{1 branch ------------------------------------------------------------------
class _Branch(object):
    _dir:Path

    def __init__(self, dir:Path):
        self._dir = dir

    def get_name(self) -> str:
        return self._dir.name

    def get_dir(self) -> Path:
        return self._dir

    def find_project(self, name:str) -> Path|None:
        name = name.lower()
        for project_path in self.read_projects():
            if project_path.stem.lower() == name:
                return project_path

    def read_projects(self) -> Iterator[Path]:
        def read_dir(dir:Path):
            for sub_dir in (x for x in dir.glob("*") if x.is_dir()):
                yield from sub_dir.glob("*.uproject")

        def read_uprojectdirs(uprojdirs_path:Path) -> Iterator[Path]:
            with uprojdirs_path.open() as file:
                root_dir = uprojdirs_path.parent
                for line in file:
                    line = line.strip()
                    for prefix in (";", "Templates", "Engine/"):
                        if line.startswith(prefix):
                            break
                    else:
                        yield from read_dir(root_dir / line)

        for item in self._dir.glob("*.uprojectdirs"):
            yield from read_uprojectdirs(item)



# {{{1 platform-provider -------------------------------------------------------
class _PlatformProvider(object):
    _temp_ctx   :"Context"
    _provisions :dict[str, Path]

    def __init__(self, temp_ctx:"Context"):
        self._temp_ctx = temp_ctx
        self._provisions = None

    @classmethod
    def _get_host_platform_name(self) -> str:
        if hasattr(self, "_host_name"):
            return self._host_name

        if os.name == "nt":
            self._host_name = "win64"
        else:
            import sys
            if sys.platform == "linux": self._host_name = "linux"
            elif sys.platform == "darwin": self._host_name = "mac"
            else: raise EnvironmentError("Unknown host platform '{sys.platform}'")

        return self._host_name

    def _get_provisions(self) -> dict[str, Path]:
        if self._provisions is not None:
            return self._provisions

        provisions = self._provisions = {}

        # Built-in platforms
        host_name = self._get_host_platform_name()
        for item in Path(__file__).parent.glob(f"platforms/{host_name}/*.py"):
            if not item.name.startswith("_"):
                provisions[item.stem] = item

        if host_name != "win64":
            return provisions

        # Use Engine/Platforms if we are hosted in a UE branch
        for candidate in Path(__file__).parent.parent.parents:
            if candidate.name == "Engine":
                candidate = (candidate / "Platforms")
                if candidate.is_dir():
                    break
        else:
            return provisions

        for platform_dir in (x for x in candidate.glob("*") if x.is_dir()):
            extras_dir = platform_dir / "Extras/ushell"
            if not extras_dir.is_dir():
                continue

            for item in extras_dir.glob("platform_*.py"):
                provisions[item.stem[9:]] = item

        return provisions

    def get_platform(self, name:str|None=None) -> Platform|None:
        host_name = self._get_host_platform_name()
        name = (name or host_name).lower()

        try:
            path = f"unreal.platforms.{host_name}.{name}"
            py_path = self._get_provisions()[name]
            import importlib.util
            spec = importlib.util.spec_from_file_location(path , py_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
        except (KeyError, ModuleNotFoundError):
            return

        return module.Platform(self._temp_ctx)

    def read_platform_names(self) -> Iterator[str]:
        yield from self._get_provisions().keys()

# {{{1 context -----------------------------------------------------------------
class Context(object):
    _type          :ContextType
    _engine_dir    :Path
    _project_path  :Path
    _platforms     :_PlatformProvider

    def __init__(self, primary_dir:str):
        primary_dir:Path = Path(primary_dir).resolve()

        # If a .uproject* was given instead the strip it off.
        if primary_dir.suffix.startswith(".uproject"):
            if primary_dir.is_file():
                return self._initialize(primary_dir)
            raise EnvironmentError(f"Project file not found '{primary_dir}'")

        # Walk up from 'primary_dir' and find something to build a context from
        if primary_dir.is_dir():
            iter_dir:Path = primary_dir
            while not iter_dir.samefile(iter_dir.anchor):
                if uproj_path := next(iter_dir.glob("*.uproject*"), None):
                    return self._initialize(uproj_path)
                iter_dir = iter_dir.parent
            raise EnvironmentError(f"Unable to establish an Unreal context from directory '{primary_dir}'")

        raise EnvironmentError(f"Unable to establish an Unreal context with '{primary_dir}'")

    def _initialize(self, uproj_path:Path) -> None:
        if uproj_path.suffix == ".uproject":
            return self._initialize_project(uproj_path)
        else:
            return self._initialize_noproject(uproj_path)

    def _initialize_project(self, uproj_path:Path) -> None:
        self._project_path = uproj_path

        # Find engine. First look up the tree from the project for an Engine/ dir
        iter_dir:Path = uproj_path.parent.parent
        while not iter_dir.samefile(iter_dir.anchor):
            candidate = iter_dir / "Engine"
            if candidate.is_dir() and (candidate / "Config").is_dir():
                self._type = ContextType.PROJECT
                self._engine_dir = candidate
                return
            iter_dir = iter_dir.parent

        # Next inspect the .uproject file for evidence of an engine
        uproj_data = None
        with uproj_path.open("rt") as x:
            try:
                import json
                uproj_data = json.load(x)
            except:
                pass

        if uproj_data and (association := uproj_data.get("EngineAssociation")):
            if engine_dir := _resolve_association(association):
                engine_dir /= "Engine"
                if engine_dir.is_dir():
                    self._type = ContextType.FOREIGN
                    self._engine_dir = engine_dir
                    return

        raise EnvironmentError(f"Unable to locate the engine for project '{uproj_path}'")

    def _initialize_noproject(self, uprojdirs_path:Path) -> None:
        # Engine/ is expected to be a sibling of the .uprojectdirs file
        engine_dir = uprojdirs_path.parent / "Engine"
        if not engine_dir.is_dir():
            raise EnvironmentError(f"Folder '{uprojdirs_path.parent}/Engine/' does not exist")

        # We've everything for a valid context now
        self._type = ContextType.BRANCH
        self._engine_dir = engine_dir
        self._project_path = None

    def _get_ufs_node(self) -> _UfsNode:
        return self.get_project() or self.get_engine()

    def get_type(self) -> ContextType:
        return self._type

    def get_name(self) -> str:
        return self._engine_dir.parent.name or "UnrealEngine"

    def get_primary_path(self) -> Path:
        return self._project_path if self._project_path else self.get_branch().get_dir()

    def get_project(self) -> _Project|None:
        if self._type != ContextType.BRANCH:
            return _Project(self._project_path, self.get_engine())

    def get_config(self) -> _Config:
        return _Config(self._get_ufs_node())

    def get_engine(self) -> _Engine:
        return _Engine(self._engine_dir)

    def get_branch(self, *, must_exist:bool=False) -> _Branch|None:
        if self._type != ContextType.FOREIGN:
            return _Branch(self._engine_dir.parent)

        if must_exist:
            raise EnvironmentError(f"No valid branch found for '{self._engine_dir}'")

    def get_platform_provider(self) -> _PlatformProvider:
        if not hasattr(self, "_platforms"):
            self._platforms = _PlatformProvider(self)
        return self._platforms

    def glob(self, pattern:str, shallow:bool=False) -> Iterator[_UfsItem]:
        yield from self._get_ufs_node().glob(pattern, shallow=shallow)

    def get_target_by_type(self, target_type:TargetType) -> _Target|None:
        return self._get_ufs_node().get_target_by_type(target_type)

    def get_target_by_name(self, name:str) -> _Target:
        return self._get_ufs_node().get_target_by_name(name)

# vim: foldlevel=1 foldmethod=marker
