# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import unrealcmd

#-------------------------------------------------------------------------------
def _find_autosdk_llvm(autosdk_path):
    path = f"{autosdk_path}/Host{unreal.Platform.get_host()}/Win64/LLVM"

    best_version = None
    if os.path.isdir(path):
        with os.scandir(path) as dirs:
            best_version = max((x.name for x in dirs if x.is_dir()), default=None)

    path += f"/{best_version}/"
    if not os.path.isfile(path + "bin/clang-cl.exe"):
        best_version = path = None

    return (best_version, path)

#-------------------------------------------------------------------------------
def _can_execute_clang():
    import subprocess
    try: subprocess.run(("clang-cl", "--version"), stdout=subprocess.DEVNULL)
    except: return False
    return True



#-------------------------------------------------------------------------------
class ClangDb(unrealcmd.Cmd):
    """ Generates a Clang database (compile_commands.json). If there is an active
    project then the database is generated in context of that project. By default
    the editor target is used. """
    target    = unrealcmd.Arg("", "Target to use when generating the database")
    ubtargs   = unrealcmd.Arg([str], "Arguments to pass onwards to UnrealBuildTool")
    platform  = unrealcmd.Opt("", "Platform to build a database for")

    @unrealcmd.Cmd.summarise
    def _run_ubt(self, target, platform, ubt_args):
        ue_context = self.get_unreal_context()
        ubt = ue_context.get_engine().get_ubt()
        if project := ue_context.get_project():
            ubt_args = ("-Project=" + str(project.get_path()), *ubt_args)

        exec_context = self.get_exec_context()
        for cmd, args in ubt.read_actions(target, platform, "Development", *ubt_args):
            cmd = exec_context.create_runnable(cmd, *args)
            cmd.run()
            ret = cmd.get_return_code()
            if ret:
                return ret

    def main(self):
        # Rough check for the presence of LLVM
        self.print_info("Checking for LLVM")
        autosdk_path = os.getenv("UE_SDKS_ROOT")
        print("UE_SDKS_ROOT:", autosdk_path)
        llvm_version, llvm_path = _find_autosdk_llvm(autosdk_path)
        if llvm_version:
            print("Found:", llvm_version, "at", llvm_path)
        else:
            print("No suitable AusoSDKs version found")
            print("Trying PATH ... ", end="")
            has_clang = _can_execute_clang()
            print("ok" if has_clang else "nope")
            if not has_clang:
                self.print_warning("Unable to locate Clang binary")

        ue_context = self.get_unreal_context()

        # Calculate how to invoke UBT
        target = self.args.target
        if not target:
            target = ue_context.get_target_by_type(unreal.TargetType.EDITOR)
            target = target.get_name()

        platform = unreal.Platform.get_host()
        if self.args.platform:
            platform = self.get_platform(self.args.platform).get_name()

        self.print_info("Database properties")
        print("Target:", target)
        print("Platform:", platform)

        ubt_args = (
            "-allmodules",
            "-Mode=GenerateClangDatabase",
            *self.args.ubtargs,
        )

        # Run UBT
        self.print_info("Running UnrealBuildTool")
        ret = self._run_ubt(target, platform, ubt_args)
        if ret:
            return ret

        # Print some information
        db_path = ue_context.get_engine().get_dir() / "../compile_commands.json"
        try: db_size = os.path.getsize(db_path)
        except: db_size = -1
        print()
        self.print_info("Database details")
        print("Path:", os.path.normpath(db_path))
        print("Size: %.02fKB" % (db_size / 1024 / 1024))
