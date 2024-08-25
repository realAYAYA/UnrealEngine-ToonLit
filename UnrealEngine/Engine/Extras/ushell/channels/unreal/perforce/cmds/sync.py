# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import p4utils
import flow.cmd
import unrealcmd
from peafour import P4
from pathlib import Path


#-------------------------------------------------------------------------------
class _SyncBase(unrealcmd.Cmd):
    def _write_p4sync_txt_header(self, out):
        out.write("# lines starting with a '#' are comments\n")
        out.write("# lines prefixed with '-' are excluded from your sync\n")
        out.write("# -.../Android/...\n")
        out.write("# -/Engine/Source/...\n")
        out.write("# -*.uasset\n")



#-------------------------------------------------------------------------------
class Sync(_SyncBase):
    """ Syncs the current project and engine directories to the given changelist
    (or latest if none is provided). If the '--all' option is specified then the
    branch will be searched locally for existing .uproject files, scheduling each
    one to be synced.

    The sync can be filtered with a .p4sync.txt file. Lines prefixed with a '-'
    (e.g. "-.../SootySweep/...") will be excluded from the sync and anything
    already synced is de-synced. The command will read .p4sync.txt files from two
    locations;

        1. <branch_root>/.p4sync.txt
        2. ~/.ushell/.p4sync.txt (where ~ is USERPROFILE on Windows)

    Quick edit the branch's .p4sync.txt file with '.p4 sync edit'. An example
    of a .p4sync.txt file is as follows;

        # a comment
        -.../Android/...
        -/Engine/Source/...
        -*.uasset """
    changelist = unrealcmd.Arg(-1, "The changelist to sync to ('now' if unspecified)")
    noresolve  = unrealcmd.Opt(False, "Do not run 'p4 resolve -am' after syncing")
    dryrun     = unrealcmd.Opt(False, "Only pretend to do the sync")
    all        = unrealcmd.Opt(False, "Sync all the branch's projects found locally")
    addprojs   = unrealcmd.Opt("", "Comma-separated names of additional projects to sync")
    clobber    = unrealcmd.Opt(False, "Clobber writable files when syncing")
    nosummary  = unrealcmd.Opt(False, "Do not print the result-time summary at the end")
    echo       = unrealcmd.Opt(False, "Echo depot paths as they are synced")

    def complete_addprojs(self, prefix):
        # Fake a _local_root
        local_root = Path(os.getcwd())
        for parent in local_root.parents:
            if (parent / "GenerateProjectFiles.bat").is_file():
                local_root = parent
                break
        self._local_root = local_root

        # Now we can fetch an approximate context and list it's projects
        ue_context = self._try_get_unreal_context()
        if not ue_context:
            return

        branch = ue_context.get_branch()
        if not branch:
            return

        return (x.stem for x in  branch.read_projects())

    @unrealcmd.Cmd.summarise
    def _main_summarised(self):
        return self._main_impl()

    def main(self):
        self._client_spec_restore = None
        try:
            if self.args.nosummary:
                return self._main_impl()
            else:
                return self._main_summarised()
        finally:
            try:
                if self._client_spec_restore:
                    print("Restoring client spec")
                    client_spec = self._client_spec_restore
                    client_spec["Options"] = client_spec["Options"].replace("clobber", "noclobber")
                    client_spec["Description"] = client_spec["Description"].replace("{ushell_clobber_patch}", "")
                    P4.client(i=True).run(input_data=client_spec)
            except:
                pass

    def _setup(self):
        self.print_info("Perforce environment")

        # Check there's a valid Perforce environment.
        username = p4utils.login()
        if not p4utils.ensure_p4config():
            self.print_warning("Unable to establish a P4CONFIG")

        # Get some info about the Perforce environment and show it to the user
        info = P4.info().run()
        print("Client:", info.clientName)
        print("  User:", info.userName)
        print("Server:", getattr(info, "proxyAddress", info.serverAddress))

        # Inform the user if Perforce didn't find the client.
        if info.clientName == "*unknown*":
            client_name = p4utils.get_p4_set("P4CLIENT")
            _, p4config_name = p4utils.has_p4config(".")
            raise EnvironmentError(f"Client '{client_name}' not found. Please check P4CLIENT setting in '{p4config_name}'")

        # So that P4.where can succeed we sync one known file first. This also
        # ensures we can accomodate an unsynced stream switch.
        for x in P4.sync(f"//{info.clientName}/GenerateProjectFiles.bat").read(on_error=False):
            pass

        # Find the root of the current branch
        self.print_info("Discovering branch root")
        branch_root = p4utils.get_branch_root(f"//{info.clientName}/Engine/Source")
        print("Branch root:", branch_root)

        # Map branch root somewhere on the local file system
        local_root = P4.where(branch_root + "X").path
        local_root = local_root[:-1] # to strip 'X'
        print("Local root:", local_root)

        self._info = info
        self._branch_root = branch_root
        self._local_root = local_root

    def _try_get_unreal_context(self):
        try:
            ue_context = self.get_unreal_context()
            branch = ue_context.get_branch()
            # If branch doesn't match os.getcwd() then ditch it
            if not (branch and branch.get_dir().samefile(self._local_root)):
                raise EnvironmentError
        except EnvironmentError:
            try:
                cwd = os.getcwd()
                ue_context = unreal.Context(cwd)
            except EnvironmentError:
                ue_context = None

        return ue_context

    def _add_paths(self, syncer):
        # Add the set of paths that all syncs should include
        syncer.add_path(self._local_root + "*")
        syncer.add_path(self._local_root + "Engine/...")
        templates = self._local_root + "Templates/..."
        if P4.files(templates, m=1).run(on_error=lambda x: None) is not None:
            syncer.add_path(templates)

        # If we've a valid context by this point we can try and use it.
        glob_for_projects = False
        self._current_cl = 0
        if ue_context := self._try_get_unreal_context():
            project = ue_context.get_project()
            if self.args.all or not project:
                if branch := ue_context.get_branch():
                    project_count = 0
                    self.print_info("Syncing all known projects")
                    for uproj_path in branch.read_projects():
                        print(uproj_path.stem)
                        syncer.add_path(str(uproj_path.parent) + "/...")
                        project_count += 1

                    # If we have somehow managed to not find any projects then
                    # fallback to globbing for them.
                    if not project_count:
                        print("No projects found via .uprojectdirs")
                        print("Falling back to a glob")
                        glob_for_projects = True
            else:
                # By default the active project is synced
                self.print_info("Single project sync")
                print("Project:", project.get_name())
                syncer.add_path(str(project.get_dir()) + "/...")

            # Extra projects
            if self.args.addprojs and not self.args.all:
                add_projects = self.args.addprojs.replace("/", ",")
                add_projects = (x.strip() for x in add_projects.split(","))
                add_projects = {x for x in add_projects if x}
                known_projects = list(ue_context.get_branch().read_projects())
                known_projects = {x.stem.lower():x for x in known_projects}
                self.print_info("Additional projects to sync;")
                for add_project in add_projects:
                    print(add_project, ": ", sep="", end="")
                    add_project = add_project.lower()
                    if add_project not in known_projects:
                        print("not found")
                        continue

                    add_project = known_projects[add_project]
                    add_project = add_project.parent
                    syncer.add_path(str(add_project) + "/...")
                    print(add_project)

            engine_info = ue_context.get_engine().get_info()
            self._current_cl = engine_info.get("Changelist", 0)
        else:
            glob_for_projects = True

        if glob_for_projects:
            # There does not appear to be a fully formed branch so we will infer
            # `--all` here on behalf of the user.
            self.print_info("Syncing all projects by **/.uproject")
            for uproj_path in Path(self._local_root).glob("**/*.uproject"):
                print(uproj_path.stem)
                syncer.add_path(str(uproj_path.parent) + "/...")

    def _main_impl(self):
        self._setup()

        # Determine the changelist to sync
        sync_cl = self.args.changelist
        if sync_cl < 0:
            sync_cl = int(P4.changes(self._branch_root + "...", m=1).change)

        # Remove "noclobber" from the user's client spec
        client = P4.client(o=True).run()
        client_spec = client.as_dict()
        client_spec.setdefault("Description", "")

        if self.args.clobber:
            self.print_info("Checking for 'noclobber'")
            if "noclobber" in client_spec["Options"]:
                client_spec["Options"] = client_spec["Options"].replace("noclobber", "clobber")
                client_spec["Description"] += "{ushell_clobber_patch}"
                self._client_spec_restore = client_spec.copy()
                if not self.args.dryrun or True:
                    print(f"Patching {client.Client} with 'clobber'")
                    P4.client(i=True).run(input_data=client_spec)
            else:
                print("Clobbering is already active")

        if not self._client_spec_restore:
            if "{ushell_clobber_patch}" in client_spec["Description"]:
                if "noclobber" not in client_spec["Options"]:
                    self._client_spec_restore = client_spec.copy()

        # Add the paths we always want to sync
        syncer = p4utils.Syncer()
        self._add_paths(syncer)

        # Load and parse the .p4sync.txt file
        self._apply_p4sync_txt(syncer)

        version_cl = 0
        build_ver_path = self._local_root + "Engine/Build/Build.version"
        try:
            # Special case to force sync Build.version. It can get easily modified
            # without Perforce's knowledge, complicating the sync.
            if not self.args.dryrun:
                P4.sync(build_ver_path + "@" + str(sync_cl), qf=True).run(on_error=False)

            # GO!
            self.print_info("Scheduling sync")
            print("Changelist:", sync_cl, f"(was {self._current_cl})")
            print("Requesting... ", end="")
            syncer.schedule(sync_cl)

            self.print_info("Syncing")
            ok = syncer.sync(dryrun=self.args.dryrun, echo=self.args.echo)

            if self.args.dryrun or not ok:
                return ok

            # Sync succeeded, update cl for build.version even if something goes wrong with resolving
            version_cl = sync_cl

            # Auto-resolve on behalf of the user.
            if not self.args.noresolve:
                conflicts = set()
                self.print_info("Resolving")
                for item in P4.resolve(am=True).read(on_error=False):
                    path = getattr(item, "fromFile", None)
                    if not path:
                        continue

                    path = path[len(self._branch_root):]
                    if getattr(item, "how", None):
                        conflicts.remove(path)
                        print(path)
                    else:
                        conflicts.add(path)

                for conflict in conflicts:
                    print(flow.cmd.text.light_red(conflict))

        except KeyboardInterrupt:
            print()
            if not self.args.dryrun:
                self.print_warning(f"Sync interrupted! Writing build.version to CL {version_cl}")
            return False
        finally:
            if not self.args.dryrun:
                # Record the synced changelist in Build.version
                with open(build_ver_path, "r") as x:
                    lines = list(x.readlines())

                import stat
                build_ver_mode = os.stat(build_ver_path).st_mode
                os.chmod(build_ver_path, build_ver_mode|stat.S_IWRITE)

                with open(build_ver_path, "w") as x:
                    for line in lines:
                        if r'"Changelist"' in line:
                            line = line.split(":", 2)
                            line = line[0] + f": {version_cl},\n"
                        elif r'"BranchName"' in line:
                            line = "\t\"BranchName\": \"X\"\n"
                            line = line.replace("X", self._branch_root[:-1].replace("/", "+"))
                        x.write(line)

    def _apply_p4sync_txt(self, syncer):
        def impl(path):
            print("Source:", os.path.normpath(path), end="")
            try:
                sync_config = open(path, "rt")
                print()
            except:
                print(" ... not found")
                return

            def read_exclusions():
                for line in map(str.strip, sync_config):
                    if line.startswith("-"):    yield line[1:]
                    elif line.startswith("$-"): yield line[2:]

            for i, line in enumerate(read_exclusions()):
                view = None
                if line.startswith("*."):    view = ".../" + line
                elif line.startswith("/"):   view = line[1:]
                elif line.startswith("..."): view = line

                print("       %2d" % i, "exclude", end=" ")

                if view and (view.count("/") or "/*." in view or view.startswith("*.")):
                    view = self._branch_root + view
                    syncer.add_exclude(view)
                    print(view)
                else:
                    view = view or line
                    print(flow.cmd.text.light_yellow(view + " (ill-formed)"))

            sync_config.close()

        self.print_info("Applying .p4sync.txt")
        for dir in (self.get_home_dir(), self._local_root):
            impl(dir + ".p4sync.txt")



#-------------------------------------------------------------------------------
class Edit(_SyncBase):
    """ Opens .p4sync.txt in an editor. The editor is selected from environment
    variables P4EDITOR, GIT_EDITOR, and the system default editor. """
    def main(self):
        username = p4utils.login()

        cwd = os.getcwd()
        client = p4utils.get_client_from_dir(cwd, username)
        if not client:
            raise EnvironmentError(f"Unable to establish the clientspec from '{cwd}'")

        _, root_dir = client
        path = Path(root_dir) / ".p4sync.txt"

        if not path.is_file():
            with path.open("wt") as out:
                self._write_p4sync_txt_header(out)

        print("Editing", path)
        self.edit_file(path)
