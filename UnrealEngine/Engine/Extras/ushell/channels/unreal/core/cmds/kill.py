# Copyright Epic Games, Inc. All Rights Reserved.

import unrealcmd

#-------------------------------------------------------------------------------
class Kill(unrealcmd.Cmd):
    """ Hard-terminates a running process on dev kits or the local machine. """
    what = unrealcmd.Arg(str, "Specified what platform or item to kill")

    def complete_what(self, prefix):
        yield "editor"
        yield "server"
        yield "client"
        yield from self.complete_platform(prefix)

    def main(self):
        what = self.args.what
        try:
            platform = self.get_platform(what)
            platform.kill()
            return
        except ValueError:
            pass

        platform = self.get_platform()
        if what == "editor":   platform.kill(what)
        elif what == "server": platform.kill(what)
        elif what == "client": platform.kill(what)
        else: raise ValueError(f"I do not know how to '.kill {what}'")
