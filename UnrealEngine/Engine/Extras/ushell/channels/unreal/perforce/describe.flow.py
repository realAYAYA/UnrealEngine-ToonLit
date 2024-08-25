# Copyright Epic Games, Inc. All Rights Reserved.

import flow.describe

#-------------------------------------------------------------------------------
bisect = flow.describe.Command()
bisect.source("cmds/bisect.py", "Bisect")
bisect.invoke("p4", "bisect")

#-------------------------------------------------------------------------------
cherrypick = flow.describe.Command()
cherrypick.source("cmds/cherrypick.py", "Cherrypick")
cherrypick.invoke("p4", "cherrypick")

#-------------------------------------------------------------------------------
clean = flow.describe.Command()
clean.source("cmds/clean.py", "Clean")
clean.invoke("p4", "clean")

#-------------------------------------------------------------------------------
client = flow.describe.Command()
client.source("cmds/workspace.py", "Workspace")
client.invoke("p4", "workspace")

#-------------------------------------------------------------------------------
min_sync = flow.describe.Command()
min_sync.source("cmds/workspace.py", "MinSync")
min_sync.invoke("p4", "sync", "mini")

#-------------------------------------------------------------------------------
gui = flow.describe.Command()
gui.source("cmds/gui.py", "Gui")
gui.invoke("p4", "v")

#-------------------------------------------------------------------------------
mergedown = flow.describe.Command()
mergedown.source("cmds/mergedown.py", "MergeDown")
mergedown.invoke("p4", "mergedown")

#-------------------------------------------------------------------------------
project = flow.describe.Command()
project.source("cmds/project.py", "Change")
project.invoke("project")

#-------------------------------------------------------------------------------
reset = flow.describe.Command()
reset.source("cmds/clean.py", "Reset")
reset.invoke("p4", "reset")

#-------------------------------------------------------------------------------
switch = flow.describe.Command()
switch.source("cmds/switch.py", "Switch")
switch.invoke("p4", "switch")

#-------------------------------------------------------------------------------
switch_list = flow.describe.Command()
switch_list.source("cmds/switch.py", "List")
switch_list.invoke("p4", "switch", "list")

#-------------------------------------------------------------------------------
sync = flow.describe.Command()
sync.source("cmds/sync.py", "Sync")
sync.invoke("p4", "sync")

sync_edit = flow.describe.Command()
sync_edit.source("cmds/sync.py", "Edit")
sync_edit.invoke("p4", "sync", "edit")

#-------------------------------------------------------------------------------
who = flow.describe.Command()
who.source("cmds/who.py", "Who")
who.invoke("p4", "who")

#-------------------------------------------------------------------------------
boot = flow.describe.Command()
boot.source("boot.py", "Boot")
boot.invoke("boot")
boot.prefix("$")

#-------------------------------------------------------------------------------
tips = flow.describe.Command()
tips.source("tips.py", "Tips")
tips.invoke("tip")
tips.prefix("$")



#-------------------------------------------------------------------------------
channel = flow.describe.Channel()
channel.version("0")
channel.parent("unreal.core")
