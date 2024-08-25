# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import p4utils
import flow.cmd
import subprocess
from peafour import P4

#-------------------------------------------------------------------------------
def _untag_desc(desc):
    tags = ("#fyi", "#review", "#codereview", "#robomerge")
    out = ""
    for line in desc.splitlines():
        for tag in (x for x in tags if x in line):
            line = line.replace(tag, f"#[{tag[1:]}]")
        out += line + "\n"

    return out



#-------------------------------------------------------------------------------
class Cherrypick(flow.cmd.Cmd):
    """ Integrates/unshelves one or more changelists into the current branch,
    resolving and clearing integration records as required. To clear integration
    records only give a single pending changelist from the current client. Providing
    a value for 'path' allows the cherrypick to be limited to a subset of files.
    'path' should be either relative to a branch root; Engine/.../Runtime, or a
    full depot path at the destination; //Dest/Stream/Engine/Source/... t.ex. The
    command will *not* submit anything on your behalf. If in doubt you can use
    '--dryrun' to preview the action. """
    changelist    = flow.cmd.Arg([int], "Changelist or shelve to edit-integrate")
    path          = flow.cmd.Opt("", "Restrict cherrypick to the given path (e.g. Engine/...)")
    saferesolve   = flow.cmd.Opt(False, "Resolve safely and not automatically")
    dryrun        = flow.cmd.Opt(False, "Only pretend to do the cherrypick")
    force         = flow.cmd.Opt(False, "Force the operation through")
    novalidate    = flow.cmd.Opt(False, "Don't run the validation step")
    alwayseddy    = flow.cmd.Opt(False, "Always edigrate the result regardless of relation")
    noeddy        = flow.cmd.Opt(False, "Skip the edigrate step")
    sync          = flow.cmd.Opt(False, "Syncs target files to head before resolving")
    virtual       = flow.cmd.Opt(False, "Perform the integration server-side")
    rawbranchspec = flow.cmd.Opt(False, "Ignore the internal branchspec that Perforce generates")

    complete_changelist = False

    def get_explicit_relations(self):
        return ()

    def main(self):
        if not self.args.changelist:
            raise ValueError("No changelist(s) given")

        # Check the user's logged into the current Perforce environment
        username = p4utils.login()

        # Get info about where we're cherrypicking to
        self.print_info("Determining destination")
        info = P4.info().run()
        print("Server:", getattr(info, "proxyAddress", info.serverAddress))
        print("Client:", info.clientName)

        if info.clientName == "*unknown*":
            raise EnvironmentError("Unknown Perforce client")

        # Don't let the user cherrypick to somewhere out of where they think they
        # are inadvertently.
        cwd = os.getcwd()
        if os.path.normpath(info.clientRoot).lower() not in cwd.lower():
            raise EnvironmentError(f"Directory '{cwd}' is not under client '{info.clientName}'")

        dest_where = P4.where("Engine").depotFile
        dest_root = p4utils.get_branch_root(dest_where)
        print("Root path: " + dest_root)

        # Condition self.args.path
        if path := self.args.path:
            path = path.replace("\\", "/")
            if path.startswith("//"):
                path = "//" + "/".join(x for x in path.split("/") if x)
                if not path.lower().startswith(dest_root.lower()):
                    raise ValueError(f"'{path}' is unrelated to '{dest_root}'")
            else:
                path = dest_root + "/".join(x for x in path.split("/") if x)
            self.args.path = path

        # Get information about each input changelist
        self.print_info("Fetching info on changelists to process")
        descs = []
        for cl in sorted(self.args.changelist):
            print(cl, end="")
            desc = P4.describe(str(cl), S=True).run()
            descs.append(desc)
            print(":", desc.desc.replace("\n", " ")[:68])

        # If there's only one changelist and it's pending on the current client
        # then clear integration records.
        if len(descs) == 1:
            desc = descs[0]
            if desc.client == info.clientName and desc.status == "pending":
                print("Change is pending changelist on current client")
                return self._clear_integration_records(desc.change)

        # Work out the branch root of each input changelist
        self.print_info("Determining branch roots")
        pick_rota = []
        branch_roots = set()
        for desc in descs:
            path = getattr(desc, "path", None)
            if not path:
                depot_file = getattr(desc, "depotFile", None)
                path = next(depot_file) if depot_file else ""

            for branch_root in branch_roots:
                if path.startswith(branch_root):
                    break
            else:
                branch_root = p4utils.get_branch_root(path) if path else ""
                branch_roots.add(branch_root)

            print(desc.change, branch_root)
            pick_rota.append((branch_root, desc))

        # Validate that we can proceed.
        self.print_info("Validating input changelists")
        for root, desc in pick_rota:
            print(desc.change, end="")
            if desc.status == "pending":
                if not hasattr(desc, "shelved"):
                    print(" ... fail")
                    raise ValueError(f"Pending changelist {desc.change} has no shelved files")
            elif root == dest_root:
                print(" ... fail")
                raise ValueError(f"Changelist {desc.change} already submitted to "
                 f"cherrypick destination '{dest_root}'")
            elif not root:
                print(" ... fail")
                raise ValueError(f"Unable to determine branch root for {desc.change}")

            print(" ... ok")

        # Get a set of the destinations files
        dest_paths = set()
        for root, desc in pick_rota:
            skip = len(root)
            for src_path in desc.depotFile:
                dest_paths.add(dest_root + src_path[skip:])

        # Filter destination files by --path=
        if path_spec := self.args.path:
            print(f"Limiting input to {path_spec} : ", end="")

            path_spec = path_spec.replace("...", "@")
            path_spec = path_spec.replace(".", "\\.")
            path_spec = path_spec.replace("*", "[^/]*")
            path_spec = path_spec.replace("@", ".*")

            prev_dest_paths_len = len(dest_paths)
            dest_paths = [x for x in dest_paths if re.search(path_spec, x, re.IGNORECASE)]
            print(prev_dest_paths_len - len(dest_paths), "of", prev_dest_paths_len, "removed")

        # Check that none of the destination files are aleady opened for edit
        self.print_info("Validating destination")
        print("Affected files:", len(dest_paths))
        print("Checking for open files")

        validate = not (self.args.novalidate or self.args.force)
        if len(dest_paths) > 500:
            validate = False
            self.print_warning("Skipping validation. Source changelist is too big")

        if validate:
            def read_dest_paths_and_count():
                count = 1
                for x in dest_paths:
                    print("\r" + str(count), "file(s) checked", end="")
                    count += 1
                    yield x

            blocked = False
            opened = P4.opened(read_dest_paths_and_count())
            for item in opened:
                self.print_warning("\r", item.depotFile[len(dest_root):])
                blocked = True

            if blocked:
                raise EnvironmentError("The cherrypick of changelist"
                    f" {desc.change} is blocked because some files are"
                    " already open for edit at the destination.")

            print()
            print("All good. No target files were found to be already open for edit")
        elif self.args.force:
            self.print_warning("Validation was explicitly skipped")

        # Create a target changelist for the cherrypick(s)
        cl_desc = ""
        for _, desc in pick_rota:
            cl_desc += _untag_desc(desc.desc).rstrip()
            cl_desc += "\n"

        for _, desc in pick_rota:
            cl_desc += f"\n#ushell-cherrypick of {desc.change} by {desc.user}"

        cl_spec = { "Change" : "new", "Description" : cl_desc }
        P4.change(i=True).run(input_data=cl_spec)
        dest_cl = P4.changes(c=info.clientName, m=1, s="pending").change

        # Integrate or unshelve each input changelist
        specs = {}
        self.print_info("Cherrypicking into", dest_cl)
        for src_root, desc in pick_rota:
            cl = desc.change

            branch_spec, file_spec = specs.get(src_root, (None, None))
            if not branch_spec:
                print("Creating branch spec for", src_root)
                branch_spec = p4utils.TempBranchSpec("cherrypick", username, src_root, dest_root, self.args.rawbranchspec)
                print(str(branch_spec))

                file_spec = src_root + "..."
                if path := self.args.path:
                    file_spec = src_root + path[len(dest_root):]
                    print(f"Limiting to path '{file_spec}'")

                specs[src_root] = branch_spec, file_spec

            def on_info(info):
                output = info.data
                if "must resolve" in output or "also opened" in output:
                    return
                print("")
                self.print_warning(info.data)

            p4_args = {
                "b" : branch_spec,
                "n" : self.args.dryrun,
                "f" : self.args.force,
                "v" : self.args.virtual,
                "c" : dest_cl,
            }
            if desc.status == "pending":
                p4_args["-bypass-exclusive-lock"] = True
                print(f"Unshelving {cl} from", src_root, end="")
                file_spec_dest = file_spec.replace(src_root, dest_root)
                unshelve = P4.unshelve(file_spec_dest, s=cl, **p4_args)
                for x in unshelve.read(on_info=on_info): pass
                print("")
            else:
                print(f"Integrating {cl} from", src_root, end="")
                integrate = P4.integrate(s=file_spec + f"@{cl},@{cl}", **p4_args)
                for x in integrate.read(on_info=on_info): pass
                print("")

        # We can't do anymore pretending after this point.
        if self.args.dryrun:
            P4.change(dest_cl, d=True).run()
            return

        # Collect files that might be in other changelists
        if self.args.force:
            P4.reopen(dest_paths, c=dest_cl).run(on_error=False)

        self.print_info("Resolving")

        # Sync to latest if requested to do so
        if self.args.sync:
            print("Syncing to head first (--sync)", end="")
            def read_sync_paths(cl):
                for item in P4.opened(c=cl):
                    yield item.depotFile
            sync = P4.sync(read_sync_paths(dest_cl), q=True)
            for x in sync.read(on_error=False):
                pass
            print("")

        # Resolve
        resolve_count = 0

        def print_error(error):
            print("\r", end="")
            msg = error.data.rstrip()
            if msg.startswith("No file(s)"):
                print(msg)
            else:
                self.print_error(msg)

        resolve_args = {
            "as" : self.args.saferesolve,
            "am" : not self.args.saferesolve,
            "f"  : self.args.force,
            "c"  : dest_cl,
        }
        resolve = P4.resolve(**resolve_args)
        for item in resolve.read(on_error=print_error):
            if getattr(item, "clientFile", None):
                resolve_count += 1
                print("\r" + str(resolve_count), "file(s) resolved", end="")
        if resolve_count:
            print("")

        # Report conflicted files
        resolve = P4.resolve(c=dest_cl, n=True)
        for item in resolve.read(on_error=False):
            if name := getattr(item, "fromFile", None):
                self.print_error(name[len(src_root):])

        # Queue up some details to display when everything's complete
        class OnExit(object):
            def __del__(self):
                print("\nCherrypick complete;", dest_cl)
        on_exit = OnExit()

        # If there are branch/integrates in dest_cl there's nothing more to do
        for item in P4.opened(c=dest_cl):
            if item.action in ["integrate", "branch"]:
                break
        else:
            print("No integrated files found")
            return

        # Post-process the cherrypick'd files.
        self.print_info("Processing integration records")
        if self.args.noeddy:
            self.print_warning("Skipped")
            return

        # There's nothing more to do if the source and destination are related.
        relations_allowed = (len(specs) == 1)
        relations_allowed &= (descs[0].status != "pending")
        relations_allowed &= not self.args.alwayseddy
        dest_stream = getattr(info, "clientStream", None)
        if dest_stream and relations_allowed:
            print("Checking relations between src and dest")

            def get_stream_parent(stream):
                stream_info = P4.stream(stream, o=True).run()
                parent = stream_info.Parent
                while parent.startswith("//"):
                    stream_info = P4.stream(parent, o=True).run()
                    if stream_info.Type != "virtual":
                        break
                    parent = stream_info.Parent
                return parent

            # Check if we've fetched from a parent stream.
            dest_parent = get_stream_parent(dest_stream)
            if src_root.startswith(dest_parent):
                print(f"Not required; {src_root} and {dest_root} are related")
                return

            # Check if we've fetch from a child stream
            src_client = P4.client(descs[0].client, o=True).run()
            src_stream = getattr(src_client, "Stream", None)
            if src_stream and dest_root.startswith(get_stream_parent(src_stream)):
                print(f"Not required; {src_root} and {dest_root} are related")
                return

        # No stream relation but maybe there's an explicit branch relation
        if relations_allowed:
            explicit_relations = self.get_explicit_relations()
            for relatives in explicit_relations:
                related = src_root.startswith(relatives[0]) and dest_root.startswith(relatives[1])
                related |= src_root.startswith(relatives[1]) and dest_root.startswith(relatives[0])
                if related:
                    print("Not required; explicit relations;")
                    print("             ", relatives[0])
                    print("             ", relatives[1])
                    return

        ret = self._clear_integration_records(dest_cl)

        return ret

    def _resolve_prompt(self, changelist):
        P4.resolve(c=changelist, n=True).run()
        self.print_error("There are pending conflicts which must be resolved before continuing")
        print("Now you have a few courses of action available to you;")
        print("  [r]esolve with P4V")
        print("  [c]ommand line resolve")
        print("  re[v]ert and exit")
        print("  retry [Enter]")
        print("  abort [Ctrl-C]")

        choice = input("Which one do you fancy? [r/c/v/Enter/Ctrl-C] ")
        try:
            if choice == "c":
                cmd = ("p4", "resolve", "-du", "-c", changelist)
                subprocess.run(cmd)
            elif choice == "r":
                #p4utils.run_p4vc("p4vc", "resolve", "-c", changelist, "...") # this just doesn't work...
                p4utils.run_p4vc("pendingchanges")
                input("Press Enter to continue (Ctrl-C to abort)...")
            elif choice == "v":
                self.print_info("")
                cmd = ("p4", "revert", "-wc", changelist, "...")
                subprocess.run(cmd)
                cmd = ("p4", "change", "-d", changelist)
                subprocess.run(cmd)
                return False
        except FileNotFoundError:
            self.print_error("Failed to run command;", *cmd)

        print()

    def _clear_integration_records(self, changelist):
        self.print_info("Clearing integration records from", changelist)

        # Editgrates are destructive so we'll need to interact with the user if
        # they have pending resolves.
        try:
            while True:
                self._resolve_prompt(changelist)
        except P4.Error:
            pass

        # Check the pending changelist is a pending
        open_files = list(P4.opened(c=changelist))
        if not open_files:
            self.print_warning(f"Changelist {changelist} is empty")
            return False

        # Work out what to do with each file
        to_edit = []
        to_add = []
        to_delete = []
        to_move = []
        action_map = {
            "edit"          : to_edit,
            "integrate"     : to_edit,
            "add"           : to_add,
            "branch"        : to_add,
            "delete"        : to_delete,
            "move/delete"   : to_move,
            "move/add"      : None,
        }

        for item in open_files:
            action_list = action_map.get(item.action)
            if action_list != None:
                action_list.append(item)

        print("   Adds:", len(to_add))
        print("  Edits:", len(to_edit))
        print("Deletes:", len(to_delete))
        print("  Moves:", len(to_move))
        print("  Total:", len(open_files))

        self.print_info("Clearing integration records")

        # Sync any edited files that are not on the client
        to_sync = [x.depotFile for x in to_edit if x.haveRev == "none"]
        if to_sync:
            print("Syncing edited files that are not on the client...", end="")
            for item in P4.sync(to_sync, q=True):
                pass
            print("done")

        def read_files(items):
            for item in items:
                yield item.depotFile

        # Server-revert and edit/add/delete files to drop integrate records.
        print("Reverting server side .. ", end="")
        revert = P4.revert("//...", c=changelist, k=True)
        for action in revert:
            pass
        print("done")

        p4args = {
            "c" : changelist,
        }

        if to_edit:
            print("Reopening .............. ", end="")
            for item in P4.edit(read_files(to_edit), **p4args):
                pass
            print("done")

        if to_add:
            print("Adding ................. ", end="")
            for item in P4.add(read_files(to_add), c=changelist):
                pass
            print("done")

        if to_delete:
            print("Deleting ............... ", end="")
            for item in P4.delete(read_files(to_delete), **p4args):
                pass
            print("done")

        if to_move:
            print("Applying moves;")
            print("  Reopening ............ ", end="")
            for item in P4.edit(read_files(to_move), **p4args):
                pass
            print("done")

            print("  Moving ............... ", end="")
            for item in to_move:
                P4.move(item.depotFile, item.movedFile, **p4args).run()
            print("done")

        return True
