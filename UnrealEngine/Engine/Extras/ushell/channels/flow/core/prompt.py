# Copyright Epic Games, Inc. All Rights Reserved.

import os
import flow.cmd

class Prompt(flow.cmd.Cmd):
    """ Used to collect key/value pairs for tags in the prompt. """
    format = flow.cmd.Opt("none", "Output format. Options; none,sh")

    class Context(object):
        def read_tags(self):
            for k in dir(self):
                v = getattr(self, k)
                if not k.startswith("_") and isinstance(v, str):
                    yield k, v

    def main(self):
        context = Prompt.Context()
        self.prompt(context)

        # Should we format the output in a particular way?
        if self.args.format == "sh":
            print(*(f"{k}='{v}'" for k, v in context.read_tags()), end="")
            return

        # Just print the tags in a friendly manner
        for key, value in context.read_tags():
            print(key, "=", value)

    def prompt(self, context):
        cwd = os.getcwd()
        context.FLOW_CWD = cwd
        context.CWD = cwd
