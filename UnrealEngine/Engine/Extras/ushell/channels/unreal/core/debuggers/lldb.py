# Copyright Epic Games, Inc. All Rights Reserved.

import os
import signal
import unreal
import subprocess

#-------------------------------------------------------------------------------
class Debugger(unreal.Debugger):
    name = "lldb"

    def _disable_ctrl_c(self):
        def nop_handler(*args): pass
        self._prev_sigint_handler = signal.signal(signal.SIGINT, nop_handler)

    def _restore_ctrl_c(self):
        signal.signal(signal.SIGINT, self._prev_sigint_handler)

    def _debug(self, exec_context, cmd, *args):
        self._disable_ctrl_c()
        subprocess.run(("lldb", cmd, "--", *args))
        self._restore_ctrl_c()

    def _attach(self, pid, transport=None, host_ip=None):
        self._disable_ctrl_c()
        subprocess.run(("lldb", "--attach-pid", str(pid)))
        self._restore_ctrl_c()
