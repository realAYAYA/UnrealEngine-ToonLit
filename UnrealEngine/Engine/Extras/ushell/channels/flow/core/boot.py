# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import flow.cmd

#-------------------------------------------------------------------------------
class Boot(flow.cmd.Cmd):
    """ Launches a new session """
    startdir    = flow.cmd.Arg("", "Working directory to start the shell in")
    bootarg     = flow.cmd.Opt("", "(for internal use)")
    cleanprompt = flow.cmd.Opt(False, "Do not modify the host shell's prompt")
    theme       = flow.cmd.Opt("", "Adds a spot of colour with '.ushell theme'")

    def main(self):
        # Clear the screen
        sys.stdout.write("\x1b[2J\x1b[0;0H")

        # Change to the start directory the user requested. Nice and early.
        if self.args.startdir:
            try: os.chdir(self.args.startdir)
            except: pass

        # Register the available shells that subclasses of Boot provide
        class ShellRegistrar(object):
            def __init__(self):       self.shells = {}
            def add(self, name, cls): self.shells[name] = cls
        shell_registrar = ShellRegistrar()
        self.register_shells(shell_registrar)

        channel = self.get_channel()
        system = channel.get_system()
        working_dir = system.get_working_dir()

        # Prepare the environment to boot into.
        exec_context = self.get_exec_context()
        env = exec_context.get_env()

        def read_path_pieces():
            for piece in env["PATH"].split(os.pathsep):
                if "flow" not in piece and "/shims" not in piece:
                    yield piece

        shims_path = os.path.normpath(working_dir) + "/shims"
        env["PATH"] = os.pathsep.join((shims_path, *read_path_pieces()))

        # In case someone wants to run a shim.
        os.environ["PATH"] = env["PATH"]

        # Set a session id and somewhere to store per-session information
        session_id = str(os.getpid())
        env["FLOW_SID"] = session_id
        os.environ["FLOW_SID"] = session_id

        # Get the prompt for the flow shell
        if not self.args.cleanprompt and (prompt := self.get_prompt()):
            env["FLOW_PROMPT"] = prompt

        # Go. This is more of a "set_env()" than a run really
        self.run(env)

        # Find the shell that should be used to hold this session
        shell_name, shell_cookie = (*self.args.bootarg.split(",", 1), "")[:2]
        try:
            shell_class = shell_registrar.shells[shell_name]
        except KeyError:
            raise RuntimeError(f"Unable to find a shell named '{shell_name}'")

        # Run the shell
        system = channel.get_system()
        shell_class(system).boot_shell(env, shell_cookie)

        # Apply a theme
        if self.args.theme:
            import subprocess
            subprocess.run(("_ushell", "theme", "apply", self.args.theme))

    def get_prompt(self):
        pass

    def run(self, env):
        pass

    def register_shells(self, registrar):
        pass
