# Copyright Epic Games, Inc. All Rights Reserved.

import p4utils
import flow.cmd
from peafour import P4

#-------------------------------------------------------------------------------
class _TtyCounter(object):
    def __init__(self, leader, suffix):
        self._suffix = suffix
        self._count = 0
        self._leader = leader
        self.flush(True)

    def inc(self):
        self._count += 1
        self.flush()

    def flush(self, inc_leader=False):
        if inc_leader:
            print(self._leader, end="")
        out = str(self._count) + self._suffix
        print(out, "\b" * len(out), sep="", end="")
        return self



#-------------------------------------------------------------------------------
class MergeDown(flow.cmd.Cmd):
    """ Merge down from the parent stream. """
    changelist    = flow.cmd.Arg("", "Changelist to merge down at")
    path          = flow.cmd.Opt("", "Limit mergedown to a sub-tree; --path=Engine/Source")
    maxscanrows   = flow.cmd.Opt(0, "Set the max scan rows with integrating")
    dryrun        = flow.cmd.Opt(False, "Only pretend to do work")
    noautoresolve = flow.cmd.Opt(False, "Do not automatically resolve")

    @flow.cmd.Cmd.summarise
    def main(self):
        p4utils.login()

        # Determine the streams involved
        self.print_info("Getting stream info")
        info = P4.info()
        stream = getattr(info, "clientStream", None)
        if not stream:
            raise EnvironmentError(f"Client '{info.clientName}' is not a stream")

        # Get the parent, skipping past virtual streams.
        stream_info = P4.stream(stream, o=True).run()
        parent = stream_info.Parent
        while True:
            stream_info = P4.stream(parent, o=True).run()
            if stream_info.Type != "virtual":
                break
            parent = stream_info.Parent

        if self.args.changelist:
            parent_cl = self.args.changelist
        else:
            parent_cl = P4.changes(parent + "/...", m=1, s="submitted").run()
            parent_cl = parent_cl.change

        print(f"Target: {stream} (as {info.clientName})")
        print(f"Parent: {parent}")
        print(f"Change: {parent_cl}")

        # Create a changelist to merge into
        if self.args.path:
            spec_stem = f"/{self.args.path}/...@".replace("\\", "/")
            print("Filter:", stream + spec_stem + parent_cl)
        else:
            spec_stem = "/...@"

        # Create a changelist to merge into
        def _create_cl(desc):
            cl_spec = {
                "Change" : "new",
                "Description" : desc,
            }
            P4.change(i=True).run(input_data=cl_spec)
            return P4.changes(c=info.clientName, m=1, s="pending").change

        self.print_info("Creating target changelist")
        cl_desc = f"Merging {parent} @ {parent_cl} to {stream} ({info.clientName})"
        cl_desc += "\n\n#ushell-mergedown"
        dest_cl = _create_cl(cl_desc)
        print(dest_cl)

        # Do the merge
        self.print_info("Merging")

        path_args = (
            stream + spec_stem + parent_cl,
        )

        int_args = {
            "c" : dest_cl,
            "n" : self.args.dryrun,
            "S" : stream,
            "r" : True,
        }

        def on_error(data):
            msg = data.data.strip()
            print(end="\r")
            self.print_error(msg)
            file_count.flush(True)
            file_count.inc()

        file_count = _TtyCounter("Running 'p4 integrate'; ", " files")
        p4 = P4(zmaxscanrows=str(self.args.maxscanrows or 1000000000))
        integrate = p4.integrate(*path_args, **int_args)
        for item in integrate.read(on_error=on_error):
            path = getattr(item, "clientFile", None)
            if path:
                file_count.inc()
        print("\nDone!")

        if self.args.dryrun:
            return

        # Resolve safely
        def do_resolve(cl, *modes):
            mode_args = {x:True for x in modes}
            yield from P4.resolve(c=cl, **mode_args).read(on_error=False)

        self.print_info("Resolving")
        file_count = _TtyCounter("Safely; ", " files")
        for item in do_resolve(dest_cl, "as"):
            path = getattr(item, "clientFile", None)
            if path:
                file_count.inc()

        # Move files that didn't resolve to a review changelist
        def create_review_cl():
            lines = ("AUTO MERGED/UNRESOLVED", cl_desc, "#nocheckin")
            review_cl_desc = "\n\n".join(lines)
            review_cl = _create_cl(review_cl_desc)
            print("Moving unresolved to " + review_cl)
            return review_cl

        review_cl = None
        try:
            for item in do_resolve(dest_cl, "n"):
                review_cl = review_cl or create_review_cl()
                path = getattr(item, "clientFile", None)
                if path:
                    P4.reopen(path, c=review_cl).run()
        except P4.Error:
            pass

        # Perhaps there's nothing to review? Hooray!
        if not review_cl:
            return

        # Make sure moved-pairs are in the same changelist
        def read_moved_pairs():
            for item in P4.opened(c=review_cl):
                move_pair = getattr(item, "movedFile", None)
                if move_pair:
                    yield move_pair

        file_count = _TtyCounter("Collecting moved pairs; ", " files")
        for item in P4.reopen(read_moved_pairs(), c=review_cl):
            file_count.inc()

        # Resolve what we can automatically.
        if not self.args.noautoresolve:
            file_count = _TtyCounter("Automatically; ", " files")
            for item in do_resolve(review_cl, "am"):
                path = getattr(item, "clientFile", None)
                if path:
                    file_count.inc()
