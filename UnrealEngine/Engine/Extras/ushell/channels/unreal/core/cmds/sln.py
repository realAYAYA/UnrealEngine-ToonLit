# Copyright Epic Games, Inc. All Rights Reserved.

import os
import shutil
import unrealcmd

#-------------------------------------------------------------------------------
class _Base(unrealcmd.MultiPlatformCmd):
    def _get_primary_name(self):
        if hasattr(self, "_primary_name"):
            return self._primary_name

        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        name = "UE" + str(engine.get_version_major())
        try:
            ppn_path = engine.get_dir() / "Intermediate/ProjectFiles/PrimaryProjectName.txt"
            if not ppn_path.is_file():
                # Old path, needs to be maintained while projects prior to UE5.1 are supported
                ppn_path = engine.get_dir() / ("Intermediate/ProjectFiles/M" + "asterProjectName.txt")
            with ppn_path.open("rb") as ppn_file:
                name = ppn_file.read().decode().strip()
        except FileNotFoundError:
            self.print_warning("Project files may be ungenerated")

        self._primary_name = name
        return name

    def _get_sln_path(self):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()
        primary_name = self._get_primary_name()
        return engine.get_dir().parent / (primary_name + ".sln")

    def _open_sln(self):
        if os.name != "nt":
            self.print_error("Opening project files is only supported on Windows")
            return False

        self.print_info("Opening project")
        sln_path = self._get_sln_path()
        print("Path:", sln_path)
        if not os.path.isfile(sln_path):
            self.print_error("Project file not found")
            return False

        print("Enumerating VS instances")
        import vs.dte
        for i, instance in enumerate(vs.dte.running()):
            print(f" {i} ", end="")
            if not (instance_path := instance.get_sln_path()):
                print("no-sln")
                continue
            print(instance_path, end="")

            if sln_path.samefile(instance_path):
                if instance.activate():
                    print(" ...activating")
                    return True
            print()

        os.chdir(os.path.dirname(sln_path))

        run_args = ("cmd.exe", "/c", "start", sln_path)
        shell_open = self.get_exec_context().create_runnable(*run_args)
        return shell_open.run()

#-------------------------------------------------------------------------------
class Generate(_Base):
    """ Generates a Visual Studio solution for the active project. To more
    easily distinguish open solutions from one branch to the next, the generated
    .sln file will be suffixed with the current branch name. Use the '--all'
    option to include all the current branch's projects. To generate-then-open
    invoke as '.sln generate open'."""
    open    = unrealcmd.Arg("", "If this argument is 'open' then open after generating")
    ubtargs = unrealcmd.Arg([str], "Additional arguments pass to UnrealBuildTool")
    notag   = unrealcmd.Opt(False, "Do not tag the solution name with a branch identifier")
    all     = unrealcmd.Opt(False, "Include all the branch's projects")

    def complete_open(self, prefix):
        yield "open"

    def get_exec_context(self):
        context = super().get_exec_context()
        if not self.args.notag:
            context.get_env()["UE_NAME_PROJECT_AFTER_FOLDER"] = "1"
        return context

    def main(self):
        self.use_all_platforms()
        ue_context = self.get_unreal_context()

        exec_context = self.get_exec_context()
        args = ("-ProjectFiles", *self.args.ubtargs)
        if not self.args.all:
            if project := ue_context.get_project():
                args = ("-Project=" + str(project.get_path()), *args)

        self.print_info("Generating project files")
        ubt = ue_context.get_engine().get_ubt()
        for cmd, args in ubt.read_actions(*args):
            cmd = exec_context.create_runnable(cmd, *args)
            ret = cmd.run()
            if ret:
                return ret

        if self.args.open == "open":
            return self._open_sln()

#-------------------------------------------------------------------------------
class Open(_Base):
    """ Opens project files in Visual Studio """

    def get_exec_context(self):
        context = super().get_exec_context()

        # Detect when UE_NAME_PROJECT_AFTER_FOLDER was used to generate the
        # project files and set it to allow tools like UnrealVS to generate
        # the project files consistently.
        primary_parts = self._get_primary_name().split("_", 1)
        if len(primary_parts) > 1:
            ue_context = self.get_unreal_context()
            engine = ue_context.get_engine()
            folder_name = engine.get_dir().parent.parent.name
            if folder_name == primary_parts[1]:
                context.get_env()["UE_NAME_PROJECT_AFTER_FOLDER"] = "1"

        return context

    def main(self):
        self.use_all_platforms()
        return self._open_sln()
