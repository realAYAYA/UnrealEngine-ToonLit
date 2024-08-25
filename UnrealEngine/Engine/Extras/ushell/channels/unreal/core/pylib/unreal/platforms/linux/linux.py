# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal

#-------------------------------------------------------------------------------
class PlatformBase(unreal.Platform):
    name = "Linux"
    autosdk_name = "Linux_x64"
    env_var = "LINUX_MULTIARCH_ROOT"

    def _get_version_ue4(self):
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Linux/UEBuildLinux.cs"
        return Platform._get_version_helper_ue4(dot_cs, "ExpectedSDKVersion")

    def _get_version_ue5(self):
        dot_cs = "Source/Programs/UnrealBuildTool/Platform/Linux/LinuxPlatformSDK"
        version = self._get_version_helper_ue5(dot_cs + ".Versions.cs")
        return version or self._get_version_helper_ue5(dot_cs + ".cs")

    def _get_cook_form(self, target):
        if target == "game":   return "Linux" if self.get_unreal_context().get_engine().get_version_major() > 4 else "LinuxNoEditor"
        if target == "client": return "LinuxClient"
        if target == "server": return "LinuxServer"

#-------------------------------------------------------------------------------
class Platform(PlatformBase):
    def _read_env(self):
        env_var = PlatformBase.env_var
        value = os.getenv(env_var)
        if value:
            yield env_var, value
            return

        version = self.get_version()
        extras_dir = self.get_unreal_context().get_engine().get_dir() / "Extras"
        for suffix in ("AutoSDK/", "ThirdPartyNotUE/SDKs/"):
            value = extras_dir / suffix / f"HostLinux/Linux_x64/{version}/"
            if (value / "x86_64-unknown-linux-gnu/bin").is_dir():
                yield env_var, value
                return

        yield env_var, f"Linux_x64/{version}/"

    def _launch(self, exec_context, stage_dir, binary_path, args):
        if stage_dir:
            midfix = "Engine";
            if project := self.get_unreal_context().get_project():
                midfix = project.get_name()
            base_dir = stage_dir + midfix + "/Binaries/Linux"
            if not os.path.isdir(base_dir):
                raise EnvironmentError(f"Failed to find base directory '{base_dir}'")
            args = (*args, "-basedir=" + base_dir)

        ue_context = self.get_unreal_context()
        engine_dir = ue_context.get_engine().get_dir()
        engine_bin_dir = str(engine_dir / "Binaries/Linux")
        if ld_lib_path := os.getenv("LD_LIBRARY_PATH"):
            ld_lib_path += ":" + str(engine_bin_dir)
        else:
            ld_lib_path = str(engine_bin_dir)
        env = exec_context.get_env()
        env["LD_LIBRARY_PATH"] = ld_lib_path

        cmd = exec_context.create_runnable(binary_path, *args)
        cmd.launch()
        return True
