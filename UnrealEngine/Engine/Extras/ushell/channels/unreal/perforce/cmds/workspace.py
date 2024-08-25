# Copyright Epic Games, Inc. All Rights Reserved.

import os
import fzf
import socket
import p4utils
import flow.cmd
from peafour import P4
from pathlib import Path, PurePosixPath

#-------------------------------------------------------------------------------
def _build_depot_path():
    ret = "//"
    for item in fzf.run((x.name for x in P4.depots()), prompt=ret):
        ret += item
        break
    else:
        return ""

    ret = PurePosixPath(ret)

    ok_but = "[ok]"
    while True:
        def read_dirs(p):
            yield ok_but
            yield ".."
            yield from (x.dir for x in P4.dirs(str(p) + "/*"))
        for item in fzf.run(read_dirs(ret), prompt=str(ret) + "/"):
            if item == ok_but:
                return ret
            if item == "..":
                ret = ret.parent
                continue
            ret /= item
            break
        else:
            return ret

#-------------------------------------------------------------------------------
def _check_dir_is_empty(dir):
    for item in dir.glob("*"):
        raise ValueError(f"Directory '{dir}' is not empty")

#-------------------------------------------------------------------------------
def _populate_ue_branch(*args):
    _populate_ue_branch_engine(*args)
    _populate_ue_branch_project(*args)

#-------------------------------------------------------------------------------
def _populate_ue_branch_engine(depot_path, dry_run):
    minimal_sync_specs = (
        depot_path / "Engine/Build/Build.version",
        depot_path / "Engine/Source/*.Target.cs",
        depot_path / "*",
    )
    if dry_run:
        print(*minimal_sync_specs, sep="\n")
        return

    count = 0
    for item in P4.sync(*minimal_sync_specs).read(on_error=False):
        count += 1
        print(item.depotFile)
    print("synced:", count)

#-------------------------------------------------------------------------------
def _populate_ue_branch_project(depot_path, dry_run):
    """
    class ProjectRoots(object):
        def __init__(self): self._root_dirs = []
        def __iter__(self): return iter(self._root_dirs)
        def on_text(self, data):
            import io
            for line in io.StringIO(data.decode()):
                line = line.strip()
                if not line.startswith(";"):
                    self._root_dirs.append(line)

    project_roots = ProjectRoots()
    prnt = P4.print(depot_path / "*.uprojectdirs", q=True)
    for x in prnt.read(on_text=project_roots.on_text):
        pass

    uproject_specs = (depot_path / x / "*/*.uproject" for x in project_roots)
    for item in P4.files(uproject_specs).read(on_error=False):
        uproject_path = PurePosixPath(item.depotFile)
        # ignore Templates/ ??|?
        print(uproject_path.stem, uproject_path.parent)
    """
    pass



#-------------------------------------------------------------------------------
class Workspace(flow.cmd.Cmd):
    """ Creates a new workspace and sync the bare minimum. """
    localdir  = flow.cmd.Arg(str, "Directory to create the new workspace in")
    depotpath = flow.cmd.Arg("", "Depot path to map the workspace to")
    name      = flow.cmd.Opt("", "The name of the workspace to use")
    dryrun    = flow.cmd.Opt(False, "Do stuff but don't do it for real")

    def main(self):
        # Misc details
        username = p4utils.login()
        host_name = socket.gethostname()

        # Local FS directory
        client_dir = Path(self.args.localdir).resolve()
        if client_dir.is_dir():
            _check_dir_is_empty(client_dir)
        elif not self.args.dryrun:
            client_dir.mkdir(parents=True)
        os.chdir(client_dir)

        self.print_info("Environment")
        print("User:", username)
        print("Host:", host_name)
        print("Destination:", client_dir)

        # Depot path
        depot_path = self.args.depotpath
        if not depot_path:
            print("Building workspace depot path;", end="")
            depot_path = _build_depot_path()
            print()
        else:
            depot_path = depot_path.replace("\\", "/")
            depot_path = PurePosixPath(depot_path)

        if depot_path.root != "//" or len(depot_path.parts) < 2:
            raise ValueError("Depot path is expected to be in a //foo/bar format")

        is_stream = P4.streams(depot_path).run(on_error=False) is not None

        print("Depot path:", depot_path)
        print("Stream?:", "Yes" if is_stream else "No")

        # Generate a new client name and check it is availble
        self.print_info("Creating client")
        client_name = self.args.name or "_".join((
            username,
            host_name,
            client_dir.stem,
        ))
        if P4.client(client_name, t=client_name).run(on_error=False):
            raise ValueError(f"Workspace '{client_name}' already exists")
        print("Name:", client_name)

        # Create the workspace
        client_spec = {
            "Client"      : client_name,
            "Owner"       : username,
            "Host"        : host_name,
            "Options"     : "noallwrite clobber nocompress unlocked nomodtime rmdir",
            "Root"        : client_dir,
            "Description" : "Created with ushell's '.p4 workspace'"
        }
        if is_stream:
            client_spec["Stream"] = depot_path
        else:
            client_spec["View0"] = f"{depot_path}/... //{client_name}/..."

        if not self.args.dryrun:
            print("Creating...", end="")
            P4.client(i=True).run(input_data=client_spec)
            print(" done")

        # Write a P4CONFIG file
        if not self.args.dryrun:
            p4config_path = client_dir / p4utils.get_p4config_name()
            with p4config_path.open("wt") as out:
                print("P4CLIENT=", client_name, sep="", file=out)
                print("P4USER=", username)
            print("P4CONFIG:", p4config_path)

        # Is this an Unreal branch?
        self.print_info("Branch type")
        ue_depot_root = p4utils.get_branch_root(depot_path)
        if not str(depot_path)[len(ue_depot_root):]:
            print("Performing very minimal sync")
            _populate_ue_branch(depot_path, self.args.dryrun)
        else:
            print("New clientspec does not appear to be an Unreal Engine branch")



#-------------------------------------------------------------------------------
class MinSync(flow.cmd.Cmd):
    """ Syncs just enough to make a branch functional with ushell """
    dryrun = flow.cmd.Opt(False, "Try but don't try too hard")

    def main(self):
        p4utils.login()

        self.print_info("Perforce environment")
        info = P4.info().run()
        depot_path = f"//{info.clientName}"

        print("Cwd:", os.getcwd())
        print("Client:", info.clientName)
        print("Root:", depot_path)

        self.print_info("Syncing")
        depot_path = PurePosixPath(depot_path)
        _populate_ue_branch(depot_path, self.args.dryrun)
