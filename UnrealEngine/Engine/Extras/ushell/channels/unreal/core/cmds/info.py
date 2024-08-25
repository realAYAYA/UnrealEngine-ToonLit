# Copyright Epic Games, Inc. All Rights Reserved.

import unreal
import flow.cmd
import unrealcmd

#-------------------------------------------------------------------------------
def _write(key, value):
    value = str(value)

    if   value.startswith("?"): value = flow.cmd.text.light_yellow(value[1:])
    elif value.startswith("!"): value = flow.cmd.text.light_red(value[1:])

    dots = max(23 - len(str(key)), 3)
    dots = flow.cmd.text.grey(("." * dots) if dots > 0 else "")
    print(key, dots, value)



#-------------------------------------------------------------------------------
class InfoBase(unrealcmd.Cmd):
    json = unrealcmd.Opt(False, "Print the output as JSON")

    def _print_pretty(self, info):
        def print_dict(name, subject):
            if isinstance(subject, dict):
                it = subject.items()
            elif isinstance(subject, list):
                it = enumerate(subject)

            for key, value in it:
                if isinstance(value, dict|list):
                    print_dict(key, value)
                    continue

                if name and name != "env":
                    print(end=self._end)
                    self._end = "\n"
                    self.print_info(name.title().replace("_", " "))
                    name = None

                _write(key, value)

        print_dict("", info)

    def _print_json(self, info):
        import sys
        import json
        def represent(subject):
            if hasattr(subject, "get_inner"):
                return subject.get_inner()
            return repr(subject)
        json.dump(info, sys.stdout, indent=2, default=represent)

    def main(self):
        self._end = ""
        info = self._get_info()
        if self.args.json:
            self._print_json(info)
        else:
            self._print_pretty(info)



#-------------------------------------------------------------------------------
class Info(InfoBase):
    """ Displays info about the current session. """

    def _get_engine_info(self):
        context = self.get_unreal_context()
        engine = context.get_engine()

        info = {
            "path"          : str(engine.get_dir()),
            "version"       : "",
            "version_full"  : "",
            "branch"        : "",
            "changelist"    : "0",
        }

        version = engine.get_info()
        if version:
            info["branch"] = version["BranchName"]
            info["changelist"] = str(version["Changelist"])
            info["version"] = str(engine.get_version_major())
            info["version_full"] = "{}.{}.{}".format(
                version["MajorVersion"],
                version["MinorVersion"],
                version["PatchVersion"])

        return info

    def _get_project_info(self):
        context = self.get_unreal_context()
        project = context.get_project()

        info = {
            "name" : "?no active project"
        }

        if not project:
            return info

        info["name"] = project.get_name()
        info["path"] = str(project.get_path())
        info["dir"] = str(project.get_dir())

        targets_info = {}
        known_targets = (
            ("editor", unreal.TargetType.EDITOR),
            ("game",   unreal.TargetType.GAME),
            ("client", unreal.TargetType.CLIENT),
            ("server", unreal.TargetType.SERVER),
        )
        for target_name, target_type in known_targets:
            target = context.get_target_by_type(target_type)
            output = target.get_name() if target else "!unknown"
            targets_info[target_name] = output
        info["targets"] = targets_info

        return info

    def _get_platforms_info(self):
        info = {}

        for platform_name in sorted(self.read_platforms()):
            inner = info.setdefault(platform_name.lower(), {})
            platform = self.get_platform(platform_name)

            inner["name"] = platform.get_name()
            inner["version"] = platform.get_version() or "!unknown"

            inner["env"] = {}
            for item in platform.read_env():
                try: value = item.validate()
                except EnvironmentError: value = "!" + item.get()
                inner["env"][item.key] = value

        return info

    def _get_info(self):
        return {
            "engine"    : self._get_engine_info(),
            "project"   : self._get_project_info(),
            "platforms" : self._get_platforms_info(),
        }



#-------------------------------------------------------------------------------
class Projects(InfoBase):
    """ Displays the branch's synced .uprojects """

    def _get_info(self):
        context = self.get_unreal_context()
        branch = context.get_branch(must_exist=True)
        return { "branch_projects" : list(str(x) for x in branch.read_projects()) }



#-------------------------------------------------------------------------------
class Config(InfoBase):
    """ Evaluates currect context's config for a particular category. The 'override'
    option can be used to enumerate platform-specific config sets."""
    category = unrealcmd.Arg(str, "The ini category to load")
    override = unrealcmd.Opt("", "Alternative config set to enumerate (e.g. 'WinGDK')")

    def complete_category(self, prefix):
        return ("Engine", "Game", "Input", "DeviceProfiles", "GameUserSettings",
            "Scalability", "RuntimeOptions", "InstallBundle", "Hardware", "GameplayTags")

    def _get_info(self):
        context = self.get_unreal_context()
        config = context.get_config()
        ini = config.get(self.args.category, override=self.args.override)

        ret = {}
        for s_name, section in ini:
            ret[s_name] = {k:v for k,v in section}

        return ret
