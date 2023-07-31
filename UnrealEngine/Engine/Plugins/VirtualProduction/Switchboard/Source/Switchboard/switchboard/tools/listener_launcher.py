# Copyright Epic Games, Inc. All Rights Reserved.

from switchboard.config import CONFIG
from .application_launcher import ApplicationLauncher

class ListenerLauncher(ApplicationLauncher):
    ''' Manages launching Switchboard Listener '''

    def __init__(self, name:str = 'SwitchboardListener'):
        super().__init__(name)

    #~Begin ApplicationLauncher interface
    def exe_path(self):
        return CONFIG.listener_path()
    #~End ApplicationLauncher interface
