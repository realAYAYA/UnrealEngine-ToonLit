# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import subprocess as sp

#-------------------------------------------------------------------------------
class _PlatformBase(unreal.Platform):
    def _read_env(self):
        yield from ()

    def _get_version_ue4(self):
        import re

        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Windows/UEBuildWindows.cs"

        needles = (
            ("DefaultWindowsSdkVersion", re.compile(r'DefaultWindowsSdkVersion\s+=\s+("|[\w.]+\("|)([^"]+)"?\)?\s*;')),
            ("VersionNumber", re.compile(r'^\s+VersionNumber.Parse\(("([^"]+))')),
        )
        versions = []
        try:
            with open(dot_cs, "rt") as lines:
                for line in lines:
                    for seed, needle in needles:
                        if seed not in line:
                            continue
                        m = needle.search(line)
                        if m:
                            versions.append(m.group(2))
        except FileNotFoundError:
            return

        return self._get_version_string(versions)

    def _get_version_ue5(self):
        # Extract the Windows SDK version
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Windows"
        version = self._get_version_ue5_path(dot_cs / "MicrosoftPlatformSDK.Versions.cs")
        return version or self._get_version_ue5_path(dot_cs / "MicrosoftPlatformSDK.cs")

    def _get_version_ue5_path(self, dot_cs):
        import re
        needle = re.compile(r'VersionNumber.Parse\("([\d.]+)"')

        versions = []
        try:
            with open(dot_cs, "rt") as lines:
                for line in (x for x in lines if "VersionNumber" in x):
                    if m := needle.search(line):
                        versions.append(m.group(1))
                        break
        except FileNotFoundError:
            pass

        if not versions:
            if sdk_version := self._get_version_helper_ue5(dot_cs):
                versions.append(sdk_version)
            else:
                return

        # Extract the Visual Studio toolchain version
        vs_version = self._get_vs_toolchain_version_string(dot_cs)
        if not vs_version:
            vs_version = self._get_vs_toolchain_version_string(dot_cs.parent / "UEBuildWindows.cs")
        if not vs_version:
            return
        versions.append(vs_version)

        return self._get_version_string(versions)

    def _get_vs_toolchain_version_string(self, dot_cs):
        import re
        try:
            with open(dot_cs, "rt") as lines:
                line_iter = iter(lines)
                for line in line_iter:
                    if "PreferredVisualCppVersions" in line:
                        break
                else:
                    return

                version = None
                for line in line_iter:
                    if "}" in line:
                        break
                    elif m := re.match(r'^\s+VersionNumberRange\.Parse\(.+, "([^"]+)', line):
                        version = m.group(1)
                        break

                return version
        except FileNotFoundError:
            return

    def _get_version_string(self, versions):
        msvc_versions = list(x for x in versions if x >= "14.")
        if not msvc_versions:
            return

        sdk_versions = list(x for x in versions if x.startswith("10."))
        if not sdk_versions:
            return

        return max(sdk_versions) + "-" + max(msvc_versions)

#-------------------------------------------------------------------------------
class Platform(_PlatformBase):
    name = "Win64"
    config_name = "Windows"
    autosdk_name = None
    vs_transport = "Default"

    def _get_cook_form(self, target):
        if target == "game":   return "Windows" if self.get_unreal_context().get_engine().get_version_major() > 4 else "WindowsNoEditor"
        if target == "client": return "WindowsClient"
        if target == "server": return "WindowsServer"

    def _launch(self, exec_context, stage_dir, binary_path, args):
        if stage_dir:
            midfix = "Engine";
            if project := self.get_unreal_context().get_project():
                midfix = project.get_name()
            base_dir = stage_dir + midfix + "/Binaries/Win64"
            if not os.path.isdir(base_dir):
                raise EnvironmentError(f"Failed to find base directory '{base_dir}'")
            args = (*args, "-basedir=" + base_dir)

        cmd = exec_context.create_runnable(binary_path, *args)
        cmd.launch()
        return (cmd.get_pid(), None)

    def _kill(self, target):
        target_name = None
        context = self.get_unreal_context()
        if target == "editor":
            ue_version = context.get_engine().get_version_major()
            target_name = "UE4Editor" if ue_version == 4 else "UnrealEditor"
        elif target:
            target_type = unreal.TargetType.parse(target)
            target_name = context.get_target_by_type(target_type).get_name()
        if target_name:
            print(f"Terminating {target_name}*.exe")
            sp.run(rf'taskkill.exe /f /fi "imagename eq {target_name}*"')
        else:
            self._kill("client")
            self._kill("game")
