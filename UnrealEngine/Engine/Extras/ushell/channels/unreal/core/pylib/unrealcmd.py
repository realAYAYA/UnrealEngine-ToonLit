# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import flow.cmd

#-------------------------------------------------------------------------------
def get_uproject_from_dir(start_dir):
    from pathlib import Path
    start_dir = Path(start_dir)

    for search_dir in (start_dir, *start_dir.parents):
        with os.scandir(search_dir) as files:
            try: project = next(x.path for x in files if x.name.endswith(".uproject"))
            except StopIteration: continue
            return project

    raise EnvironmentError(f"Unable to find a .uproject from '{start_dir}'")



#-------------------------------------------------------------------------------
Arg = flow.cmd.Arg
Opt = flow.cmd.Opt
class Cmd(flow.cmd.Cmd):
    def complete_platform(self, prefix):
        yield from self.read_platforms()

    def complete_variant(self, prefix):
        yield from (x.name.lower() for x in unreal.Variant)

    def __init__(self):
        super().__init__()
        self._project = None
        self._context = None

    def get_unreal_context(self):
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        path = session["uproject"] or "."
        self._context = self._context or unreal.Context(path)
        return self._context

    def read_platforms(self):
        platforms = self.get_unreal_context().get_platform_provider()
        yield from platforms.read_platform_names()

    def get_platform(self, name=None):
        ue_context = self.get_unreal_context()
        platforms = ue_context.get_platform_provider()
        if platform := platforms.get_platform(name):
            return platform

        available = ",".join(platforms.read_platform_names())
        raise ValueError(f"Unknown platform '{name}'. Valid list: {available}")

    def get_host_platform(self):
        return self.get_platform()



#-------------------------------------------------------------------------------
class MultiPlatformCmd(Cmd):
    noplatforms = Opt(False, "Disable discovery of available platforms")

    def __init__(self):
        super().__init__()
        self._platforms = []

    def use_all_platforms(self):
        self._platforms = list(self.read_platforms())

    def use_platform(self, platform_name):
        self._platforms.append(platform_name)

    def get_exec_context(self):
        context = super().get_exec_context()
        if self.args.noplatforms or not self._platforms:
            return context

        env = context.get_env()
        if len(self._platforms) == 1:
            name = self._platforms[0]
            platform = self.get_platform(name)
            self.print_info(name.title(), platform.get_version())
            for item in platform.read_env():
                try:
                    dir = str(item)
                    env[item.key] = dir
                except EnvironmentError:
                    dir = flow.cmd.text.red(item.get())
                print(item.key, "=", dir)
            return context

        if self.is_interactive():
            self.print_info("Establishing platforms")

        found = []
        failed = []
        for platform_name in self._platforms:
            platform = self.get_platform(platform_name)
            try:
                platform_env = {x.key:str(x) for x in platform.read_env()}
                env.update(platform_env)
                found.append(platform_name)
            except EnvironmentError:
                failed.append(platform_name)

        if self.is_interactive():
            if found:
                print("  found:", flow.cmd.text.green(" ".join(found)))
            if failed:
                out = "missing: "
                out += flow.cmd.text.light_red(" ".join(failed))
                out += flow.cmd.text.grey(" ('.info' for details)")
                print(out)

        return context
