# Copyright Epic Games, Inc. All Rights Reserved.

import flow.cmd
import subprocess

#-------------------------------------------------------------------------------
class Boot(flow.cmd.Cmd):
    project = flow.cmd.Opt("", "The active project/branch. See '.project --help'")

    def get_prompt(self):
        return "\x1b[93m:UE_BRANCH:\x1b[90m|\x1b[36m:UE_PROJECT:\x1b[90m / \x1b[96m:FLOW_CWD:\n\x1b[97m-> \x1b[0m"

    def run(self, env):
        try:
            proc = subprocess.run(
                ("_project", self.args.project or "auto"),
                stdin=subprocess.DEVNULL
            )
            if proc.returncode:
                self.print_error()
                self.print_error("Set a shortcut's 'Start In' to a folder with a valid .uproject")
                self.print_error("path or launch with the 'ushell --project=[uprojpath]' argument.")
                self.print_error()
        except Exception as e:
            self.print_error(e)
        print()

        env["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"

        return super().run(env)
