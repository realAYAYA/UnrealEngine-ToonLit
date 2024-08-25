# Copyright Epic Games, Inc. All Rights Reserved.

import re
import unreal
import flow.cmd
import unrealcmd
import uelogprinter
import unreal.cmdline

#-------------------------------------------------------------------------------
class _CookPrettyPrinter(uelogprinter.Printer):
    def __init__(self):
        super().__init__()
        self._cook_re = re.compile(r"ages (\d+).+otal (\d+)")

    def _print(self, line):
        m = self._cook_re.search(line)
        if m:
            percent = (int(m.group(1)) * 100) // max(int(m.group(2)), 1)
            line += f" [{percent}%]"
        super()._print(line)



#-------------------------------------------------------------------------------
class _Cook(unrealcmd.MultiPlatformCmd):
    cookargs = unrealcmd.Arg([str], "Additional arguments passed to the cook commandlet")
    cultures = unrealcmd.Opt("en", "Cultures to cook (comma separated, defaults to 'en')")
    onthefly = unrealcmd.Opt(False, "Launch as an on-the-fly server")
    iterate  = unrealcmd.Opt(False, "Cook iteratively on top of the previous cook")
    noxge    = unrealcmd.Opt(False, "Disable XGE-based shader compilation")
    unpretty = unrealcmd.Opt(False, "Turns off colourful pretty-printing")
    attach   = unrealcmd.Opt(False, "Attach a debugger to the cook")
    debug    = unrealcmd.Opt(False, "Use debug executables")

    @unrealcmd.Cmd.summarise
    def _cook(self, cook_form, exec_context=None):
        ue_context = self.get_unreal_context()

        # Check there is a valid project
        project = ue_context.get_project()
        if not project:
            self.print_error("An active project is required to cook")
            return False

        # Prepare arguments
        cultures = self.args.cultures.replace(",", "+")

        commandlet_args = (
            ("-targetplatform=" + cook_form) if cook_form else None,
            ("-cookcultures=" + cultures) if cultures else None,
            "-unattended",
            "-unversioned",
            "-stdout",
            "-cookonthefly" if self.args.onthefly else None,
            "-iterate" if self.args.iterate else None,
            "-noxgeshadercompile" if self.args.noxge else None,
            *unreal.cmdline.read_ueified(*self.args.cookargs),
        )

        commandlet_args = tuple(x for x in commandlet_args if x)

        # To support attaching a debugger we will reuse '.run commandlet'. This
        # approach is not used normally as it interferes with pretty-printing and
        # creates an awful lot of child processes.

        if self.args.attach:
            args = ("_run", "commandlet", "cook", "--noplatforms", "--attach")
            if self.args.debug:
                args = (*args, "debug")
            args = (*args, "--", *commandlet_args)

            # Disable our handling of Ctrl-C
            import signal
            def nop_handler(*args): pass
            signal.signal(signal.SIGINT, nop_handler)

            import subprocess
            return subprocess.run(args).returncode

        # Find the editor binary
        target = ue_context.get_target_by_type(unreal.TargetType.EDITOR)
        variant = unreal.Variant.DEBUG if self.args.debug else unreal.Variant.DEVELOPMENT
        build = target.get_build(variant=variant)
        if not build:
            self.print_error(f"No {variant.name.lower()} editor build found")
            return False

        cmd = str(build.get_binary_path())
        if not cmd.endswith("-Cmd.exe"):
            cmd = cmd.replace(".exe", "-Cmd.exe")

        # Launch
        args = (
            project.get_path(),
            "-run=cook",
            *commandlet_args,
        )

        exec_context = exec_context or self.get_exec_context()
        cmd = exec_context.create_runnable(cmd, *args)
        if self.args.unpretty or not self.is_interactive():
            cmd.run()
        else:
            printer = _CookPrettyPrinter()
            printer.run(cmd)
        return cmd.get_return_code()



#-------------------------------------------------------------------------------
class Cook(_Cook):
    """ Prepares a cooked meal of dataset for the given target platform """
    target = unrealcmd.Arg(str, "Full target platform name (i.e. WindowsNoEditor)")
    def main(self):
        return self._cook(self.args.target)



#-------------------------------------------------------------------------------
class _Runtime(_Cook):
    platform = unrealcmd.Arg("", "The platform to cook data for")

    def main(self, target):
        platform = self.args.platform
        if not platform:
            platform = unreal.Platform.get_host()
        platform = self.get_platform(platform)

        exec_context = super().get_exec_context()
        exec_env = exec_context.get_env()

        # Establish target platform's environment
        name = platform.get_name()
        self.print_info(name, "-", platform.get_version())
        for item in platform.read_env():
            try:
                dir = str(item)
                exec_env[item.key] = dir
            except EnvironmentError:
                dir = flow.cmd.text.red(item.get())
            print(item.key, "=", dir)

        cook_form = platform.get_cook_form(target.name.lower())
        return self._cook(cook_form, exec_context)

#-------------------------------------------------------------------------------
class Game(_Runtime):
    """ Cooks game data for a particular platform """
    def main(self):
        return super().main(unreal.TargetType.GAME)

#-------------------------------------------------------------------------------
class Client(_Runtime):
    """ Cooks client data for a particular platform """
    def main(self):
        return super().main(unreal.TargetType.CLIENT)

#-------------------------------------------------------------------------------
class Server(_Runtime):
    """ Cooks server data for a particular platform """
    complete_platform = ("win64", "linux")

    def main(self):
        return super().main(unreal.TargetType.SERVER)
