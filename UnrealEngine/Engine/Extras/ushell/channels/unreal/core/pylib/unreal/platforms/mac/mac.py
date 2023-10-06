# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal

#-------------------------------------------------------------------------------
class Platform(unreal.Platform):
    name = "Mac"

    def _read_env(self):
        yield from ()

    def _get_version_ue4(self):
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Mac/MacToolChain.cs"
        return Platform._get_version_helper_ue4(dot_cs, "MinMacOSVersion")

    def _get_version_ue5(self):
        dot_cs = "Source/Programs/UnrealBuildTool/Platform/Mac/ApplePlatformSDK"
        version = self._get_version_helper_ue5(dot_cs + ".Versions.cs")
        return version or self._get_version_helper_ue5(dot_cs + ".cs")

    def _get_cook_form(self, target):
        if target == "game":   return "Mac" if self.get_unreal_context().get_engine().get_version_major() > 4 else "MacNoEditor"
        if target == "client": return "MacClient"
        if target == "server": return "MacServer"

    def _launch(self, exec_context, stage_dir, binary_path, args):
        if stage_dir:
            bins_index = binary_path.find("/Binaries/Mac/")
            if bins_index >= 0:
                base_dir = stage_dir
                base_dir += os.path.basename(binary_path[:bins_index])
                base_dir += "/Binaries/Mac/"
            else:
                raise EnvironmentError(f"Unable to calculate base directory from '{binary_path}'")
            args = (*args, "-basedir=" + base_dir)

        cmd = exec_context.create_runnable(binary_path, *args)
        cmd.launch()
        return True
