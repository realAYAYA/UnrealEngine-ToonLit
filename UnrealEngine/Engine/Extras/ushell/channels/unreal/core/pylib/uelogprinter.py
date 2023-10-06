# Copyright Epic Games, Inc. All Rights Reserved.

import re
import flow.cmd
import prettyprinter

#-------------------------------------------------------------------------------
class Printer(prettyprinter.Printer):
    def __init__(self):
        self._log_re = re.compile(r"(\[.+\]|)Log(\w+):\s+(\w+):\s?")
        self._decorators = (
            flow.cmd.text.green,
            flow.cmd.text.yellow,
            flow.cmd.text.blue,
            flow.cmd.text.magenta,
            flow.cmd.text.cyan,
            flow.cmd.text.light_green,
            flow.cmd.text.light_yellow,
            flow.cmd.text.light_blue,
            flow.cmd.text.light_magenta,
            flow.cmd.text.light_cyan,
        )
        self._decorations = {}

    def _print_log(self, category, verbosity, line):
        if verbosity == "Error":
            line = flow.cmd.text.light_red(line)

        decorator = self._decorations.get(category, None)
        if not decorator:
            hash = 5831
            for c in category:
                hash = ((hash << 5) + hash) ^ ord(c)
            index = hash % len(self._decorators)
            decorator = self._decorators[index]
            self._decorations[category] = decorator

        print(decorator(category), line)

    def _print(self, line):
        m = self._log_re.search(line)
        if m: self._print_log(m.group(2), m.group(3), line[m.end():])
        else: print(line)
