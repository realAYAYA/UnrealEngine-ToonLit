# Copyright Epic Games, Inc. All Rights Reserved.

import os
import p4utils
import flow.cmd

#-------------------------------------------------------------------------------
class Boot(flow.cmd.Cmd):
    def run(self, env):
        self.print_info("Perforce environment")
        env["P4IGNORE"] = ".p4ignore.txt"
        env["P4CONFIG"] = p4utils.get_p4config_name()
        if editor := p4utils.get_p4_set("P4EDITOR"):
            env["P4EDITOR"] = editor

        print("P4CONFIG = " + env["P4CONFIG"])
        print("P4IGNORE = " + env["P4IGNORE"])
        print()

        return super().run(env)
