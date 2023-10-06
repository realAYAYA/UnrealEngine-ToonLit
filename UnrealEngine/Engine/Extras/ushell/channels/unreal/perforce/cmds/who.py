# Copyright Epic Games, Inc. All Rights Reserved.

import re
import os
import fzf
import time
import shutil
import p4utils
import tempfile
import flow.cmd
from peafour import P4
import subprocess as sp

#-------------------------------------------------------------------------------
def _as_p4_path(path_rev):
    path, rev = map(str, path_rev)
    hash_pound = "" if "@" in str(rev) else "#"
    return f"{path}{hash_pound}{rev}"

#-------------------------------------------------------------------------------
# 'p4 diff2' can be very slow. We'll use Python's built in difflib
class _Difflib(object):
    def __init__(self, left_path_rev, right_path_rev):
        class P4PrintText(object):
            def __init__(self):   self.data = b""
            def on_text(self, t): self.data += t

        left_print = P4PrintText()
        on_text = left_print.on_text
        for x in P4.print(_as_p4_path(left_path_rev)).read(on_text=on_text):
            pass

        right_print = P4PrintText()
        on_text = right_print.on_text
        for x in P4.print(_as_p4_path(right_path_rev)).read(on_text=on_text):
            pass

        import io
        import difflib
        diff = difflib.unified_diff(
            list(io.StringIO(left_print.data.decode(errors="ignore"))),
            list(io.StringIO(right_print.data.decode(errors="ignore"))),
            fromfile="from",
            tofile="to"
        )
        self._diff_iter = iter(diff)
        self.stdout = self

    def readline(self, *args):
        ret = next(self._diff_iter, None)
        if ret:
            return ret.encode()
        return b""

    def wait(self): pass
    def close(self): pass

#-------------------------------------------------------------------------------
# 'p4 diff2' can be very slow, we can use 'git diff' and 'p4 print'
class _Diff2(object):
    _git_bin = "git"

    def __init__(self, left_path_rev, right_path_rev):
        self._left_path = None
        self._right_path = None

        # Fetch the left file
        left_fd, left_path = tempfile.mkstemp()
        on_text = lambda x: os.write(left_fd, x)
        for x in P4.print(_as_p4_path(left_path_rev)).read(on_text=on_text):
            pass
        os.close(left_fd)
        self._left_path = left_path

        # Fetch the right file
        right_fd, right_path = tempfile.mkstemp()
        on_text = lambda x: os.write(right_fd, x)
        for x in P4.print(_as_p4_path(right_path_rev)).read(on_text=on_text):
            pass
        os.close(right_fd)
        self._right_path = right_path

        # Spin up git diff
        diff = (
            _Diff2._git_bin,
            "diff",
            "--minimal",
            "--unified=1",
            "--diff-algorithm=histogram", # myers, patience
            left_path,
            right_path
        )
        proc = sp.Popen(diff, stdout=sp.PIPE, stderr=sp.DEVNULL)
        self.stdout = proc.stdout
        self.wait = proc.wait

    def __del__(self):
        def do_unlink(path):
            try: os.unlink(path)
            except (OSError, FileNotFoundError): pass

        if self._right_path: do_unlink(self._right_path)
        if self._left_path: do_unlink(self._left_path)

#-------------------------------------------------------------------------------
def _get_diff_hunk_for_line(right_line_no, left_path_rev, right_path_rev=None):
    if right_path_rev:
        proc = _Difflib(left_path_rev, right_path_rev)
    else:
        cmd = ("p4", "diff", "-Od", "-du", _as_p4_path(left_path_rev))
        proc = sp.Popen(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL)

    def inner_iter():
        for line in line_iter:
            if line.startswith(b"@@"):
                break
            yield line

        proc.stdout.close()
        proc.wait()

    offset = 0
    line_iter = iter(proc.stdout.readline, b"")
    for line in (x for x in line_iter if x.startswith(b"@@")):
        m = re.match(rb"@@\s+-(\d+),(\d+)\s+\+(\d+),(\d+)", line)
        if not m:
            continue
        left_no, left_n, right_no, right_n = map(int, m.groups())

        if right_line_no >= right_no and right_line_no <= right_no + right_n:
            return inner_iter(), (left_no, right_no)

        if right_no > right_line_no:
            break

        offset += left_n - right_n

    proc.stdout.close()
    proc.wait()
    return None, right_line_no + offset

#-------------------------------------------------------------------------------
def _remap_line_no(right_line_no, left_path_rev, right_path_rev=None):
    line_iter, param = _get_diff_hunk_for_line(right_line_no, left_path_rev, right_path_rev)
    if not line_iter:
        return param

    left_no, right_no = param
    for line in line_iter:
        if line[0] == ord(b"-"):
            left_no += 1
            continue

        if right_no == right_line_no:
            break

        if line[0] == ord(b" "):
            left_no += 1
            right_no += 1
        elif line[0] == ord(b"+"):
            right_no += 1

    return left_no

#-------------------------------------------------------------------------------
def _get_edit_rev(path_rev, line_no):
    annotate_iter = P4.annotate(_as_p4_path(path_rev), dwl=True, q=True).read()
    header = next(annotate_iter)
    try: lines = list(next(annotate_iter) for x in range(line_no))
    except RuntimeError: raise IndexError
    return int(lines[-1].lower)

#-------------------------------------------------------------------------------
def _select_line(path_rev):
    proc = sp.Popen(("p4", "print", "-q", _as_p4_path(path_rev)), stdout=sp.PIPE)

    def line_iter():
        for i, line in enumerate(proc.stdout):
            yield "%4d %s" % (i + 1, line.strip().decode())

    line_no = 0
    for line in fzf.run(line_iter()):
        if m := re.match(r"\s*(\d+)", line):
            line_no = int(m.group(1))
            break

    proc.wait()
    proc.stdout.close()
    return line_no



#-------------------------------------------------------------------------------
class Who(flow.cmd.Cmd):
    """ Find the changelist that modified a particular line of a file. Who will
    do its best to follow integrations and moves to find the changelist that edited
    the line. If no line number is given then an prompt will be given to select
    the line to follow. If 'path' has a '#rev' or '@cl' suffix then no remapping
    against local edits is skipped."""
    path      = flow.cmd.Arg(str, "Path to the file (P4 paths & #rev/@cl suffix allowed)")
    line      = flow.cmd.Arg(0, "Line number to check modifications of")
    printdiff = flow.cmd.Opt(False, "Print a unified diff of the line change - no P4V")
    noremap   = flow.cmd.Opt(False, "Skip remapping line numbers against local edits")

    def main(self):
        # Get the input file, we'll assume it has a good Perforce environment so
        # we'll chdir alongside it. Might be edited so diff it to adjust line
        self.print_info("Parsing input")
        path = self.args.path

        if "#" in path:
            path_rev = path.split("#", 1)
            remap_local = False
        elif "@" in path:
            path, rev = path.split("@", 1)
            path_rev = (path, "@" + rev)
            remap_local = False
        else:
            path_rev = (path, "head")
            remap_local = True
        path = path_rev[0]

        # Try and pick up a suitable Perforce environment from the on-disk file
        if os.path.isfile(path):
            path_rev = (path, "have")
            path_dir, path = os.path.split(path)
            if path_dir:
                os.chdir(path_dir)

        # Path needs converting to a depot path
        if os.path.isfile(path) or not path.startswith("//"):
            path_rev = (P4.where(path).depotFile, path_rev[1])
        print("Depot path:", path_rev[0])

        # Is the file a binary file?
        is_binary = False
        try:
            fstat = P4.fstat(path_rev[0] + "#head", m=1, T="headType").run()
            is_binary = "binary" in fstat.headType
        except P4.Error:
            pass

        if is_binary:
            raise ValueError("Unable to follow line-by-line edits of binary files")

        line_no = self.args.line
        if line_no <= 0:
            if self.is_interactive():
                print("Select a line from file;", end="")
                line_no = _select_line(path_rev)
            if line_no <= 0:
                self.print_error("No line was selected")
                return False
            print("Using line", line_no)
        elif remap_local and not self.args.noremap:
            for _ in P4.opened(path_rev[0]):
                self.print_warning(path_rev[0], "is open for edit")
                print("Attempting to remap line number against local edits")
                line_no = _remap_line_no(line_no, path_rev)
                print("Line", line_no, "perhaps")
                break

        # Use annotate to find out how a line changed, going further if the
        # change it was an integration or branch
        self.print_info("Following edits", flow.cmd.text.grey("... (a)nnotate (f)ilelog (d)iff"))
        who_cl = 0
        while not who_cl:
            print("   ", _as_p4_path(path_rev), end="\r")
            def print_step(x):
                print(flow.cmd.text.grey(x), end="")

            # Who last edited out target line?
            print_step("a")
            try:
                edit_rev = _get_edit_rev(path_rev, line_no)
                edit_path_rev = (path_rev[0], edit_rev)
            except IndexError:
                raise IndexError(f"Line {line_no} does not seem to exist")

            # Adjust the edit path if it came from somewhere else
            print_step("f")
            log = P4.filelog(_as_p4_path(edit_path_rev), s=True, m=1).run()
            action = getattr(log, "action0")
            if action not in ("branch", "integrate", "edit", "add", "move/add"):
                assert False, "Unknown action; " + action + str(log.as_dict())

            if action not in ("edit", "add"):
                try:
                    for how, path, rev in zip(getattr(log, "how0,"), getattr(log, "file0,"), getattr(log, "erev0,")):
                        if "from" in how:
                            edit_path_rev = (path, int(rev[1:]))
                            break
                    else:
                        assert False, "no 'from' filelog.how found"
                except AttributeError:
                    # This is probably as far as we can go.
                    who_cl = getattr(log, "change0")
            else:
                who_cl = getattr(log, "change0")

            # Remap the line number from where we are (path-rev) to the edit
            print_step("d")
            edit_line_no = _remap_line_no(line_no, edit_path_rev, path_rev)
            if edit_line_no != line_no:
                print("\n   ", line_no, "->", edit_line_no, end="")

            path_rev = edit_path_rev
            line_no = edit_line_no
            print()

        # Inform the user about the change
        print()
        self.print_info("Attribution")
        white = flow.cmd.text.white
        print(white("Where:     "), _as_p4_path(edit_path_rev))
        print(white("Line:      "), edit_line_no)
        desc = P4.describe(who_cl).run()
        print(white("Changelist:"), desc.change)
        print(white("User:      "), desc.user)
        print(white("Time:      "), time.ctime(int(desc.time)))
        print(white("Description:"))
        print(flow.cmd.text.light_yellow(desc.desc.rstrip()), sep="")

        # Open the change in a gui.
        if not self.args.printdiff and self.is_interactive():
            try:
                print("Starting pv4c...", end="")
                p4utils.run_p4vc("change", desc.change)
                print()
                return
            except:
                print()
                self.print_warning("Unabled to start 'p4vc'")

        # Show a unfied diff of the line edit
        self.print_info("Diff")

        # Is the change an add action?
        if edit_path_rev[1] == 1:
            print("(No diff to show. This is revision 1)")
            return

        prev_path_rev = (edit_path_rev[0], edit_path_rev[1] - 1)
        hunk_iter, param = _get_diff_hunk_for_line(line_no, prev_path_rev, edit_path_rev)
        if not hunk_iter:
            return

        max_width = shutil.get_terminal_size(fallback=(9999,0))[0] - 2
        for line in hunk_iter:
            prefix = line[:1].decode()
            line = line[1:].rstrip().expandtabs(4).decode()

            decorator = lambda x: x
            if prefix == "+":   decorator = flow.cmd.text.green
            elif prefix == "-": decorator = flow.cmd.text.red
            print(decorator(prefix + line[:max_width]))
