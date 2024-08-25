# Copyright Epic Games, Inc. All Rights Reserved.

import os
import subprocess
from pathlib import Path

#-------------------------------------------------------------------------------
class Debugger(object):
    def __init__(self, ue_context):
        self._ue_context = ue_context

    def debug(self, exec_context, cmd, *args):
        cmd = exec_context.create_runnable(cmd, *args)
        cmd.launch(suspended=True, new_term=True)
        pid = cmd.get_pid()

        try:
            if not self.attach(pid):
                cmd.kill()
        except:
            cmd.kill()
            raise

    def attach(self, pid, transport=None, host_ip=None):
        engine = self._ue_context.get_engine()
        engine_dir = engine.get_dir()

        import vs.dte
        for instance in vs.dte.running():
            candidate = instance.get_sln_path()
            if candidate and engine_dir.is_relative_to(Path(candidate).parent):
                if instance.attach(pid, transport, host_ip):
                    break
        else:
            # There's nothing to fallback if a fancy debugger is supposed to be used
            if transport:
                return False

            ret = subprocess.run(("vsjitdebugger.exe", "-p", str(pid)))
            if ret.returncode:
                return False

        if not transport:
            vs.dte.resume_process(pid)

        return True
