# Copyright Epic Games, Inc. All Rights Reserved.

import unrealcmd

#-------------------------------------------------------------------------------
class Kill(unrealcmd.Cmd):
    """ Hard-terminates a running process on dev kits or the local machine. """
    what = unrealcmd.Arg(str, "Specified what platform or item to kill")
    wait = unrealcmd.Opt(0.0, "Number of minutes to wait beforehand")

    def complete_what(self, prefix):
        yield "editor"
        yield "server"
        yield "client"
        yield from self.complete_platform(prefix)

    def _kill(self, platform, *args):
        if self.args.wait > 0:
            import time
            until = time.time() + (self.args.wait * 60)
            while True:
                if time.time() >= until:
                    break
                print("Time remaining; %4ds" % max(0, int(until - time.time())), end="\r")
                time.sleep(1.0)
            print()

        platform.kill(*args)

    def main(self):
        what = self.args.what
        try:
            platform = self.get_platform(what)
            self._kill(platform)
            return
        except ValueError:
            pass

        platform = self.get_platform()
        if what == "editor":   self._kill(platform, what)
        elif what == "server": self._kill(platform, what)
        elif what == "client": self._kill(platform, what)
        else: raise ValueError(f"I do not know how to '.kill {what}'")
