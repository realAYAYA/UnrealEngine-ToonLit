# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import time
import flow.cmd
import threading
from peafour import P4
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class _ViewWildcard(object):
    def __init__(self, view, *, allow_empty_wildcards=False, case_sensitive=False):
        view = view.replace("\\", "/")

        if view.endswith("/"):
            view += "\x01"
        elif view.endswith("/..."):
            view = view[:-3] + "\x01"

        rooted = view.startswith("//")

        regex = ""
        sep = ""
        for piece in (x for x in view.split("/") if x):
            regex += sep
            sep = "/"

            if piece == "..." and allow_empty_wildcards:
                regex += "(?:[^/]+/)*"
                sep = ""
                continue

            piece = piece.replace("...", "\x01")
            piece = piece.replace("*", "\x02")
            piece = re.escape(piece)
            piece = piece.replace("\x01", ".*")
            piece = piece.replace("\x02", "[^/]*")
            regex += piece

        regex = "//" + regex if rooted else regex
        regex = (regex or "^") + "$"
        flags = None if case_sensitive else re.IGNORECASE
        self._re = re.compile(regex, flags)

    def test(self, depot_path):
        return self._re.search(depot_path) is not None



#-------------------------------------------------------------------------------
p4_set_result = None
def get_p4_set(prop_name):
    if ret := os.getenv(prop_name):
        return ret

    global p4_set_result
    if not p4_set_result:
        p4_set_result = {}
        try: proc = sp.Popen(("p4", "set", "-q"), stdout=sp.PIPE)
        except: return

        for line in iter(proc.stdout.readline, b""):
            try: key, value = line.split(b"=", 1)
            except ValueError: continue
            p4_set_result[key.decode()] = value.strip().decode()

        proc.wait()
        proc.stdout.close()

    return p4_set_result.get(prop_name, None)

#-------------------------------------------------------------------------------
def login():
    try:
        detail = P4.login(s=True).run()
    except P4.Error:
        raise EnvironmentError("No valid Perforce session found. Run 'p4 login' to authenticate.")
    return getattr(detail, "User", None)

#-------------------------------------------------------------------------------
def get_p4config_name():
    return os.path.basename(get_p4_set("P4CONFIG") or ".p4config.txt")

#-------------------------------------------------------------------------------
def has_p4config(start_dir):
    from pathlib import Path
    p4config = get_p4config_name()
    for dir in (Path(start_dir) / "x").parents:
        candidate = dir / p4config
        if os.path.isfile(candidate):
            return True, candidate
    return False, p4config

#-------------------------------------------------------------------------------
def create_p4config(p4config_path, client, username, port=None):
    with open(p4config_path, "wt") as out:
        print_args = { "sep" : "", "file" : out }
        print("P4CLIENT=", client, **print_args)
        print("P4USER=", username, **print_args)
        if port:
            print("P4PORT", port, **print_args)

#-------------------------------------------------------------------------------
def ensure_p4config(start_dir=None):
    start_dir = start_dir or os.getcwd()
    found, p4config_name = has_p4config(start_dir)
    if found:
        return p4config_name, False

    username = login()

    # Get the client for 'start_dir'
    client = get_client_from_dir(start_dir, username)
    if not client:
        return
    client, root_dir = client

    # Now we know where to locate a p4config file
    p4config_path = f"{root_dir}/{p4config_name}"
    create_p4config(p4config_path, client, username)
    return p4config_path, True

#-------------------------------------------------------------------------------
def get_client_from_dir(root_dir, username):
    import socket
    host_name = socket.gethostname().lower()

    root_dir = Path(root_dir)

    clients = (x for x in P4.clients(u=username) if x.Host.lower() == host_name)
    for client in clients:
        if root_dir.is_relative_to(client.Root):
            return client.client, client.Root

#-------------------------------------------------------------------------------
def get_branch_root(depot_path):
    depot_path = str(depot_path)

    def fstat_paths():
        limit = 5 # ...two of which are always required
        query_path = "//"
        for piece in depot_path[2:].split("/")[:limit]:
            query_path += piece + "/"
            yield query_path + "GenerateProjectFiles.bat"

    print("Probing for well-known file:")
    for x in fstat_paths():
        print(" ", x)

    fstat = P4.fstat(fstat_paths(), T="depotFile")
    root_path = fstat.run(on_error=False)
    if root_path:
        return "/".join(root_path.depotFile.split("/")[:-1]) + "/"

    raise ValueError("Unable to establish branch root")

#-------------------------------------------------------------------------------
def run_p4vc(*args):
    try:
        proc = sp.run(("p4vc", *args))
    except FileNotFoundError:
        proc = sp.run(("p4v", "-p4vc", *args))
    return proc.returncode



#-------------------------------------------------------------------------------
class TempBranchSpec(object):
    def __init__(self, use, username, from_path, to_path, ignore_streams=False):
        import hashlib
        id = hashlib.md5()
        id.update(from_path.encode())
        id.update(to_path.encode())
        id = id.hexdigest()[:6]
        self._name = f"{username}-ushell.{use}-{id}"

        # To map between streams we need to extract the internal branchspec that
        # Perforce builds. If from/to aren't related streams it will fail so we
        # fallback to a conventional trivial branchspec.
        try:
            if ignore_streams:
                raise P4.Error("")

            branch = P4.branch(self._name, o=True, S=from_path[:-1], P=to_path[:-1])
            result = branch.run()
            spec = result.as_dict()
        except P4.Error:
            spec = {
                "Branch" : self._name,
                "View0"  : f"{from_path}... {to_path}...",
            }

        P4.branch(i=True).run(input_data=spec, on_error=False)

    def __del__(self):
        P4.branch(self._name, d=True).run()

    def __str__(self):
        return self._name



#-------------------------------------------------------------------------------
def _kb_string(value):
    return format(value // 1024, ",") + "KB"

#-------------------------------------------------------------------------------
class _SyncRota(object):
    class _Worker(object):
        def __init__(self, id):
            self.id = id
            self.work_items = []
            self.burden = 0
            self.done_size = 0
            self.done_items = 0
            self.error = False

    def __init__(self, changelist, worker_count):
        # An extra worker is used for syncs to #0 (i.e. deletes). When using the
        # -L argument a move will be two path#rev rota items. If they are actioned
        # by the same p4 worker only one item gets synced as "p4 sync" ignores the
        # other. So a move's two path#rev actions are give to two separate workers.
        self._workers = [_SyncRota._Worker(x) for x in range(worker_count + 1)]
        self.changelist = str(changelist)

    def add_work(self, item, rev, cost):
        if rev == 0:
            worker = self._workers[0]
        else:
            worker = min(self._workers[1:], key=lambda x: x.burden)
        worker.work_items.append((item, rev, cost))
        worker.burden += cost

    def sort(self):
        direction = 1
        for worker in self._workers:
            worker.work_items.sort(key=lambda x: x[2] * direction)
            direction *= -1

    def read_workers(self):
        yield from (x for x in self._workers if x.work_items)

#-------------------------------------------------------------------------------
class Syncer(object):
    def __init__(self):
        self._paths = []
        self._excluded_views = set()

    def _read_sync_specs(self, include_excluded=True):
        cl_suffix = "@" + self._rota.changelist
        cl_suffix = "#0" if cl_suffix == "@0" else cl_suffix
        for depot_path in self._paths:
            yield depot_path + cl_suffix

        if include_excluded:
            # Using "@0" results in slow queries it seems
            yield from (x + "#0" for x in self._excluded_views)

    def _is_excluded(self, path):
        return next((True for x in self._view_excludes if x.test(path)), False)

    def _build_exclude_re(self):
        ret = [_ViewWildcard(x, allow_empty_wildcards=True) for x in self._excluded_views]
        self._view_excludes = ret

    def add_path(self, dir):
        self._paths.append(dir)

    def add_exclude(self, view):
        self._excluded_views.add(view)

    def schedule(self, changelist, worker_count=8):
        self._build_exclude_re()

        # P4.<cmd> uses p4's Python-marshalled output (the -G option). However the
        # "p4 sync -n" will report open files via a "info" message instead of a
        # structured "stat" one. So we capture those and fake a reply
        def read_items():
            misreports = []
            def on_info(data):
                if m := re.match(r"\s*(//[^#]+)#(\d+).+is opened and", data.data):
                    misreports.append((m.group(1), m.group(2)))

            yield from P4.sync(self._read_sync_specs(), n=True).read(on_error=False, on_info=on_info)

            class FakeItem(object): pass
            for depot_path, rev in misreports:
                item = FakeItem()
                item.depotFile = depot_path
                item.rev = rev
                item.fileSize = 1 # to save looking this up we'll use a nominal size
                item.action = "edit"
                yield item

        self._rota = _SyncRota(changelist, worker_count)

        # Fill the rota
        total_size = 0
        count = 0
        for item in read_items():
            depot_path = item.depotFile
            rev = int(item.rev)

            if self._is_excluded(depot_path):
                if item.action != "deleted":
                    continue
                rev = 0

            if count % 17 == 0:
                print("\r" + str(count), "files", f"({_kb_string(total_size)})", end="")

            size = int(getattr(item, "fileSize", 0)) # deletes have no size attr

            # When switching some files are metadata only changes and no content
            # is transferred.
            if item.action == "replaced":
                size = 0

            # When syncing backwards files need to be removed. Perforce has been
            # seen reporting these correctly as a delete action but with a revision
            # that is not 0. So we'll just force a rev=0
            if item.action == "deleted" and rev >= 1:
                rev = 0

            self._rota.add_work(depot_path, rev, size)

            total_size += size
            count += 1
        self._rota.sort()
        print("\r" + str(count), "files", f"({_kb_string(total_size)})")

    def sync(self, *, dryrun=False, echo=False):
        # Sum up what we have to do
        total_burden = sum(x.burden for x in self._rota.read_workers())
        total_items = sum(len(x.work_items) for x in self._rota.read_workers())
        lock = threading.Lock()

        print(f"Fetching {_kb_string(total_burden)} in {total_items} files")

        # Launch the worker threads
        def sync_thread(worker):
            def on_error(p4_error):
                if "not enough space" in p4_error.data:
                    worker.error = "Out of disk space"
                    raise EOFError()
                else:
                    with lock:
                        print("sync/error: ", p4_error.data, flush=True)

            try:
                # The 'p4 sync -L' option reportedly does less work on the server
                # so that's what we use here. Note that it only works with "#rev"
                # syntax if one wants to sync a specific revision or changelist.
                def read_sync_items():
                    for path, rev, size in worker.work_items:
                        yield f"{path}#{rev}"

                sync = P4(b=8192).sync(read_sync_items(), n=dryrun, L=True)
                for item in sync.read(on_error=on_error):
                    if echo:
                        with lock:
                            print("sync/worker:", item.depotFile, "#" + str(item.rev), flush=True)

                    size = int(getattr(item, "fileSize", 0))
                    if item.action == "replaced":
                        # meta data update only, no content xfer
                        size = 0

                    worker.done_size += size + 0.01
            except EOFError:
                pass

        def create_thread(worker):
            thread = threading.Thread(target=sync_thread, args=(worker,))
            thread.start()
            return thread

        threads = [create_thread(x) for x in self._rota.read_workers()]
        print(f"Using {len(threads)} workers")

        # While there are active threads, print detail about their progress
        total_burden += (0.01 * total_items)
        while not echo:
            threads = [x for x in threads if x.is_alive()]
            if not threads:
                break

            done_size = sum(x.done_size for x in self._rota.read_workers())
            progress = ((done_size * 1000) // total_burden) / 10
            print("\r%5.1f%%" % progress, _kb_string(int(done_size)), end="");
            time.sleep(0.3)
        else:
            for thread in threads:
                thread.join()
        print("\r...done              ")

        # Check for errors from the workers
        for worker in (x for x in self._rota.read_workers() if x.error):
            print(flow.cmd.text.red("!!" + str(worker.error)))
            return False

        # Nothing more to do if this is a dry run as the remaining tasks need a
        # sync to operate on.
        if dryrun:
            return True

        # P4.sync() returns 'stat' type events but "p4 sync" will report files
        # with a complex sync scenario only as unstructured 'info' messages. As the
        # above won't know about these files we'll do a second sync to catch them.
        global sync_errors
        sync_errors = False
        print("Finalising...", end="")
        def read_depot_files():
            def on_error(data):
                msg = data.data.strip()
                if "up-to-date" in msg: return
                if "not in client view" in msg: return
                if "protected namespace - access denied" in msg: return

                print("\n", flow.cmd.text.red(msg), end="")
                global sync_errors
                sync_errors = True

            p4_is_daft = []
            def on_info(data):
                msg = data.data
                if " - must resolve #" in msg: return
                if " - is opened and not being changed" in msg:
                    try: p4_is_daft.append(msg.split(" - ")[0])
                    except IOError: pass
                    return
                print("\n", flow.cmd.text.light_yellow(data.data), end="")

            sync = P4.sync(self._read_sync_specs(False), n=True)
            for item in sync.read(on_error=on_error, on_info=on_info):
                if not self._is_excluded(item.depotFile):
                    yield item.depotFile + "#" + item.rev

            yield from iter(p4_is_daft)

        count = 0
        sync = P4.sync(read_depot_files(), L=True)
        for item in sync.read(on_error=False):
            if echo:
                with lock:
                    print(f"sync/final: {item.depotFile} #{item.rev}")
            else:
                count += 1
                print("\rFinalising...", count, end="")

        print()

        return not sync_errors
