# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import shutil
import unreal
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class Platform(unreal.Platform):
    name = "Android"
    config_section = "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"

    def _read_env(self):
        version = self.get_version()
        prefix = f"Android/{version}/"
        env_vars = {}

        # sdk
        env_vars["ANDROID_HOME"] = "android-sdk-windows/" if version == "-22" else ""

        # jdk
        if version <= "-22":
            env_vars["JAVA_HOME"] = "jdk1.8.0_77/"
        elif version <= "-25":
            env_vars["JAVA_HOME"] = "jre/"
        else:
            env_vars["JAVA_HOME"] = "jbr/"

        # ndk
        if version <= "-22":
            env_vars["NDKROOT"] = "android-ndk-r14b/"
        else:
            if sdks_root := os.getenv("UE_SDKS_ROOT"):
                from pathlib import Path
                ndk_dir = Path(sdks_root)
                ndk_dir /= "Host" + unreal.Platform.get_host()
                ndk_dir /= prefix
                ndk_dir /= "ndk"
                if ndk_dir.is_dir():
                    ndk_ver = max(x.name for x in ndk_dir.glob("*") if x.is_dir())
                    env_vars["NDKROOT"] = "ndk/" + ndk_ver
            env_vars.setdefault("NDKROOT", "ndk/")

        # dispatch
        for env_var, template in env_vars.items():
            value = os.getenv(env_var)
            if not value:
                value = prefix + template
            yield env_var, value

    def _get_version_ue4(self):
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Android/UEBuildAndroid.cs"
        try:
            import re
            with open(dot_cs, "rt") as cs_in:
                cs_in.read(8192)
                lines = iter(cs_in)
                next(x for x in lines if "override string GetRequiredSDKString()" in x)
                next(lines) # {
                for i in range(5):
                    line = next(lines)
                    if m := re.search(r'return "(-\d+)"', line):
                        return m.group(1)
        except (StopIteration, FileNotFoundError):
            pass

    def _get_version_ue5(self):
        func = "GetAutoSDKDirectoryForMainVersion"
        dot_cs = "Source/Programs/UnrealBuildTool/Platform/Android/AndroidPlatformSDK"
        version = self._get_version_helper_ue5(dot_cs + ".Versions.cs", func)
        return version or self._get_version_helper_ue5(dot_cs + ".cs", func)

    def _get_android_home(self):
        out = next((v for k,v in self._read_env() if k == "ANDROID_HOME"), None)
        if not out:
            return

        for prefix in ("", unreal.Platform.get_sdks_dir()):
            candidate = Path(prefix) / out
            if candidate.is_dir():
                return candidate

    def _get_adb(self):
        home_dir = self._get_android_home()
        return (home_dir / "platform-tools/adb") if home_dir else "adb"

    def _get_aapt(self):
        home_dir = self._get_android_home()
        if not home_dir:
            return "aapt"

        try:
            build_tools_dir = home_dir / "build-tools"
            latest = max(x.name for x in build_tools_dir.glob("*"))
            return build_tools_dir / latest / "aapt"
        except ValueError:
            return

    def _get_cook_form(self, target):
        if target == "game":   return "Android_ASTC"
        if target == "client": return "Android_ASTCClient"

    def _get_cook_flavor(self):
        return "ASTC"

    def _get_package_name(self):
        config = self.get_unreal_context().get_config()
        if ret := str(config.get("Engine", Platform.config_section, "PackageName")):
            return ret
        raise ValueError(f"Failed querying '{Platform.config_section}/PackageName'")

    def _launch(self, exec_context, stage_dir, binary_path, args):
        print("!!\n!! NOTE: .run does not currently use locally-built executables\n!!\n")

        package_name = self._get_package_name()
        intent = package_name + "/com.epicgames.unreal.SplashActivity"
        print("Package:", package_name)
        print("Intent: ", intent)
        print()

        args = "'" + " ".join(x.replace("\"", "\\\"") for x in args) + "'"
        adb = self._get_adb()
        cmd = (str(adb), "-d", "shell", "am", "start", "--es", "cmdline", args, intent)
        print(*cmd)

        # subprocess.run() messes with quotes on Windows so we'll use os.system
        self._kill()
        os.system(" ".join(cmd))

        return True
        """ -- not yet implemented properly --
        # get device
        device = None
        for line in exec_context.create_runnable(adb, "-d", "devices"):
            if line.endswith("device") and not device:
                device = line.split()[0]
        print(device)

        # get pid
        pid = -1
        for line in exec_context.create_runnable(adb, "-d", "shell", "run-as", package_name, "ps"):
            print(line)

        return (device, pid)
        """

    def _kill(self, target=""):
        package_name = self._get_package_name()
        adb = self._get_adb()
        cmd = (adb, "-d", "shell", "am", "force-stop", str(package_name))
        print(*cmd)
        sp.run(cmd)
