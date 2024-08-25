# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import fsutils
import hashlib
import marshal
import flow.describe
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class _Log(object):
    def __init__(self, log_path):
        self._indent = 0
        try: self._log_file = open(log_path, "wt")
        except: self._log_file = None

    def __del__(self):
        if self._log_file:
            self._log_file.close()

    def indent(self, *name):
        self.write("Section:", *name)
        self._indent += 1

    def unindent(self):
        self._indent -= 1

    def write(self, *args):
        if self._log_file:
            print(*args, file=self._log_file)

    def print(self, *args, **kwargs):
        print("  " * (self._indent * (1 if kwargs.get("end") == None else 0)), end="")
        print(*args, **kwargs, file=sys.stdout)

    def print_write(self, *args, **kwargs):
        self.print(*args, **kwargs)
        self.write(*args)

_log = None



#-------------------------------------------------------------------------------
def _http_get(url, dest_dir, progress_cb=None):
    import re
    from urllib.request import urlopen

    # Try and get the certifi CA file
    try:
        import certifi
        cafile_path = certifi.where()
    except ImportError:
        cafile_path = None

    # Fire up an http client to get the url.
    client = urlopen(url, cafile=cafile_path)
    if (client.status // 100) != 2:
        assert False, f"Error creating HTTP client for {url}"

    if progress_cb:
        progress_cb("", 0.0)

    # Get the download's file name
    content_header = client.headers.get("content-disposition", "")
    m = re.search(r'filename="?([^;"]+)"?(;|$)', content_header)
    if m: file_name = m.group(1)
    else: file_name = os.path.basename(client.url)
    dest_path = dest_dir + file_name
    assert not os.path.exists(dest_path), f"Directory '{dest_path}' unexpectedly exists"

    # Do the download. If there's no content-length we are probably receiving
    # chunked transfer-encoding. So we'll cheat.
    chunk_size = 128 << 10
    if chunk_count := client.headers.get("content-length"):
        chunk_count = int(chunk_count)
    else:
        chunk_count = 1 << 48;
    chunk_count = (chunk_count + chunk_size - 1) // chunk_size
    chunk_index = 0
    with open(dest_path, "wb") as out:
        while chunk := client.read(chunk_size):
            out.write(chunk)
            out.flush()
            chunk_index += 1
            if progress_cb:
                progress_cb(file_name, chunk_index / chunk_count)

    return dest_path



#-------------------------------------------------------------------------------
def _extract_tar(payload, dest_dir):
    cmd = ("tar", "-x", "-C", dest_dir, "-f", payload)
    ret = sp.run(cmd, stdout=sp.DEVNULL)
    assert ret.returncode == 0, "Extract failed; " + " ".join(cmd)

#-------------------------------------------------------------------------------
def _tool_extract_method(self, src_path, dest_dir):
    extractors = {
        ".tar.gz"   : _extract_tar,
        ".tgz"      : _extract_tar,
        ".zip"      : _extract_tar, # Windows comes with tar as standard now
    }

    was_archive = False
    for file_name in (x.lower() for x in os.listdir(src_path)):
        for suffix, extract_func in extractors.items():
            if not file_name.endswith(suffix):
                continue

            _log.write("extracting", file_name, "to", dest_dir, "with", extract_func)
            extract_func(src_path + file_name, dest_dir)
            was_archive = True
            break

    if was_archive:
        return

    for file_name in os.listdir(src_path):
        os.rename(src_path + file_name, dest_dir + file_name)

flow.describe.Tool.extract = _tool_extract_method



#-------------------------------------------------------------------------------
def _validate_tool(name, tool):
    assert tool._version, f"No version set on tool '{name}'"
    assert tool._bundles, f"No payload set on tool '{name}'"

#-------------------------------------------------------------------------------
def _acquire_tool(name, tool, target_dir, progress_cb):
    # Some boiler plate to provide a progress update
    payload_i = 0
    payload_n = sum(len(x._payloads) for x in tool._bundles.values())
    def inner_progress_cb(file_name, value):
        value = int(((float(payload_i) + value) * 100) / payload_n)
        progress_cb(f"{value:3}%")

    # Acquire the tool's bundles/payloads
    temp_dir = fsutils.WorkPath(target_dir[:-1] + ".work/")
    for bundle_name, bundle in tool._bundles.items():
        _log.write("Bundle", bundle_name)
        fetch_dir = temp_dir + bundle_name + "/"
        os.mkdir(fetch_dir)
        for payload, ftype in bundle._payloads:
            payload = payload.replace("$VERSION", tool._version)
            _log.write("Payload", payload, ftype)
            payload_dest = _http_get(payload, fetch_dir, inner_progress_cb)
            payload_i += 1
            if ftype:
                os.rename(payload_dest, payload_dest + "." + ftype)

    # Hash the downloaded content to confirm it is what is expected
    progress_cb("sha1")
    sha1 = hashlib.sha1()
    for bundle_name, bundle in tool._bundles.items():
        for item in fsutils.read_files(temp_dir + bundle_name):
            with open(item.path, "rb") as in_file:
                while True:
                    data = in_file.read(16384)
                    if len(data) <= 0:
                        break
                    sha1.update(data)
    tool_sha1 = sha1.hexdigest()
    _log.write("SHA1", name, tool_sha1)

    if tool._sha1:
        expected_sha1 = tool._sha1.lower()
        if tool_sha1 != expected_sha1:
            assert false, f"Unexpected content bundle data for tool '{name}' [sha1:{tool_sha1}]"

    # Extract the payloads.
    progress_cb("uzip")
    extract_dir = temp_dir + "$extract/"
    os.mkdir(extract_dir)
    for bundle_name, bundle in tool._bundles.items():
        _log.write("Extract", bundle_name, "from", fetch_dir, "to", extract_dir, "with", tool.extract)
        fetch_dir = temp_dir + bundle_name + "/"
        tool.extract(fetch_dir, extract_dir)

    # Collapse
    source_dir = str(extract_dir)
    while True:
        dir_iter = iter(fsutils.read_files(source_dir))
        x = next(dir_iter, None)
        if x and not next(dir_iter, None) and x.is_dir():
            source_dir = x.path
            continue
        break

    if tool._root_dir:
        source_dir += "/" + tool._root_dir
        assert os.path.isdir(source_dir), "Given root f'{tool._root_dir}' for tool '{name}' is not valid"

    # Store sha1
    with open(source_dir + "/sha1.flow.txt", "w") as out:
        out.write(tool_sha1)

    os.rename(source_dir, target_dir)

    progress_cb("done")
    return tool_sha1

#-------------------------------------------------------------------------------
def _manifest_tool(name, tool):
    bundles = {}
    for bundle_name, bundle in tool._bundles.items():
        urls = (x[0] for x in bundle._payloads)
        bundles[bundle_name] = tuple(x.replace("$VERSION", tool._version) for x in urls)

    return {
        "sha1"      : 0,
        "version"   : tool._version,
        "double"    : f"{name}-{tool._version}",
        "bin_paths" : tuple(tool._bin_paths),
        "bundles"   : bundles,
        "source"    : tool._source,
    }

#-------------------------------------------------------------------------------
def _install_tool(name, tool, channel_dir):
    manifest = _manifest_tool(name, tool)
    if tool._platform and tool._platform != sys.platform:
        return manifest

    tool_double = f"{name}-{tool._version}/"
    _log.write("Installing tool", tool_double)

    log_line = (13 * " ") + tool_double[:-1]
    log_line += "\b" * (len(log_line) - 1)
    _log.print(log_line, end="\r      [    ]\b")

    # If the tool doesn't exist then it needs acquiring
    tool_dir = "../tools/" + tool_double
    if not os.path.isdir(tool_dir):
        def progress_cb(info):
            _log.print("\b" * 4, info[:4], sep="", end="")

        try:
            _log.write("Acquiring to", tool_dir)
            sha1 = _acquire_tool(name, tool, tool_dir, progress_cb)
        except KeyboardInterrupt:
            raise
        except Exception as e:
            _log.write("ERROR", str(e))
            _log.print("\b" * 4, "fail", sep="", end="\n")
            return manifest

        # Post install
        if hasattr(tool, "post_install"):
            progress_cb("post")
            _log.write("Post-install")
            tool.post_install(tool_dir)

        # Validate that the required binaries exist.
        for bin_path in tool._bin_paths:
            if not os.path.exists(tool_dir + bin_path):
                _log.write(f"Unable to find '{bin_path}' in '{tool_dir}'")
    else:
        sha1 = 1
        if os.path.isfile(tool_dir + "/sha1.flow.txt"):
            with open(tool_dir + "/sha1.flow.txt", "rt") as sha1_file:
                sha1 = sha1_file.read()
        _log.write("Tool already acquired", tool_dir)

    manifest["sha1"] = sha1
    _log.print("\b" * 4, " ok ", sep="", end="\n")
    return manifest



#-------------------------------------------------------------------------------
class _Action(object):
    def __init__(self, name, input_path):
        self._name = name
        self._input_path = input_path

    def needs_update(self):
        return True



#-------------------------------------------------------------------------------
def _validate_command(channel_src_dir, name, command):
    assert command._path, f"Command '{name}' missing call to invoke/composite()"
    assert command._py_path, f"Command '{name}' missing call to source()"
    if not os.path.isfile(channel_src_dir + command._py_path):
        assert False, f"Command '{name}' missing source '{command._py_path}'"

#-------------------------------------------------------------------------------
def _select_update_type():
    version_path = os.path.abspath(__file__ + "/../../version")
    input_deps = (
        version_path,             # primary version for schema/state invalidation
        "../python/current/",     # embedded version of Python.
    )
    mtime = max(fsutils.get_mtime(x) for x in input_deps)
    return _UpdateMaybe if fsutils.get_mtime("manifest") > mtime else _UpdateImpl

#-------------------------------------------------------------------------------
class _UpdateImpl(_Action):
    def run(self):
        _log.print(f"Updating channel '{self._name}'")
        _log.indent("Channel", self._name)

        # Clean up the output. Updates are an install from scratch.
        out_dir = f"channels/{self._name}/"
        fsutils.rmdir(out_dir)
        out_dir = fsutils.WorkPath(out_dir)

        # Load the describe.flow.py as a module.
        channel_py = self._input_path + "describe.flow.py"
        module = fsutils.import_script(channel_py)

        def read_items_by_type(type):
            for name, value in module.__dict__.items():
                if isinstance(value, type):
                    yield name, value

        # Get the channel's description.
        name, channel = next(read_items_by_type(flow.describe.Channel), (None, None))
        assert channel, f"Channel() instance missing from channel '{self._name}'"
        assert channel._version != None, f"Channel '{self._name}' has no version"

        # Install channel's pips.
        if channel._pips:
            _log.print("Pips:")
            _log.indent("Pips")
            for pip_name in channel._pips:
                _log.print_write(pip_name)
                cmd = (sys.executable, "-Xutf8", "-Esum", "pip", "install", pip_name)
                result = sp.run(cmd, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
                if result.returncode:
                    _log.print(" ...failed");
            _log.unindent()

        # Tools
        tools_manifest = {}
        tools = [x for x in read_items_by_type(flow.describe.Tool)]
        if tools:
            _log.print("Tools:")
            _log.indent("Tools")
            for name, tool in tools:
                _validate_tool(name, tool)

            for name, tool in tools:
                tools_manifest[name] = _install_tool(name, tool, out_dir)
            _log.unindent()

        # Commands
        commands = [x for x in read_items_by_type(flow.describe.Command)]
        for name, command in commands:
            _validate_command(self._input_path, name, command)
            command._path = command._path or (name)
            command._path = (command._prefix, *command._path)

        # Build manifest
        manifest = {}
        manifest["name"] = self._name
        manifest["path"] = self._input_path
        manifest["parent"] = channel._parent or "flow.core"
        manifest["tools"] = tools_manifest
        manifest["commands"] = tuple(v.__dict__ for k,v in commands)

        # Success. Write the action's output
        with open(out_dir + "manifest", "wb") as x:
            marshal.dump(manifest, x)

        out_dir.keep()
        _log.print("Done\n")
        _log.unindent()

#-------------------------------------------------------------------------------
class _UpdateMaybe(_UpdateImpl):
    def needs_update(self):
        in_mtime = fsutils.get_mtime(self._input_path + "describe.flow.py")
        out_mtime = fsutils.get_mtime(f"channels/{self._name}/manifest")
        return out_mtime <= in_mtime



#-------------------------------------------------------------------------------
def _plant_cmd_tree(channels):
    # Build the command tree
    cmd_tree = {}
    for channel in channels:
        for cmd in channel["commands"]:
            node = cmd_tree
            prefix, *cmd_path = cmd["_path"]
            for piece in (prefix + cmd_path[0], *cmd_path[1:]):
                node = node.setdefault(piece, {})

            cmd_desc = (
                channel["index"],
                cmd["_py_path"],
                cmd["_py_class"],
            )

            leaf = node.setdefault(493, [channel["index"]])
            leaf.insert(1, cmd_desc)

    return cmd_tree

#-------------------------------------------------------------------------------
def _carve_channels(channels):
    out = [None] * len(channels)
    for channel in channels:
        index = channel["index"]
        parent_index = channel["parent"]["index"] if channel["parent"] else -1
        tools = {k:(v["version"], v["bin_paths"]) for k,v in channel["tools"].items() if v["sha1"]}
        out[channel["index"]] = {
            "name" : channel["name"],
            "path" : channel["path"],
            "parent" : parent_index,
            "tools" : tools,
        }

    return out

#-------------------------------------------------------------------------------
def _create_shims(manifest):
    shims_dir = "shims/"
    _log.write("Shims dir;", shims_dir)

    import shims
    builder = shims.Builder(shims_dir)
    builder.clean()

    # Create command shims
    run_py = os.path.abspath(__file__ + "/../../run.py")
    manifest_path = os.path.abspath("manifest")
    args_prefix = ("-Xutf8", "-Esu", run_py, manifest_path)
    for name in (x for x in manifest["cmd_tree"] if isinstance(x, str)):
        builder.create_python_shim(name, *args_prefix, name)

    # Create tool shims
    tool_root = os.path.abspath("../tools") + "/"
    for channel in reversed(manifest["channels"]):
        for name, (version, bin_paths) in channel["tools"].items():
            tool_dir = tool_root + f"{name}-{version}/"
            for bin_path in bin_paths:
                name = os.path.basename(bin_path)
                tool_path = tool_dir + bin_path
                if os.path.isfile(tool_path):
                    builder.create_shim(name, tool_path)
                else:
                    _log.write("Binary not found;", tool_path)

    builder.commit()

#-------------------------------------------------------------------------------
class _Finalise(_Action):
    def needs_update(self):
        self._manifest_paths = [x.path + "/manifest" for x in fsutils.read_dirs(self._input_path)]
        mtime = max(fsutils.get_mtime(x) for x in self._manifest_paths)
        mtime = max(mtime, fsutils.get_mtime(self._input_path))
        return fsutils.get_mtime("manifest") <= mtime

    def run(self):
        # Load channel manifests
        manifests = []
        for manifest_path in self._manifest_paths:
            with open(manifest_path, "rb") as x:
                manifest = marshal.load(x)
                manifests.append(manifest)

        # Assign each manifest an index
        for i in range(len(manifests)):
            manifests[i]["index"] = i

        # Connect children to their parents
        manifests = {x["name"]:x for x in manifests}
        for manifest in manifests.values():
            parent = manifest["parent"]
            assert parent in manifests, f"Invalid parent '{parent}' for channel '{manifest['name']}'"
            manifest["parent"] = manifests[parent] if parent != manifest["name"] else None

        # Sort the manifests topologically
        topo_manifests = {}

        def topo_insert(manifest):
            if manifest:
                topo_insert(manifest["parent"])
                topo_manifests[manifest["name"]] = manifest

        for manifest in manifests.values():
            topo_insert(manifest)

        manifests = topo_manifests
        _log.write("Channels;", *(x["name"] for x in manifests.values()))

        # Collect various paths for each channel
        pylib_dirs = []
        for manifest in manifests.values():
            pylib_dir = manifest["path"] + "pylib/"
            if os.path.isdir(pylib_dir):
                pylib_dirs.append(pylib_dir)
        _log.write("Pylib dirs;", *(x for x in pylib_dirs))

        # Build the primary manifest.
        primary = {
            "channels"   : _carve_channels(manifests.values()),
            "pylib_dirs" : tuple(pylib_dirs),
            "tools_dir"  : os.path.abspath("../tools") + "/",
            "cmd_tree"   : _plant_cmd_tree(manifests.values()),
        }

        # Create shims
        _create_shims(primary)

        # Write the primary manifest out.
        with open("manifest", "wb") as x:
            marshal.dump(primary, x)



#-------------------------------------------------------------------------------
class _Prune(_Action):
    def run(self):
        _log.print_write(f"Removing old channel '{self._name}'")
        fsutils.rmdir(self._input_path)



#-------------------------------------------------------------------------------
def impl(working_dir:Path, *channels_dirs:Path):
    channels_dirs = [str(x.resolve()) for x in channels_dirs if x.is_dir()]
    working_dir = str(working_dir.resolve())

    def _read_channels(dir, depth=0):
        for item in fsutils.read_dirs(dir):
            if os.path.isfile(item.path + "/describe.flow.py"):
                yield item.name, os.path.abspath(item.path) + "/"
            elif depth == 0:
                for name, path in _read_channels(item.path, depth + 1):
                    yield f"{item.name}.{name}", path

    # Seed the working dir with a hash of our location so we can detect movement
    loc_hash = 5381
    for c in os.path.abspath(__file__):
        loc_hash = ((loc_hash << 5) + loc_hash) + (ord(c) | 0x20)
        loc_hash &= 0x7fffffff
    working_suffix = "flow_%08x" % loc_hash

    # Change to the main working dir
    working_dir += f"/{working_suffix}/"
    try: os.makedirs(working_dir)
    except FileExistsError: pass
    boot_dir = os.getcwd()
    os.chdir(working_dir)

    lock = fsutils.DirLock("..")

    # Build a list of actions to do. The flow channel must be first
    actions = {"flow.core" : None}

    # First; add existing channels for removal
    for dest_dir in fsutils.read_dirs("channels"):
        actions[dest_dir.name] = _Prune(dest_dir.name, dest_dir.path + "/")

    # Second; add channels from source to get updated.
    update_type = _select_update_type()
    for channels_dir in channels_dirs:
        for name, path in _read_channels(channels_dir):
            actions[name] = update_type(name, path)

    # Lastly; aggregate channels into a manifest and create command shims
    actions["*finalise"] = _Finalise("*finalise", "channels/")

    # If there's something to do, do it or wait for it to be done
    for action in (x for x in actions.values() if x.needs_update()):
        if lock.claim():
            global _log
            if not _log:
                _log = _Log(working_dir + "setup.log")
                _log.write("Working dir;", working_dir)

            try:
                _log.write("-- Running action;", type(action).__name__)
                action.run()
            except:
                import traceback
                _log.write(traceback.format_exc())
                raise
            continue

        import time
        print("Waiting for another instance to provision.", end="")
        while not lock.can_claim():
            time.sleep(1)
            print(".", end="")

        break

    os.chdir(boot_dir)
    return working_suffix
