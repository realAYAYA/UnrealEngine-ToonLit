# Copyright Epic Games, Inc. All Rights Reserved.

import os
import signal
import subprocess

#-------------------------------------------------------------------------------
class Debugger(object):
    def __init__(self, ue_context):
        self._ue_context = ue_context

    def _disable_ctrl_c(self):
        def nop_handler(*args): pass
        self._prev_sigint_handler = signal.signal(signal.SIGINT, nop_handler)

    def _restore_ctrl_c(self):
        signal.signal(signal.SIGINT, self._prev_sigint_handler)

    def debug(self, exec_context, cmd, *args):
        self._disable_ctrl_c()
        subprocess.run(("lldb", cmd, "--", *args))
        self._restore_ctrl_c()

    def attach(self, pid, transport=None, host_ip=None):
        self._disable_ctrl_c()
        subprocess.run(("lldb", "--attach-pid", str(pid)))
        self._restore_ctrl_c()
