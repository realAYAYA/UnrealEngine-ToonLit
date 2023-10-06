# Copyright Epic Games, Inc. All Rights Reserved.

import os
import fzf
import unreal
import p4utils
import flow.cmd
import subprocess
from peafour import P4

#-------------------------------------------------------------------------------
class _RestorePoint(object):
    def __init__(self):
        self._files = {}
        self._shelf = None

    def __del__(self):
        if not self._files:
            return

        print("Attemping to restore changelists")
        if self._shelf:
            print("Unshelving...")
            for item in P4.unshelve(s=self._shelf).read(on_error=False):
                pass

        print("Restoring...")
        for cl, paths in self._files.items():
            print(" ", cl)
            for item in P4.reopen(paths, c=cl).read(on_error=False):
                print("   ", item.depotFile)

    def set_shelf(self, shelf):
        self._shelf = shelf

    def add_file(self, changelist, path):
        out = self._files.setdefault(changelist, [])
        out.append(path)

    def clear(self):
        self._files.clear()

#-------------------------------------------------------------------------------
class _SwitchCmd(flow.cmd.Cmd):
    def _read_streams(self):
        depot = self._depot.lower()
        yield from (x for x in P4.streams() if x.Stream.lower().startswith(depot))

    def main(self):
        self.print_info("Perforce environment")

        # Ensure there's a valid branch and Perforce ticket
        branch = unreal.Context(".").get_branch()
        if not branch:
            raise EnvironmentError("Unable to find a valid branch")
        os.chdir(branch.get_dir())

        self._username = p4utils.login()

        # Find out where we are and the stream
        self._depot = None

        info = P4.info().run()
        self._src_stream = getattr(info, "clientStream", None)
        if self._src_stream:
            self._depot = "//" + self._src_stream[2:].split("/")[0] + "/"

        print("Client:", info.clientName)
        print("  User:", self._username)
        print("Stream:", self._src_stream)
        print(" Depot:", self._depot)
        if not self._src_stream:
            self.print_error(info.clientName, "is not a stream-based client")
            return False

        return self._main_impl()

#-------------------------------------------------------------------------------
class List(_SwitchCmd):
    """ Prints a tree of available streams relevant to the current branch """

    def _main_impl(self):
        self.print_info("Available streams")

        streams = {}
        for item in self._read_streams():
            stream = streams.setdefault(item.Stream, [None, []])
            stream[0] = item
            parent = streams.setdefault(item.Parent, [None, []])
            parent[1].append(item.Stream)

        def print_stream(name, depth=0):
            item, children = streams[name]
            prefix = ("| " * depth) + "+ "
            dots = "." * (64 - len(item.Name) - (2 * depth))
            print(flow.cmd.text.grey(prefix), item.Name, sep="", end=" ")
            print(flow.cmd.text.grey(dots), end=" ")
            print(item.Type)
            for child in sorted(children):
                print_stream(child, depth + 1)

        _, roots = streams.get("none", (None, None))
        if not roots:
            print("None?")
            return False
        for root in roots:
            print_stream(root)

#-------------------------------------------------------------------------------
class Switch(_SwitchCmd):
    """ Switch between streams and integrate any open files across to the new
    stream. If no stream name is provided then the user will be prompted to
    select a stream from a list. Operates on the current directory's branch.

    There maybe occasions where a stream is only partially synced like a temporary
    client used for cherrypicking. The '--haveonly' can be used in cases liek this
    to avoid syncing the whole branch post-switch and instead just update the
    files synced prior to switching. Use with extraordinary caution!"""
    stream      = flow.cmd.Arg("", "Name of the stream to switch to")
    changelist  = flow.cmd.Arg(-1, "The changelist to sync to ('head' if unspecified)")
    haveonly    = flow.cmd.Opt(False, "Only switch files synced prior to the switch")
    saferesolve = flow.cmd.Opt(False, "Resolve safely and not automatically")

    def _select_stream(self):
        self.print_info("Stream select")
        print("Enter the stream to switch to (fuzzy, move with arrows, enter to select)...", end="")
        stream_iter = (x.Name for x in self._read_streams())
        for reply in fzf.run(stream_iter, height=10, prompt=self._depot):
            return reply

    @flow.cmd.Cmd.summarise
    def _main_impl(self):
        # Work out the destination stream.
        if not self.args.stream:
            dest_stream = self._select_stream()
            if not dest_stream:
                self.print_error("No stream selected")
                return False
        else:
            dest_stream = self.args.stream
        dest_stream = self._depot + dest_stream

        # Check for the destination stream and correct its name
        self.print_info("Checking destination")
        stream = P4.streams(dest_stream, m=1).run()
        dest_stream = stream.Stream
        print("Stream:", dest_stream)
        print("Parent:", stream.Parent)
        print("  Type:", stream.Type)
        self._dest_stream = dest_stream

        if self._src_stream.casefold() == self._dest_stream.casefold():
            self.print_warning("Already on stream", self._dest_stream)
            return

        # Move the user's opened files into a changelist
        self.print_info("Shelving and reverting open files")
        self._get_opened_files()

        if len(self._opened_files):
            shelf_cl = self._shelve_revert()

        # Do the switch
        P4.client(S=self._dest_stream, s=True).run()
        if self.args.haveonly:
            self._do_have_table_update()
        else:
            self._do_sync()

        # Unshelve to restore user's open files
        if len(self._opened_files):
            self._restore_changes(shelf_cl)

    def _do_have_table_update(self):
        # This probably isn't correct if stream specs diff significantly
        # betweeen source and destination.

        def read_have_table():
            for item in P4.have():
                depot_path = item.depotFile
                yield self._dest_stream + depot_path[len(self._src_stream):]

        print("Updating have table; ", end="")
        for i, item in enumerate(P4.sync(read_have_table()).read(on_error=False)):
            print(i, "\b" * len(str(i)), end="", sep="")
        for x in P4.sync(self._src_stream + "/...#have").read(on_error=False):
            pass
        print()

    def _do_sync(self):
        self.print_info("Syncing")

        self.print_warning("If the sync is interrupted the branch will be a mix of files synced pre-")
        self.print_warning("and post-switch. Should this happen `.p4 sync` can be used to recover but")
        self.print_warning("any open files at the time of the switch will remain shelved.")

        sync_cmd = ("_p4", "sync", "--all", "--noresolve", "--nosummary")
        if self.args.changelist >= 0:
            sync_cmd = (*sync_cmd, str(self.args.changelist))

        popen_kwargs = {}
        if os.name == "nt":
            popen_kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP

        proc = subprocess.Popen(sync_cmd, **popen_kwargs)
        proc.wait()

    def _get_opened_files(self):
        self._restore_point = _RestorePoint();
        self._opened_files = {}
        src_len = len(self._src_stream) + 1
        for item in P4.opened():
            if not item.depotFile.startswith(self._src_stream):
                raise ValueError(f"Open file {item.depotFile} is not from the current stream {self._src_stream}")

            base_path = item.depotFile[src_len:]

            out = self._opened_files.setdefault(item.change, [])
            out.append(base_path)

            self._restore_point.add_file(item.change, base_path)

            prefix = item.change if len(out) == 1 else ""
            print("%-9s" % prefix, base_path, sep=": ")

    def _shelve_revert(self):
        def _for_each_open(command, info_text):
            def read_open_files():
                for x in self._opened_files.values():
                    yield from x

            print(info_text, "." * (22 - len(info_text)), end=" ")
            runner = getattr(P4, command)
            runner = runner(read_open_files(), c=shelf_cl)
            for i, item in enumerate(runner):
                print(i, "\b" * len(str(i)), sep="", end="")
            print("done")

        cl_desc = f"'.p4 switch {self.args.stream}' backup of opened files from {self._src_stream}\n\n#ushell-switch"
        cl_spec = {"Change": "new", "Description": cl_desc}
        P4.change(i=True).run(input_data=cl_spec)
        shelf_cl = P4.changes(u=self._username, m=1, s="pending").change

        _for_each_open("reopen", "Moving changelist")
        _for_each_open("shelve", "Shelving")
        _for_each_open("revert", "Reverting")

        self._restore_point.set_shelf(shelf_cl)

        return shelf_cl

    def _restore_changes(self, shelf_cl):
        # Unshelve each file into its original CL, integrating between streams
        # at the same time.
        self.print_info("Restoring changes")
        branch_spec = p4utils.TempBranchSpec("switch", self._username, self._src_stream, self._dest_stream)
        print("Branch spec:", branch_spec)
        for orig_cl, files in self._opened_files.items():
            print(orig_cl)
            orig_cl = None if orig_cl == "default" else orig_cl
            unshelve = P4.unshelve(files, b=branch_spec, s=shelf_cl, c=orig_cl)
            for item in unshelve.read(on_error=False):
                print("", item.depotFile)
        del branch_spec
        self._restore_point.clear()

        # Resolve files
        def print_error(error):
            print("\r", end="")
            self.print_error(error.data.rstrip())

        resolve_count = 0
        resolve_args = {
            "as" : self.args.saferesolve,
            "am" : not self.args.saferesolve,
        }
        resolve = P4.resolve(**resolve_args)
        for item in resolve.read(on_error=print_error):
            if getattr(item, "clientFile", None):
                resolve_count += 1
                print("\r" + str(resolve_count), "file(s) resolved", end="")
        if resolve_count:
            print("")

        # Inform the user about conflicts
        resolve = P4.resolve(n=True)
        for item in resolve.read(on_error=False):
            if name := getattr(item, "fromFile", None):
                self.print_error(name[len(self._src_stream) + 1:])
