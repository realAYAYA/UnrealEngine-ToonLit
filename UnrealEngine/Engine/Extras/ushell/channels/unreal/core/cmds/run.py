# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import unreal
import unrealcmd
import unreal.cmdline
from pathlib import Path


#-------------------------------------------------------------------------------
def _get_ip():
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("172.31.255.255", 1))
        ret = s.getsockname()[0]
    except:
        ret = "127.0.0.1"
    finally:
        s.close()
    return ret



#-------------------------------------------------------------------------------
class _Attachable(object):
    attach = unrealcmd.Opt(False, "Attach a debugger to the launched process")

    def _get_debugger(self, name=None):
        name = name or os.getenv("USHELL_DEBUGGER")
        name = name or ("vs" if os.name == "nt" else "lldb")
        dbg_py_path = Path(__file__).parent.parent / f"debuggers/{name}.py"

        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location("", dbg_py_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            debugger_class = getattr(module, "Debugger", None)
            return debugger_class(self.get_unreal_context())
        except Exception as e:
            raise RuntimeError(f"Failed to load debugger '{dbg_py_path}' ({e})")

    def attach_to(self, pid, *, transport=None, host_ip=None):
        debugger = self._get_debugger()
        return debugger.attach(pid, transport, host_ip)

    def debug(self, cmd, *args):
        debugger = self._get_debugger()
        exec_context = self.get_exec_context()
        return debugger.debug(exec_context, cmd, *args)



#-------------------------------------------------------------------------------
class _RunCmd(unrealcmd.Cmd):
    _variant = unrealcmd.Arg("development", "Build variant to launch")
    runargs  = unrealcmd.Arg([str], "Arguments passed on to the process being launched")
    build    = unrealcmd.Opt(False, "Build the target prior to running it")

    def _build(self, target, variant, platform):
        if target.get_type() == unreal.TargetType.EDITOR:
            build_cmd = ("_build", "editor", variant)
        else:
            platform = platform or unreal.Platform.get_host()
            build_cmd = ("_build", "target", target.get_name(), platform, variant)

        import subprocess
        ret = subprocess.run(build_cmd)
        if ret.returncode:
            raise SystemExit(ret.returncode)

    def get_binary_path(self, target, platform=None):
        ue_context = self.get_unreal_context()

        if isinstance(target, str):
            target = ue_context.get_target_by_name(target)
        else:
            target = ue_context.get_target_by_type(target)

        if self.args.build:
            self._build(target, self.args.variant, platform)

        variant = unreal.Variant.parse(self.args.variant)
        if build := target.get_build(variant, platform):
            if build := build.get_binary_path():
                return build

        raise EnvironmentError(f"No {self.args.variant} build found for target '{target.get_name()}'")



#-------------------------------------------------------------------------------
class Editor(_RunCmd, _Attachable, unrealcmd.MultiPlatformCmd):
    """ Launches the editor """
    noproject   = unrealcmd.Opt(False, "Start the editor without specifying a project")
    variant     = _RunCmd._variant

    def main(self):
        self.use_all_platforms()

        ue_context = self.get_unreal_context()

        binary_path = self.get_binary_path(unreal.TargetType.EDITOR)

        args = (*unreal.cmdline.read_ueified(*self.args.runargs),)
        if (project := ue_context.get_project()) and not self.args.noproject:
            args = (project.get_path(), *args)

        wants_terminal = "-stdout" in (x.lower() for x in self.args.runargs)
        if wants_terminal and binary_path.suffix == "exe":
            binary_path = binary_path[:-4]
            binary_path += "-Cmd.exe"

        if self.args.attach:
            self.debug(binary_path, *args)
            return

        exec_context = self.get_exec_context()
        editor = exec_context.create_runnable(str(binary_path), *args);
        if wants_terminal or not self.is_interactive() or os.name != "nt":
            import uelogprinter
            printer = uelogprinter.Printer()
            printer.run(editor)
            return editor.get_return_code()
        else:
            editor.launch()



#-------------------------------------------------------------------------------
class Commandlet(_RunCmd, _Attachable, unrealcmd.MultiPlatformCmd):
    """ Runs a commandlet """
    commandlet = unrealcmd.Arg(str, "Name of the commandlet to run")
    variant    = _RunCmd._variant

    def complete_commandlet(self, prefix):
        ue_context = self.get_unreal_context()
        for dot_h in ue_context.glob("Source/Editor/UnrealEd/Classes/Commandlets/*.h"):
            if "Commandlet.h" in dot_h.name:
                yield dot_h.name[:-12]

    def main(self):
        self.use_all_platforms()

        binary_path = self.get_binary_path(unreal.TargetType.EDITOR)
        if binary_path.suffix == ".exe":
            binary_path = binary_path.parent / (binary_path.stem + "-Cmd.exe")

        args = (
            "-run=" + self.args.commandlet,
            *unreal.cmdline.read_ueified(*self.args.runargs),
        )
        if project := self.get_unreal_context().get_project():
            args = (project.get_path(), *args)

        if self.args.attach:
            self.debug(str(binary_path), *args)
            return

        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable(str(binary_path), *args);
        if not self.is_interactive():
            cmd.run()
        else:
            import uelogprinter
            printer = uelogprinter.Printer()
            printer.run(cmd)
        return cmd.get_return_code()



#-------------------------------------------------------------------------------
class _ProgramTarget(_RunCmd, _Attachable, unrealcmd.MultiPlatformCmd):
    def _main_impl(self):
        self.use_all_platforms()

        binary_path = self.get_binary_path(self.args.target)
        args = (*unreal.cmdline.read_ueified(*self.args.runargs),)

        if self.args.attach:
            self.debug(str(binary_path), *args)
            return

        launch_kwargs = {}
        if not sys.stdout.isatty():
            launch_kwargs = { "silent" : True }
        exec_context = self.get_exec_context()
        runnable = exec_context.create_runnable(str(binary_path), *args);
        runnable.launch(**launch_kwargs)
        return True if runnable.is_gui() else runnable.wait()

#-------------------------------------------------------------------------------
class Program(_ProgramTarget):
    """ Launches the specified program """
    program = unrealcmd.Arg(str, "Name of the program to run")
    variant = _RunCmd._variant

    def complete_program(self, prefix):
        ue_context = self.get_unreal_context()

        for depth in ("*", "*/*"):
            for target_cs in ue_context.glob(f"Source/Programs/{depth}/*.Target.cs"):
                yield target_cs.name[:-10]

    def main(self):
        self.args.target = self.args.program
        return self._main_impl()

#-------------------------------------------------------------------------------
class Target(_ProgramTarget):
    """ Runs a name target on the host platform. """
    target  = unrealcmd.Arg(str, "Name of the target to run")
    variant = _RunCmd._variant

    def complete_target(self, prefix):
        #seen = set()
        #for name, _ in self.get_unreal_context().read_targets():
            #if name not in seen:
                #seen.add(name)
                #yield name
        pass

    def main(self):
        return self._main_impl()



#-------------------------------------------------------------------------------
class _Runtime(_RunCmd, _Attachable):
    """ When using --attach to debug to a process running on a devkit it is expected
    that the Visual Studio solution is already open (i.e. '.sln open' has been run
    prior to '.run ... --attach'). """
    platform = unrealcmd.Arg("", "Platform to run on")
    variant  = _RunCmd._variant
    trace    = unrealcmd.Opt((False, ""), "Add command line argument to trace to development host")
    cooked   = unrealcmd.Opt(False, "Use cooked data instead of staged data (local only)")
    onthefly = unrealcmd.Opt(False, "Add the -filehostip= argument.")
    binpath  = unrealcmd.Opt("", "Override the binary that is launched")
    datadir  = unrealcmd.Opt("", "Use and alternative staged/packaged directory")

    def _main_impl(self, target_type):
        ue_context = self.get_unreal_context()

        platform = self.args.platform
        if not platform:
            platform = unreal.Platform.get_host()

        platform = self.get_platform(platform)
        platform_name = platform.get_name()

        project = ue_context.get_project()
        if not project:
            raise EnvironmentError(f"An active project is required to launch a {target_type.name.lower()} instance")

        cook_form = platform.get_cook_form(target_type.name.lower())
        stage_dir = project.get_dir() / f"Saved/StagedBuilds/{cook_form}"

        # Allow the user to override the data directory used.
        if self.args.datadir:
            if self.args.cooked:
                self.print_warning("Ignoring --cooked because --datadir was given")
                self.args.cooked = False

            stage_dir = Path(self.args.datadir).absolute()
            if not stage_dir.is_dir():
                self.print_error(f"Directory {stage_dir} does not exist")
                return False

            if not (stage_dir / "Engine").is_dir():
                self.print_warning(f"An Engine/ directory is expected in '{stage_dir}'")


        # I probably shouldn't do this, but some platforms need this file to be
        # able to run with data served from the host PC. But I want to be explicit
        # and clear what command line arguments are given

        # Find the file in a manner that maintains case.
        cmdline_txt = None
        maybe_names = (
            "UECommandLine.txt",
            "uecommandline.txt",
            "UE4CommandLine.txt",
            "ue4commandline.txt",
        )
        for maybe_name in maybe_names:
            cmdline_txt = stage_dir / maybe_name
            if not cmdline_txt.is_file():
                continue
            cmdline_txt = cmdline_txt.resolve() # corrects case on Windows

            dest_txt = cmdline_txt.parent / (cmdline_txt.stem + "_old_ushell.txt")
            if not dest_txt.is_file():
                cmdline_txt.rename(dest_txt)

            break
        else:
            cmdline_txt = None

        if cmdline_txt is not None:
            cmdline_txt.open("wb").close()

        # Get the path to the binary to execute
        if self.args.binpath:
            binary_path = Path(self.args.binpath).resolve()
            if not binary_path.is_file():
                raise ValueError(f"Binary not found; '{binary_path}'")
        else:
            binary_path = self.get_binary_path(target_type, platform_name)

        # The user can opt-in to using cooked data instead of staged data
        if self.args.cooked:
            stage_dir = None
            if platform.get_name() != unreal.Platform.get_host():
                raise ValueError(f"The '--cooked' option may only be used when launching on {unreal.Platform.get_host()}")

        # Validate data directories
        if stage_dir:
            if not stage_dir.is_dir():
                suffix = "Client" if target_type == unreal.TargetType.CLIENT else ""
                stage_dir = project.get_dir() / f"Saved/StagedBuilds/{platform_name}{suffix}/"

            if not stage_dir.is_dir():
                raise EnvironmentError(f"No staged data found for '{cook_form}'")
        else:
            cook_dir = project.get_dir() / f"Saved/Cooked/{cook_form}/"
            if not cook_dir.is_dir():
                sandboxed = next((x for x in self.args.runargs if "-sandbox=" in x), None)
                if sandboxed:
                    self.print_warning(f"No cooked data found for '{cook_form}'")
                else:
                    raise EnvironmentError(f"No cooked data found for '{cook_form}'")

        # Condition command line with -filehostip
        has_filehostip = False
        for arg in self.args.runargs:
            if "-filehostip" in arg.lower():
                has_filehostip = True
                break
        else:
            if self.args.onthefly:
                self.args.runargs = (*self.args.runargs, "-filehostip=" + _get_ip())
                has_filehostip = True

        # The project argument.
        project_arg = project.get_name()
        if not has_filehostip:
            project_arg = f"../../../{project_arg}/{project_arg}.uproject"

        # Build arguments
        args = (
            project_arg,
            *unreal.cmdline.read_ueified(*self.args.runargs),
        )

        if not self.args.cooked:
            args = (*args, "-pak")

        if self.args.trace is not False:
            args = (*args, "-tracehost=" + _get_ip())
            if self.args.trace:
                args = (*args, "-trace=" + self.args.trace)

        # Launch the runtime
        stage_dir = str(stage_dir) + "/" if stage_dir else ""
        exec_context = self.get_exec_context()
        instance = platform.launch(exec_context, stage_dir, str(binary_path), args)
        if not instance:
            raise RuntimeError("An error occurred while launching.")

        # Attach a debugger
        if self.args.attach:
            if type(instance) is tuple:
                attach_ok = False
                pid, devkit_ip = instance

                # If there is no devkit involved we can try and use
                if devkit_ip == None:
                    attach_ok = self.attach_to(pid)

                # Otherwise we're using VS to attach and that's only on Windows
                elif vs_transport := getattr(platform, "vs_transport", None):
                    print("Attaching to", pid, "on", devkit_ip, "with", vs_transport)
                    attach_ok = self.attach_to(pid, transport=vs_transport, host_ip=devkit_ip)

                # Tell the user stuff
                if attach_ok:
                    print("Debugger attached successfully")
                else:
                    self.print_warning("Unable to attach a debugger")

                return

            self.print_warning(f"--attach is not currently supported on {platform.get_name()}")

#-------------------------------------------------------------------------------
class Game(_Runtime):
    """ Launches the game runtime. """
    def complete_platform(self, prefix):
        yield from super().complete_platform(prefix)
        yield "editor"

    @unrealcmd.Cmd.summarise
    def main(self):
        if self.args.platform == "editor":
            editor = Editor()
            editor._channel = self._channel
            args = (self.args.variant, "--", *self.args.runargs, "-game")
            if self.args.build:  args = ("--build", *args)
            if self.args.attach: args = ("--attach", *args)
            return editor.invoke(args)

        return self._main_impl(unreal.TargetType.GAME)

#-------------------------------------------------------------------------------
class Client(_Runtime):
    """ Launches the game client. """
    @unrealcmd.Cmd.summarise
    def main(self):
        return self._main_impl(unreal.TargetType.CLIENT)

#-------------------------------------------------------------------------------
class Server(_Runtime):
    """ Launches the server binary. """
    complete_platform = ("win64", "linux")
    @unrealcmd.Cmd.summarise
    def main(self):
        self.args.runargs = (*self.args.runargs, "-log", "-unattended")
        return self._main_impl(unreal.TargetType.SERVER)
