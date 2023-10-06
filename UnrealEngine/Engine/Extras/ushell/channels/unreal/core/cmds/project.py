# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import flow.cmd

#-------------------------------------------------------------------------------
class Change(flow.cmd.Cmd):
    """ Changes this session's active project. The 'nameorpath' argument can be
    one of following;

     .project MyProjectName       - use branch's .uprojectdirs and find by name
     .project d:/foo/bar.uproject - set by .uproject path
     .project branch              - unset the active project on the branch
     .project cwd                 - active project or branch follows CWD
     .project active              - output current active project
     .project                     - pick from a list of all projects under the CWD
    """
    nameorpath   = flow.cmd.Arg("", "Name or path of the project to make active")

    def _complete_path(self, prefix):
        prefix = prefix or "."
        for item in os.scandir(prefix):
            if item.name.endswith(".uproject"):
                yield item.name
                return

        for item in os.scandir(prefix):
            if item.is_dir():
                yield item.name + "/"

    def complete_nameorpath(self, prefix):
        if "/" in prefix or "\\" in prefix:
            yield from self._complete_path(prefix)
            return

        yield "branch"
        yield "cwd"
        yield "active"

        session = self.get_noticeboard(self.Noticeboard.SESSION)
        context = unreal.Context(session["uproject"] or ".")
        branch = context.get_branch()
        for uproj_path in branch.read_projects():
                yield uproj_path.stem

    def _select_project(self):
        if not self.is_interactive():
            return

        import fzf
        import pathlib
        import subprocess as sp

        # Select the folder to use. If there's a .uprojectdirs up the tree we'll
        # start there otherwise we'll use the cwd
        try:
            context = unreal.Context(".")
            branch = context.get_branch()
            cwd = str(branch.get_dir())
            def read_projects():
                for uproj_path in branch.read_projects():
                    yield f"{uproj_path.stem:19}\t{uproj_path}"
        except:
            cwd = os.getcwd()
            def read_projects():
                rg_args = (
                    "rg",
                    "--files",
                    "--path-separator=/",
                    "--no-ignore",
                    "--ignore-file=" + os.path.abspath(__file__ + "/../../uproject.rgignore"),
                    "--ignore-file=" + os.path.abspath(__file__ + "/../../dirs.rgignore"),
                )

                rg = sp.Popen(rg_args, stdout=sp.PIPE, stderr=sp.DEVNULL)

                for line in rg.stdout.readlines():
                    line = line.decode().rstrip().split("/")
                    if len(line) > 1:
                        name = line[-2].replace(" ", "")
                        yield f"{name:19}\t{'/'.join(line)}"
                rg.wait()

        print("Select a project (fuzzy, move with arrows, enter to select)...", end="")
        prompt = os.path.normpath(cwd).replace("\\", "/") + "/"
        for reply in fzf.run(read_projects(), prompt=prompt):
            reply = reply.split("\t", 1)
            reply = reply[1].lstrip()
            return os.path.abspath(reply)

    def _print_active(self):
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        context = unreal.Context(session["uproject"] or ".")
        branch = context.get_branch()
        if not branch:
            raise EnvironmentError(f"Unable to find a branch with a .uprojectdirs from '{context.get_primary_path()}'")

        if project := context.get_project():
            print(project.get_dir())
        else:
            self.print_info("[no active project]")
        return

    def _set_cwd(self):
        self.print_info("Clearing active branch/project")
        print("Context will now follow current directory")
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        session["uproject"] = "."

    def _set_branch(self):
        self.print_info("Clearing active project")
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        context = unreal.Context(session["uproject"] or ".")
        branch = context.get_branch()
        if not branch:
            context = unreal.Context(".")
            branch = context.get_branch()
            if not branch:
                raise EnvironmentError(f"Unable to clear the active project without a branch")
        candidate = branch.get_dir()
        print("Using branch;", candidate)
        return candidate

    def _set_auto(self):
        try:
            context = unreal.Context(".")
        except EnvironmentError:
            return

        if project := context.get_project():
            return str(project.get_dir())

        if (branch := context.get_branch()) is None:
            return

        best_mtime = 0
        best = None
        for uproj_path in branch.read_projects():
            mtime = uproj_path.stat().st_mtime
            if mtime < best_mtime:
                continue
            best_mtime = mtime
            best = uproj_path

        if best:
            return str(best)

        return str(branch.get_dir())


    def _set_qualified(self, candidate):
        candidate = os.path.abspath(candidate)
        self.print_info("Using qualified directory or file path")
        print("Path:", candidate)

        try:
            context = unreal.Context(candidate)
        except EnvironmentError as e:
            # If the user gave a directory we can fallback to a fuzzy search
            if os.path.isdir(candidate):
                os.chdir(candidate)
                self.args.nameorpath = ""
                return self._main_impl()

            self.print_error(e)
            return

        if project := context.get_project():
            candidate = project.get_path()
            print("Project found;", candidate)
        else:
            candidate = context.get_branch().get_dir()
            print("Branch found;", candidate)

        return candidate

    def _set_short_form(self, candidate):
        self.print_info("Short-form search")

        # Get an Unreal context, preferring the CWD and falling back to the
        # current active context if there is one.
        try:
            context = unreal.Context(".")
        except EnvironmentError:
            session = self.get_noticeboard(self.Noticeboard.SESSION)
            context = unreal.Context(session["uproject"] or ".")

        # To be able to search projects by short-name we need a branch
        branch = context.get_branch()
        if not branch:
            raise EnvironmentError(f"Unable to find a branch with a .uprojectdirs from '{context.get_primary_path()}'")

        # Find the file that lists search paths
        candidate = candidate.lower()
        for i, uproj_path in enumerate(branch.read_projects()):
            print("%-2d" % i, os.path.normpath(uproj_path.parent))
            if uproj_path.stem.lower() == candidate:
                print("Project found")
                candidate = uproj_path
                break
        else:
            self.print_error(f"Unable to find a project called '{candidate}' from", branch.get_dir())
            return

        return candidate

    def main(self):
        ret = self._main_impl()
        self._provision(ret)
        return ret

    def _main_impl(self):
        if not self.args.nameorpath:
            if selected := self._select_project():
                self.args.nameorpath = selected

        candidate = self.args.nameorpath

        if not candidate:
            self.print_error("No project name or path specified")
            return False

        if candidate.lower() == "active":
            return self._print_active()

        if candidate.lower() == "auto":
            candidate = self._set_auto()
            if not candidate:
                self.print_error(f"Unable to locate any projects from '{os.getcwd()}'")
                return False

        # Work out what the candidate should be
        if candidate.lower() == "cwd":
            return self._set_cwd()
        elif candidate.lower() == "none" or candidate.lower() == "branch":
            candidate = self._set_branch()
        elif "/" in candidate or "\\" in candidate or os.path.exists(candidate):
            candidate = self._set_qualified(candidate)
        else:
            candidate = self._set_short_form(candidate)

        if not candidate:
            return False

        # Fire up the context
        context = unreal.Context(candidate)
        if project := context.get_project():
            self.print_info("Changing active project")
            print("Name:", project.get_name())
            print("Path:", project.get_path())
            primary_dir = str(project.get_path())
        else:
            primary_dir = str(context.get_primary_path())

        # Record the project as active
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        session["uproject"] = primary_dir

        return primary_dir

    def _provision(self, primary_dir):
        if not primary_dir:
            return

        from pathlib import Path

        # Find branch root
        for candidate in Path(__file__).parents:
            if (candidate / "ushell.bat").is_file():
                root_dir = candidate
                break
        else:
            return

        # Find the bundle of data to provision
        input_path = root_dir / Path("channels/bundle")
        if not input_path.is_file():
            return

        with input_path.open("rb") as inp:
            input_data = memoryview(inp.read())

        input_mtime = input_path.stat().st_mtime

        # Check we've something to provision from
        nyckels = self._get_nyckels(primary_dir)
        if not nyckels:
            return

        self.print_info("Provisioning")

        # Let's provision!
        import cipher
        blob = cipher.Blob(input_data)
        for nyckel in nyckels:
            for name, data in blob.find(nyckel):
                path = root_dir / name
                if not path.is_file():
                    print(name)
                    dest_path = (root_dir / path)
                    dest_path.parent.mkdir(parents=True, exist_ok=True)
                    with dest_path.open("wb") as out:
                        out.write(data)

    def _get_nyckels(self, primary_dir):
        pass
        """
        context = unreal.Context(primary_dir)
        ini_paths = [x for x in context.glob("Extras/ushell/Settings.ini")]
        if not ini_paths:
            return

        # And we need to know what we can provision
        import base64
        nyckels = []
        for ini_path in ini_paths:
            ini = unreal.Ini()
            with ini_path.open("rt") as inp:
                ini.load(inp)
            if nyckel := ini.ushell.blob:
                nyckel = base64.b64decode(str(nyckel))
                nyckels.append(nyckel)
        """
