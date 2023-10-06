# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import flow.cmd
from peafour import P4
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class _Collector(object):
    def __init__(self):
        self._dirs = []
        self._careful_dirs = []
        self._pinned_files = set()

    def get_pinned_count(self):
        return len(self._pinned_files)

    def add_dir(self, path, *, remove_carefully=False):
        (self._careful_dirs if remove_carefully else self._dirs).append(path)

    def pin_file(self, path):
        path = str(path.resolve())
        self._pinned_files.add(path.lower())

    def read_careful_dirs(self):
        return (x for x in self._careful_dirs)

    def dispatch(self, actuator):
        actuator.begin()

        for dir in self._dirs:
            actuator.remove_dir(dir)

        for dir in self._careful_dirs:
            for item in (x for x in dir.rglob("*") if x.is_file()):
                key = str(item.resolve()).lower()
                if key not in self._pinned_files:
                    actuator.remove_file(item)

        actuator.end()

#-------------------------------------------------------------------------------
class _DryRun(object):
    def _spam(self):
        mb_size = format(self._bytes // 1024 // 1024, ",") + "MB"
        print("%d files to remove (%s)" % (self._count, mb_size), end="\r")

    def begin(self):
        self._bytes = 0
        self._count = 0
        self._breakdown = {}

    def remove_dir(self, path):
        for item in (x for x in path.rglob("*") if x.is_file()):
            self.remove_file(item)

    def remove_file(self, path):
        self._bytes += path.stat().st_size
        self._count += 1
        if self._count % 493:
            self._spam()

    def end(self):
        self._spam()
        print()

#-------------------------------------------------------------------------------
class _Cleaner(object):
    def __init__(self, work_dir):
        self._work_dir = work_dir

    def __del__(self):
        if self._dir_count:
            print(f"Launching background rmdir for {self._rubbish_dir}")
        else:
            print("Skipping background rmdir. No major directories to remove")

        if os.name == "nt":
            sp.Popen(
                ("cmd.exe", "/c", "rd", "/q/s", str(self._rubbish_dir)),
                stdout=sp.DEVNULL,
                stderr=sp.DEVNULL
                )
            pass
        else:
            if self._rubbish_dir.is_dir() and self._rubbish_dir != "/": # paranoia!
                sp.Popen(
                    ("rm", "-rf", str(self._rubbish_dir)),
                    stdout=sp.DEVNULL,
                    stderr=sp.DEVNULL)

    def _spam(self):
        print(f"Moved {self._dir_count} directories, {self._file_count} files removed", end="\r", flush=True)

    def begin(self):
        rubbish_dir = self._work_dir / ".ushell_clean"
        rubbish_dir.mkdir(parents=True, exist_ok=True)
        self._rubbish_dir = rubbish_dir

        self._dir_count = 0
        self._file_count = 0
        print(f"Moving directories to {self._rubbish_dir.name} and removing unversioned files")

    def remove_dir(self, path):
        dest_name = "%08x_%016x_%s_%s" % (os.getpid(), id(path), path.parent.name, path.name)
        try:
            path.rename(str(self._rubbish_dir / dest_name))
            self._dir_count += 1
            self._spam()
        except OSError as e:
            print("WARNING:", e)

    def remove_file(self, item):
        item.chmod(0o666)
        item.unlink()
        self._file_count += 1
        self._spam()

    def end(self):
        if self._file_count or self._dir_count:
            print()



#-------------------------------------------------------------------------------
class Clean(flow.cmd.Cmd):
    """ Cleans intermediate and temporary files from an Unreal Engine branch. The
    following sub-directories of .uproject, .uplugin, and Engine/ are cleaned up;

      Intermediate     - Removed entirely
      DerivedDataCache - Removed entirely
      Binaries         - Unversioned files are removed
      Saved            - All sub-directories except --savedkeeps=... (see below)

    Only a subset of sub-directories of Saved/ are removed. Any directories that
    match --savedkeeps's comma-separated list are not removed. For example, to
    clean everything excepted Saved/StagedBuilds/ and Saved/Profiling/;

      .p4 clean --savedkeeps=StagedBuilds,Profiling

    "StagedBuilds,Profiling" is the default value for --savedkeeps. If --allsaved
    is given then all of Saved/ will be removed.

    Note that the removal happens in two stages. First directories are moved into
    .ushell_clean/ in the root of the branch. This directory is then removed in
    the background after `.p4 clean` exits."""
    dryrun     = flow.cmd.Opt(False, "Do nothing except reports statistics")
    allsaved   = flow.cmd.Opt(False, "Completely clean Saved/ directories")
    savedkeeps = flow.cmd.Opt("Profiling,StagedBuilds", "Comma-separated list of Saved/ sub-directories to keep")

    def _append_saved(self, collector, saved_dir):
        if self.args.allsaved:
            collector.add_dir(saved_dir)
            return

        for sub_dir in (x for x in saved_dir.glob("*") if x.is_dir()):
            if sub_dir.name.lower() not in self._saved_keeps:
                collector.add_dir(sub_dir)

    def _detect_locked_files(self, root_dir):
        proc = sp.Popen(
            ("wmic.exe", "process", "get", "executablepath"),
            stdout=sp.PIPE, stderr=sp.DEVNULL
        )

        ret = False
        for line in proc.stdout.readlines():
            line = line.strip().decode()
            if line and Path(line).parent.is_relative_to(root_dir):
                ret = line
                break

        proc.stdout.close()
        proc.wait()
        return ret

    def main(self):
        self._saved_keeps = {x.strip().lower() for x in self.args.savedkeeps.split(",")}

        ue_context = unreal.Context(os.getcwd())
        branch = ue_context.get_branch(must_exist=True)
        engine_dir = ue_context.get_engine().get_dir()

        if os.name == "nt":
            self.print_info("Checking running processes")
            if running_exe := self._detect_locked_files(branch.get_dir()):
                raise RuntimeError(f"Not cleaning because '{running_exe}' is running")
                return False

        self.print_info("Finding directories and files to clean")
        root_dirs = [
            engine_dir,
            *(x for x in engine_dir.glob("Programs/*") if x.is_dir()),
        ]

        print("Enumerating...", end="")
        rg_args = (
            "rg",
            "--files",
            "--path-separator=/",
            "--no-ignore",
            "-g*.uplugin",
            "-g*.uproject",
            str(branch.get_dir()),
        )
        rg = sp.Popen(rg_args, stdout=sp.PIPE, stderr=sp.DEVNULL)
        for line in rg.stdout.readlines():
            path = Path(line.decode().rstrip()).parent
            root_dirs.append(path)
        rg.wait()
        print("\r", len(root_dirs), " uproject/uplugin roots found", sep="")

        collector = _Collector()
        clean_handlers = {
            "Intermediate"     : lambda x,y: x.add_dir(y),
            "DerivedDataCache" : lambda x,y: x.add_dir(y),
            "Binaries"         : lambda x,y: x.add_dir(y, remove_carefully=True),
            "Saved"            : self._append_saved,
        }
        for root_dir in root_dirs:
            for dir_name, clean_handler in clean_handlers.items():
                candidate = root_dir / dir_name
                if candidate.is_dir():
                    clean_handler(collector, candidate)

        # Ask Perforce which files shouldn't be deleted
        print("Asking Perforce what's synced...", end="", flush=True)
        specs = (str(x) + "/..." for x in collector.read_careful_dirs())
        p4_have = P4.have(specs)
        for item in p4_have.read(on_error=False):
            path = Path(item.path)
            collector.pin_file(path)
        print("done (", collector.get_pinned_count(), " files)", sep="")

        if self.args.dryrun:
            collector.dispatch(_DryRun())
            return

        self.print_info("Cleaning")
        actuator = _Cleaner(branch.get_dir())
        collector.dispatch(actuator)



#-------------------------------------------------------------------------------
class Reset(flow.cmd.Cmd):
    """ Reconciles a branch to make it match the depot. Use with caution; this is
    a destructive action and involes removing and rewriting files! """
    thorough = flow.cmd.Opt(False, "Compare digests instead of file-modified time")

    def _get_reset_paths(self):
        try:
            ue_context = unreal.Context(os.getcwd())
            if not (branch := ue_context.get_branch()):
                return None
        except EnvironmentError:
            return None

        root_dir = branch.get_dir()
        ret = {
            # root_dir / "*", # to much scope to go wrong here and it's so few files
            root_dir / "Template/...",
            root_dir / "Engine/...",
        }
        for uproj_path in branch.read_projects():
            ret.add(uproj_path.parent.absolute() / "...")

        return ret

    def main(self):
        # Confirm the user wants to really do a reset
        self.print_warning("Destructive action!")
        if self.is_interactive():
            while True:
                c = input("Are you sure you want to continue [yn] ?")
                if c.lower() == "y": break
                if c.lower() == "n": return False

        # Run the reconcile
        args = ("reconcile", "-wade",)
        if not self.args.thorough:
            args = (*args, "--modtime")

        if reset_paths := self._get_reset_paths():
            args = (*args, *(str(x) for x in reset_paths))

        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable("p4", *args)
        return cmd.run()
