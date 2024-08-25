# Copyright Epic Games, Inc. All Rights Reserved.

import os
import flow.describe

#-------------------------------------------------------------------------------
help_cmd = flow.describe.Command()
help_cmd.source("cmds/help.py", "Help")
help_cmd.invoke("help")

help_readme = flow.describe.Command()
help_readme.source("cmds/help.py", "ReadMe")
help_readme.invoke("help", "readme")

#-------------------------------------------------------------------------------
tip_cmd = flow.describe.Command()
tip_cmd.source("cmds/tips.py", "Tips")
tip_cmd.invoke("tip")
tip_cmd.prefix("$")

#-------------------------------------------------------------------------------
debug_manifest = flow.describe.Command()
debug_manifest.source("cmds/debug.py", "Debug")
debug_manifest.invoke("debug", "manifest")
debug_manifest.prefix("$")

debug_tools = flow.describe.Command()
debug_tools.source("cmds/debug.py", "Tools")
debug_tools.invoke("debug", "tools")
debug_tools.prefix("$")

debug_sha1s = flow.describe.Command()
debug_sha1s.source("cmds/debug.py", "Sha1s")
debug_sha1s.invoke("debug", "tools", "sha1s")
debug_sha1s.prefix("$")

debug_args = flow.describe.Command()
debug_args.source("cmds/debug.py", "Argumentss")
debug_args.invoke("debug", "args")
debug_args.prefix("$")

debug_invalidate = flow.describe.Command()
debug_invalidate.source("cmds/debug.py", "Invalidate")
debug_invalidate.invoke("debug", "invalidate")
debug_invalidate.prefix("$")

debug_paths = flow.describe.Command()
debug_paths.source("cmds/debug.py", "Paths")
debug_paths.invoke("debug", "paths")
debug_paths.prefix("$")

debug_wipe = flow.describe.Command()
debug_wipe.source("cmds/debug.py", "Wipe")
debug_wipe.invoke("debug", "wipe")
debug_wipe.prefix("$")

#-------------------------------------------------------------------------------
theme_set = flow.describe.Command()
theme_set.source("cmds/theme.py", "Set")
theme_set.invoke("ushell", "theme", "apply")

theme_random = flow.describe.Command()
theme_random.source("cmds/theme.py", "Random")
theme_random.invoke("ushell", "theme", "random")

theme_showcase = flow.describe.Command()
theme_showcase.source("cmds/theme.py", "Showcase")
theme_showcase.invoke("ushell", "theme", "showcase")

#-------------------------------------------------------------------------------
complete = flow.describe.Command()
complete.source("cmds/complete.py", "Complete")
complete.invoke("complete")
complete.prefix("$")

#-------------------------------------------------------------------------------
prompt = flow.describe.Command()
prompt.source("prompt.py", "Prompt")
prompt.invoke("prompt")
prompt.prefix("$")

#-------------------------------------------------------------------------------
boot = flow.describe.Command()
boot.source("boot.py", "Boot")
boot.invoke("boot")
boot.prefix("$")



#-------------------------------------------------------------------------------
channel = flow.describe.Channel()
channel.version("1")
