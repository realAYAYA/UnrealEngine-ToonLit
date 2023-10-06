# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unrealcmd
import ctypes

class Notify(unrealcmd.Cmd):
    """ Flash the console window (if possible) to get the users attention """

    def main(self):
        if os.name == "nt":
            ctypes.windll.user32.FlashWindow(ctypes.windll.kernel32.GetConsoleWindow(), True )
        else:
            print("Notify is not supported by this OS")