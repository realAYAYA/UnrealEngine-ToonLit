# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import sys
import pprint
import hashlib
import marshal
import flow.cmd
from urllib.request import urlopen, URLError

#-------------------------------------------------------------------------------
def _http_get(url, on_data):
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

    # Get the download's file name
    content_header = client.headers.get("content-disposition", "")
    m = re.search(r'filename="?([^;"]+)"?(;|$)', content_header)
    if m: file_name = m.group(1)
    else: file_name = os.path.basename(client.url)

    # Do the download.
    recv_size = 0
    total_size = int(client.headers.get("content-length"))
    while chunk := client.read(128 << 10):
        recv_size += len(chunk)
        on_data(file_name, recv_size, total_size, chunk)



#-------------------------------------------------------------------------------
class Debug(flow.cmd.Cmd):
    """ Shows debug information about the internal state of the session """

    def main(self):
        channel = self.get_channel()
        system = channel.get_system()

        with open(system.get_working_dir() + "manifest", "rb") as x:
            manifest = marshal.load(x)

        cmd_tree = manifest["cmd_tree"]
        pprint.pprint(manifest)

        print()
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        pprint.pprint(session._get_inner())

#-------------------------------------------------------------------------------
class Invalidate(flow.cmd.Cmd):
    """ Invalidates cached state such that initialisation runs again the next
    time the shell is started up."""

    def main(self):
        from pathlib import Path

        channel = self.get_channel()
        system = channel.get_system()
        working_dir = Path(system.get_working_dir())
        print(working_dir)

        for manifest_path in working_dir.glob("**/manifest"):
            if not manifest_path.parent.samefile(working_dir):
                print("Removing:", manifest_path.relative_to(working_dir))
                manifest_path.unlink(missing_ok=True)

        (working_dir / "manifest").unlink(missing_ok=True)

#-------------------------------------------------------------------------------
class Wipe(flow.cmd.Cmd):
    """ Wipes everything. """

    def main(self):
        import shutil
        channel = self.get_channel()
        system = channel.get_system()
        for item in (system.get_temp_dir(), system.get_working_dir()):
            print("Removing:", item, end="")
            shutil.rmtree(item, ignore_errors=True)
            print("")

        from pathlib import Path
        tool_dir = Path(system.get_tools_dir())
        print("Tools dir:", tool_dir)
        for item in tool_dir.glob("*"):
            if item.is_dir():
                print("Removing tool:", item.name, end="")
                item = item.rename(str(item) + str(os.getpid()))
                shutil.rmtree(item, ignore_errors=True)
                print("...error" if item.is_dir() else "")

#-------------------------------------------------------------------------------
class Paths(flow.cmd.Cmd):
    """ Prints behind-the-scenes paths. """

    def main(self):
        channel = self.get_channel()
        system = channel.get_system()
        print("State:", system.get_working_dir())
        print("Tools:", system.get_tools_dir())
        print(" Temp:", system.get_temp_dir())

#-------------------------------------------------------------------------------
class _Channels(flow.cmd.Cmd):
    def main(self):
        channel = self.get_channel()
        system = channel.get_system()

        self.channels = {}
        channels_dir = system.get_working_dir() + "/channels/"
        with os.scandir(channels_dir) as dirs:
            for item in dirs:
                with open(item.path + "/manifest", "rb") as data:
                    manifest = marshal.load(data)

                self.channels[item.name] = manifest["tools"]

#-------------------------------------------------------------------------------
class Tools(_Channels):
    """ Prints debug information about tools. """

    def main(self):
        super().main()
        pprint.pprint(self.channels)

#-------------------------------------------------------------------------------
class Sha1s(_Channels):
    """ Shows channels' tools SHA1s """

    def _impl(self, name, manifest):
        print("--", name)
        for tool_name, tool in manifest.items():
            def on_data(file_name, recv_size, total_size, data):
                if sys.stdout.isatty():
                    print("%.02f%%" % ((recv_size * 100) / total_size), end="\r")
                digest.update(data)

            digest = hashlib.sha1()
            for urls in tool["bundles"].values():
                for url in urls:
                    try: _http_get(url, on_data)
                    except URLError: pass
            sha1 = digest.hexdigest()

            print(rf'{tool_name}.sha1("{sha1}")')

    def main(self):
        super().main()
        for name, manifest in self.channels.items():
            self._impl(name, manifest)

#-------------------------------------------------------------------------------
class Argumentss(flow.cmd.Cmd):
    """ Prints the given arguments as they would be received by commands """
    arguments = flow.cmd.Arg([str], "Arguments to print")

    def main(self):
        for i, arg in enumerate(self.args.arguments):
            print("%-2d:" % i, arg)
