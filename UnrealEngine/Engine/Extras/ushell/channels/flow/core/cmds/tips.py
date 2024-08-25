# Copyright Epic Games, Inc. All Rights Reserved.

import random
import flow.cmd

#-------------------------------------------------------------------------------
class Tips(flow.cmd.Cmd):
    """ Displays a useful tip """

    @classmethod
    def read_get_tip_methods(self):
        for component in self.__mro__:
            if method := getattr(component, "get_tips", None):
                yield method

    def main(self):
        tips = []
        for get_tips in self.read_get_tip_methods():
            tips += get_tips(self)

        with flow.cmd.text.light_cyan:
            print("TIP!", end="")

        with flow.cmd.text.white:
            print("", tips[random.randrange(len(tips))])

    def get_tips(self):
        return (
            "Press Tab for context sensitive completion",
            "List command history using 'history' and use ![NUM] to recall.",
            "Incrementally search your command history with Ctrl-R (or Ctrl-S)",
            "'.[command] --help' to see a commands inline help",
            "Run '.help' to see all available commands",
            "Calvin and Hobbes is excellent",
            "Customise a session with '$USERPROFILE/.ushell/hooks/startup.bat'",
            "'d:\\>dc' changes directory using the fuzzy finder",
            "'d:\\>dc.' limits fuzzy-finder-chdir to the current directory",
            "'d:\\>dc DIR' limits fuzzy-finder-chdir to directory 'DIR'",
            "Use Ctrl-P/Ctrl-N to recall previous queries to 'qq' and 'dc'",
            "'qq' to fuzzy find and execute lines from the command history",
            "'.ushell theme' for setting and exploring terminal themes",
        )
