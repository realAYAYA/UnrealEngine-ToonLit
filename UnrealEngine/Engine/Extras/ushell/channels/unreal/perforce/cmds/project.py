# Copyright Epic Games, Inc. All Rights Reserved.

import os
import p4utils
import flow.cmd
from pathlib import Path

#-------------------------------------------------------------------------------
class Change(flow.cmd.Cmd):
    def main(self):
        # Call the main project command (inheritance happens in describe.flow.py)
        primary_dir = super().main()
        if not primary_dir:
            return False

        # Don't do anything if a .git/ or a previous failure are detected
        for item in Path(primary_dir).parents:
            if (item / ".git").is_dir():            return True
            if (item / ".p4.off.ushell").is_file():   return True
            if (item / "GenerateProjectFiles.bat").is_file():
                primary_dir = item
                break
        else:
            return True

        # Help the user by ensure there is a P4CONFIG file
        self.print_info("Perforce environment")
        try:
            import time
            ts = time.perf_counter()
            p4config = p4utils.ensure_p4config(primary_dir)
            if p4config:
                path, created = p4config
                print("Status:", "created" if created else "exists")
                print("Path:", path)
            else:
                self.print_warning("Unabled to find a client from", primary_dir)
                if time.perf_counter() - ts > 5.0:
                    self.print_warning()
                    self.print_warning("Perforce environment checks by '.project' are now disabled")
                    self.print_warning("Delete '<branch_root>/.p4.off.ushell' to reinstate")
                    with (primary_dir / ".p4.off.ushell").open("wt") as out:
                        out.write(".project failed detecting Perforce env")
        except EnvironmentError as e:
            self.print_warning(e)

        return True
