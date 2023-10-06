# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal

#-------------------------------------------------------------------------------
class Platform(unreal.Platform):
    name = "IOS"

    def _read_env(self):
        yield from ()

    def _get_version_ue4(self):
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Mac/MacToolChain.cs"
        return Platform._get_version_helper_ue4(dot_cs, "MinMacOSVersion")

    def _get_version_ue5(self):
        dot_cs = "Source/Programs/UnrealBuildTool/Platform/Mac/ApplePlatformSDK"
        version = self._get_version_helper_ue5(dot_cs + "Versions.cs")
        return version or self._get_version_helper_ue5(dot_cs + ".cs")

    def _get_cook_form(self, target):
        if target == "game":   return "IOS"
        if target == "client": return "IOSClient"
