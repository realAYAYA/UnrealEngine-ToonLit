# Copyright Epic Games, Inc. All Rights Reserved.

import unreal
import unrealcmd
import subprocess

#-------------------------------------------------------------------------------
class _Impl(unrealcmd.Cmd):
    target   = unrealcmd.Arg(str, "Cooked target to stage")
    platform = unrealcmd.Arg("", "Platform whose cook should be staged")
    variant  = unrealcmd.Arg("development", "Build variant to stage binaries to copy")
    uatargs  = unrealcmd.Arg([str], "Additional arguments to pass to UAT")

    complete_target = ("game", "client", "server")

    def _build(self, build_editor, ubt_args=None):
        self.print_info("Building")

        monolithic = (
            self.args.target,
            self.args.platform,
            self.args.variant,
        )
        if ubt_args:
            import shlex
            monolithic = (
                *monolithic,
                "--",
                *shlex.split(ubt_args),
            )

        schedule = []
        if build_editor:
            schedule.append(("editor", ))
        schedule.append(monolithic)

        for params in schedule:
            ret = subprocess.run(("_build", *params))
            if ret.returncode:
                raise SystemExit(ret.returncode)

    def _cook(self, cook_args=None):
        self.print_info("Cooking")

        if cook_args:
            import shlex
            cook_args = ("--", *shlex.split(cook_args))

        ret = subprocess.run((
            "_cook",
            self.args.target,
            self.args.platform,
            *(cook_args if cook_args else tuple()),
        ))

        if ret.returncode:
            raise SystemExit(ret.returncode)

    @unrealcmd.Cmd.summarise
    def main(self, *, skipstage=False):
        # Check there is a valid active project
        ue_context = self.get_unreal_context()
        project = ue_context.get_project()
        if not project:
            raise EnvironmentError("An active project is required to stage a build")

        # Fix up the platform argument so it has a value
        platform = self.args.platform
        if not platform:
            platform = unreal.Platform.get_host()
        platform = self.args.platform = platform.lower()

        # Validate the given target
        target = unreal.TargetType.parse(self.args.target)
        target = ue_context.get_target_by_type(target)
        if not target:
            self.print_error(f"'{project.get_name()}' does not appear to have a '{self.args.target}' target")
            return 1

        # Inform the user about the context stage is going to operate in.
        self.print_info("Context")
        print("Target:", self.args.target)
        print("Platform:", self.args.platform)
        print("Variant:", self.args.variant)

        # Build and cook convenience
        if self.args.build is not False:
            ubt_args = self.args.build if self.args.build else None
            self._build(self.args.cook is not False, ubt_args=ubt_args)
            print()

        if self.args.cook is not False:
            cook_args = self.args.cook if self.args.cook else None
            self._cook(cook_args)
            print()

        # Launch UAT
        self.print_info("Staging")

        if target.get_type() == unreal.TargetType.GAME:
            target_arg = None
        elif target.get_type() == unreal.TargetType.CLIENT:
            target_arg = "-client"
        elif target.get_type() == unreal.TargetType.SERVER:
            target_arg = "-server"
            self.args.uatargs = (*self.args.uatargs, "-noclient")
        else:
            raise ValueError(f"Unable to stage target of type '{self.args.target}'")

        platform = self.get_platform(platform)

        cook_flavor = platform.get_cook_flavor()

        dot_uat_cmd = "_uat"
        dot_uat_args = (
            "BuildCookRun",
            "--", # NB. NOT a UAT arg!
            #"-project=" + ue_context.get_project().get_path() <-- implicit via _uat
            target_arg,
            "-skipbuild" if "-build" not in self.args.uatargs else None,
            "-skipcook" if "-cook" not in self.args.uatargs else None,
            "-skipstage" if skipstage else "-stage",
            "-deploy" if self.args.deploy else None,
            "-config=" + self.args.variant,
            "-platform=" + platform.get_name(),
            ("-cookflavor=" + cook_flavor) if cook_flavor else None,
            None if self.args.nopak else "-pak",
            *self.args.uatargs,
        )

        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable(dot_uat_cmd, *dot_uat_args)
        return cmd.run()



#-------------------------------------------------------------------------------
class Stage(_Impl):
    """ Stages a build from the latest cook and built binaries. Note that
    specifying the variant is only relevant if this stage is to be deployed at
    some point in the future (it is not the binary used when using '.run').

    The --build/cook options accept an optional argument that are the arguments
    passed on to UnrealBuildTool and the cook commandlet respectively. For
    example; '--build="-GoSuperFast -DisableUnity"'. Double-up ("") or escape
    (\\") to pass through quotes."""
    build  = unrealcmd.Opt((False, ""), "Build code prior to staging")
    cook   = unrealcmd.Opt((False, ""), "Run a cook before running the stage step")
    deploy = unrealcmd.Opt(False, "Deploy to devkit/device after staging")
    nopak  = unrealcmd.Opt(False, "Staged result should be loose files")



#-------------------------------------------------------------------------------
class Deploy(_Impl):
    """ Deploys a staged build to a devkit for the given platform. If there is no
    existing staging to deploy the command '.stage ... --deploy' can be used. """
    def main(self):
        self.args.build  = False
        self.args.cook   = False
        self.args.deploy = True
        self.args.nopak  = True
        return super().main(skipstage=True)
