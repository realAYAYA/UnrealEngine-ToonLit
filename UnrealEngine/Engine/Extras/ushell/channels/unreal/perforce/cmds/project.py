# Copyright Epic Games, Inc. All Rights Reserved.

import os
import p4utils
import flow.cmd

#-------------------------------------------------------------------------------
class Change(flow.cmd.Cmd):
    def main(self):
        # Call the main project command (inheritance happens in describe.flow.py)
        primary_dir = super().main()
        if not primary_dir:
            return False

        self.print_info("Perforce environment")
        try:
            p4config = p4utils.ensure_p4config(primary_dir)
            if p4config:
                path, created = p4config
                print("Status:", "created" if created else "exists")
                print("Path:", path)
            else:
                self.print_warning("Unabled to find a client from", primary_dir)
        except EnvironmentError as e:
            self.print_warning(e)

        return True
