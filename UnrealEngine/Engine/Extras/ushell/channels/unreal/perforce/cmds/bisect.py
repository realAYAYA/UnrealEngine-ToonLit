# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import p4utils
import unrealcmd
from peafour import P4
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class _Bisectomatron(object):
    def __init__(self, length):
        self._good = 0
        self._bad = length - 1
        self._jump = -1
        self._ugly = set()
        self._bisect()

    def get_good(self):  return self._good
    def get_bad(self):   return self._bad
    def get_index(self): return self._index
    def get_state(self): return self._good, self._index, self._bad, self._ugly

    def __bool__(self):
        return (self._bad - self._good) > len(self._ugly) + 1

    def _bisect(self):
        self._index = (self._good + self._bad) // 2
        while self._index in self._ugly:
            self._index += self._jump

    def good(self):
        self._ugly = {x for x in self._ugly if x > self._index}
        self._good = self._index
        self._jump = 1
        self._bisect()

    def bad(self):
        self._ugly = {x for x in self._ugly if x < self._index}
        self._bad = self._index
        self._jump = -1
        self._bisect()

    def ugly(self):
        self._ugly.add(self._index)
        while self:
            jump = self._jump
            self._jump = -jump - (jump // abs(jump))
            self._index += jump
            if self._index not in self._ugly:
                break



#-------------------------------------------------------------------------------
def _resolve_script_path(script_path):
    import shutil
    if whiched := shutil.which(script_path):
        return Path(whiched)

    script_path = Path(script_path).absolute()
    if script_path.is_file():
        return script_path

    raise ValueError(f"Script '{script_path}' does not exist");



#-------------------------------------------------------------------------------
class Bisect(unrealcmd.Cmd):
    """ Bisects a range of changelists between 'good' and 'bad' (exclusive). If
    a failed-build is reported, then bisection expands outwards from the active
    changelist until bisection resumed.

    The optional 'script' argument can be used to automate a bisection. Which change
    is considered next is controlled by the scripts exit code; 0=good, 80=bad, and
    90=failed_build. The exit codes are chosen to avoid interference from general
    scripting errors. Here is an example Batch script that could be used with the
    command line '.p4 bisect 123 456 path/to/script.bat test_case debug';

      @echo off
      .build editor %2
      if errorlevel 1 exit 90
      .run editor %2 -- -stdout -ExecCmds="Automation RunTests %1;Quit"
      if errorlevel 1 exit 80

    Here the 'test_case' and 'debug' arguments given when running the bisect are
    passed on to the script and appear in '%1' and '%2' respectively. """
    good       = unrealcmd.Arg(int, "First changelist that is in good shape")
    bad        = unrealcmd.Arg(int, "Changelist that is known to be bad")
    script     = unrealcmd.Arg([str], "Path and arguments to a script to automate bisection")
    dryrun     = unrealcmd.Opt(False, "Do not sync or run the given script")
    silentsync = unrealcmd.Opt(False, "Hide the output from '.p4 sync'")
    clsfromcwd = unrealcmd.Opt(False, "Use the current directory to build candidate changelists")

    def _user_choice(self):
        return input("[g]ood, [b]ad, [f]ailed-build? ").lower()

    def _script_choice(self):
        if self.args.dryrun:
            return _user_choice()

        cmd = self._script
        if self._script[0].endswith(".py"):
            cmd = (sys.executable, *cmd)
        ret = sp.run(cmd).returncode
        if ret == 80: return "b"
        if ret ==  0: return "g"
        if ret == 90: return "f"

        self.print_warning("Unexpected script exit code;", ret)
        return self._user_choice()

    def _get_candidate_changes(self, good, bad):
        # Check we've a branch-type UE context to get paths from
        ue_context = None
        use_cwd = self.args.clsfromcwd
        try:
            ue_context = self.get_unreal_context()
            use_cwd = use_cwd or (ue_context.get_branch() == None)
        except EnvironmentError:
            use_cwd = True

        # Fetch changes
        depot_paths = []
        if use_cwd or not ue_context:
            depot_paths += "..."
        else:
            depot_paths.append(ue_context.get_engine().get_dir() / "...")
            if project := ue_context.get_project():
                depot_paths.append(project.get_dir() / "...")

        temp_cls = []
        self.print_info("Fetching changes from Perforce")
        for i, depot_path in enumerate(depot_paths):
            print(i, depot_path, end="")
            temp_cls += [x for x in P4.changes(f"{depot_path}@{good},@{bad}", L=True)]
            print()

        # Sort and dedupe
        temp_cls.sort(key=lambda x: x.change)

        changes = temp_cls[:1]
        for i, candidate in enumerate(temp_cls[1:]):
            if candidate.change != temp_cls[i].change:
                changes.append(candidate)

        # Can't bisect a range that's too small
        if len(changes) < 3:
            raise EnvironmentError(f"Perforce returned too few changelists between {good} and {bad}")
        print(len(changes), " changes to bisect")

        # Remove CRs and LFs for neat(ish) printing
        for item in changes:
            item.desc = item.desc.replace("\n", " ")
            item.desc = item.desc.replace("\r", "")

        return changes

    def main(self):
        p4utils.login()

        # Work out how we're going to interrogate bisect progress
        self.print_info("Operating mode")
        if self.args.script:
            script, *args = self.args.script
            script = _resolve_script_path(script)
            self._script = (str(script), *args)
            choice = self._script_choice
            print("Script:   ", script)
            print("Arguments:", *args)
        else:
            print("Interactive")
            choice = self._user_choice

        # Our range of candidates submits must contain one good and one bad
        # submit as we're really looking for a state change.
        good, bad = self.args.good, self.args.bad
        if good >= bad:
            raise ValueError("Invalid range of changelists given (f >= l)")

        # Ask Perforce for some changes
        changes = self._get_candidate_changes(good, bad)

        # Run the bisect
        self.print_info("Bisection")
        sync_stdout = sp.DEVNULL if self.args.silentsync else None
        index_generator = _Bisectomatron(len(changes))
        while index_generator:
            index = index_generator.get_index()
            change = changes[index]

            # Sync
            if not self.args.dryrun:
                print(f"Syncing {change.change}...", end="" if sync_stdout else "\n")
                sync_cl = changes[index].change
                sync_ret = sp.run(
                    ("_p4", "sync", sync_cl),
                    stdout=sync_stdout
                )
                if exitcode := sync_ret.returncode:
                    print()
                    raise RuntimeError(f"Failed syncing changelist {sync_cl} (exitcode={exitcode})")
                print(" done")

            # Pause and and ask the user what to do next.
            while True:
                good = index_generator.get_good()
                bad = index_generator.get_bad()
                print(f"\n\n{changes[index].change} ({good},{index},{bad}) {changes[index].desc[:64]}")

                c = choice()
                if c == "g":
                    # Search earlier
                    index_generator.good()
                    break
                elif c == "b":
                    # Search later
                    index_generator.bad()
                    break
                elif c == "f":
                    # Expand out from index looking for a successful compile
                    index_generator.ugly()
                    break
                else:
                    print(f"Unknown input '{c}'!")

        good_index = index_generator.get_good()
        bad_index = index_generator.get_bad()
        good_cl = changes[good_index]
        bad_cl = changes[bad_index]

        print("")
        print("")
        print("Bisection complete")
        print("")
        print("Good change:", good_cl.change)
        print("Bad change: ", bad_cl.change)



#-------------------------------------------------------------------------------
def _test_bisectomatron():
    import random
    random.seed(37)

    j = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    def run(length, error_at, ugly=None):
        def print_legend():
            if ugly:
                ugs = bytearray(b" " * (ugly[1] + 1))
                for x in range(ugly[0], ugly[1] + 1):
                    ugs[x] = ord("x")
                print(" ", ugs.decode(), sep="")
            guide = ("_" * error_at) + "|" + ("-" * (length - error_at - 1))
            print("[", guide, "]", sep="")

        line = bytearray(b" " * length)
        b = _Bisectomatron(length);
        i = 0
        while b:
            index = b.get_index()
            line[index] = j[i]
            if ugly and index >= ugly[0] and index <= ugly[1]: b.ugly()
            elif index < error_at: b.good()
            else: b.bad()
            i += 1
        line[b.get_bad()] = ord("*")

        ok = b.get_bad() == error_at
        if ugly:
            ok = error_at > b.get_good() and error_at <= b.get_bad()
        if not ok:
            print_legend()
            print(" ", line.decode(), sep="")
            print()
            print(b.get_good(), b.get_bad())

    for i in range(50000):
        print(i, end="\r")
        length = random.randint(0, 55) + 3
        error_at = 1 + random.randint(0, length - 2)
        run(length, error_at)
    print()

    for i in range(50000):
        print(i, end="\r")
        length = random.randint(3, 55)
        error_at = random.randint(1, length - 2)
        ugly = [0, 0]
        ugly[0] = random.randint(1, length - 2)
        ugly[1] = random.randint(ugly[0], length - 2)
        run(length, error_at, ugly)
