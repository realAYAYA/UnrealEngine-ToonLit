# Copyright Epic Games, Inc. All Rights Reserved.

import os
from ..linux import linux

#-------------------------------------------------------------------------------
class Platform(linux.PlatformBase):
    def _read_env(self):
        env_var = linux.PlatformBase.env_var
        value = os.getenv(env_var)
        if not value:
            version = self.get_version()
            value = f"Linux_x64/{version}/"
        yield env_var, value
