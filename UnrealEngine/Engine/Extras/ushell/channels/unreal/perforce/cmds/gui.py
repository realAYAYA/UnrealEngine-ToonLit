# Copyright Epic Games, Inc. All Rights Reserved.

import os
import p4utils
import flow.cmd
import subprocess
from pathlib import Path

#-------------------------------------------------------------------------------
def _check_for_existing_nt(client):
    client = client.encode()

    wmic = ("wmic.exe", "process", "get", "name,commandline")
    proc = subprocess.Popen(wmic, stdout=subprocess.PIPE)
    for line in iter(proc.stdout.readline, b""):
        if b"p4v.exe" in line and client in line:
            return True
    proc.stdout.close()
    proc.wait()

_check_for_existing = _check_for_existing_nt if os.name == "nt" else lambda x: ""



#-------------------------------------------------------------------------------
class Gui(flow.cmd.Cmd):
    """ Opens the clientspec for the current directory in P4V """
    filename = flow.cmd.Arg(Path(), "Path to select in the workspace/depot tree")
    p4vargs  = flow.cmd.Arg([str], "Additional arguments to pass to P4V")
    def main(self):
        self.print_info("Fetching Perforce info")

        username = p4utils.login()

        cwd = os.getcwd()
        print(f"Finding client for '{cwd}'; ", end="")
        client = p4utils.get_client_from_dir(cwd, username)
        if not client:
            print()
            self.print_error("No client found")
            return False

        client, client_dir = client
        print(client)

        p4_port = p4utils.get_p4_set("P4PORT")
        if not p4_port:
            self.print_error("P4PORT must be set to start P4V")
            return False
        print("Using P4PORT", p4_port)

        self.print_info("Starting P4V")
        if _check_for_existing(client):
            print("P4V with client", client, "is already open")
            return

        print("Client root;", client_dir)

        args = (
            "-p", p4_port,
            "-u", username,
            "-c", client,
            "-t", "pending",
        )

        if self.args.filename.is_file():
            filename = self.args.filename.resolve()
            args = (*args, "-s", str(filename))

        print("p4v", *args)
        subprocess.Popen(("p4v", *args, *self.args.p4vargs), cwd=client_dir)
        print("Done!")
