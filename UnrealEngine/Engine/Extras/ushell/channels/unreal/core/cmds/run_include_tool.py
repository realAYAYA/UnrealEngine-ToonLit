# Copyright Epic Games, Inc. All Rights Reserved.

import unreal
import tempfile
import unrealcmd
import subprocess

#-------------------------------------------------------------------------------
def _build_include_tool(exec_context, ue_context):
    # Work out where MSBuild is
    vswhere = (
        "vswhere.exe",
        "-latest",
        "-find MSBuild/*/Bin/amd64/MSBuild.exe",
    )
    proc = subprocess.Popen(" ".join(vswhere), stdout=subprocess.PIPE)
    msbuild_exe = next(iter(proc.stdout.readline, b""), b"").decode().rstrip()
    proc.stdout.close()

    msbuild_exe = msbuild_exe or "MSBuild.exe"

    # Get MSBuild to build IncludeTool
    engine = ue_context.get_engine()
    csproj_path = engine.get_dir() / "Source/Programs/IncludeTool/IncludeTool/IncludeTool.csproj"
    msbuild_args = (
        "/target:Restore;Build",
        "/property:Configuration=Release,Platform=AnyCPU",
        "/v:m",
        str(csproj_path)
    )
    return exec_context.create_runnable(msbuild_exe, *msbuild_args).run() == 0

#-------------------------------------------------------------------------------
def _run_include_tool(exec_context, ue_context, target, variant, *inctool_args):
    temp_dir = tempfile.TemporaryDirectory()
    engine = ue_context.get_engine()
    it_bin_path = engine.get_dir() / "Binaries/DotNET/IncludeTool/IncludeTool.exe"
    if not it_bin_path.is_file():
        it_bin_path = engine.get_dir() / "Binaries/DotNET/IncludeTool.exe"
    it_args = (
        "-Mode=Scan",
        "-Target=" + target,
        "-Platform=Linux",
        "-Configuration=" + variant,
        "-WorkingDir=" + temp_dir.name,
        *inctool_args,
    )
    return exec_context.create_runnable(str(it_bin_path), *it_args).run()

#-------------------------------------------------------------------------------
class RunIncludeTool(unrealcmd.MultiPlatformCmd):
    """ Runs IncludeTool.exe """
    target      = unrealcmd.Arg(str, "The target to run the IncludeTool on")
    variant     = unrealcmd.Arg("development", "Build variant to be processed (default=development)")
    inctoolargs = unrealcmd.Arg([str], "Arguments to pass through to IncludeTool")

    def complete_target(self, prefix):
        return ("editor", "game", "client", "server")

    def main(self):
        self.use_platform("linux")
        exec_context = self.get_exec_context()
        ue_context = self.get_unreal_context()

        # Establish the target to use
        try:
            target = unreal.TargetType[self.args.target.upper()]
            target = ue_context.get_target_by_type(target).get_name()
        except:
            target = self.args.target

        # Build
        self.print_info("Building IncludeToole")
        if not _build_include_tool(exec_context, ue_context):
            self.print_error("Failed to build IncludeTool")
            return False

        # Run
        variant = self.args.variant
        self.print_info("Running on", target + "/" + variant)
        return _run_include_tool(exec_context, ue_context, target, variant, *self.args.inctoolargs)
