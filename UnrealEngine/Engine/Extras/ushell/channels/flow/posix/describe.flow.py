# Copyright Epic Games, Inc. All Rights Reserved.

import flow.describe

#-------------------------------------------------------------------------------
shells = flow.describe.Command()
shells.source("shells.py", "Shells")
shells.invoke("boot")
shells.prefix("$")

#-------------------------------------------------------------------------------
channel = flow.describe.Channel()
channel.version("1")
channel.parent("flow.core")
