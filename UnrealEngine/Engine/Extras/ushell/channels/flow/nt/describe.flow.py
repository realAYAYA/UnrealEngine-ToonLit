# Copyright Epic Games, Inc. All Rights Reserved.

import os
import flow.describe

#-------------------------------------------------------------------------------
clink = flow.describe.Tool()
clink.version("1.0.0a4")
clink.payload("https://github.com/mridgers/clink/releases/download/$VERSION/clink-$VERSION.zip")
clink.sha1("6a642bc48e21a3093698067f0fe09eff3ccdfaa5")
clink.platform("win32")
clink.bin("clink_x64.exe")

#-------------------------------------------------------------------------------
fd = flow.describe.Tool()
fd.version("8.7.1")
fd.payload("https://github.com/sharkdp/fd/releases/download/v$VERSION/fd-v$VERSION-x86_64-pc-windows-msvc.zip")
fd.sha1("7e8f5d0a9d1fed75b12483809f9619c04b39b60c")
fd.platform("win32")
fd.bin("fd.exe")
fd.source("https://github.com/sharkdp/fd/releases/latest", "fd-v([0-9.]+)-x86_64")

#-------------------------------------------------------------------------------
fzf_win32 = flow.describe.Tool()
fzf_win32.version("0.44.1")
fzf_win32.payload("https://github.com/junegunn/fzf/releases/download/$VERSION/fzf-$VERSION-windows_amd64.zip")
fzf_win32.sha1("463f064d0403714ab5bf68ebf9fdea4c0159f6c3")
fzf_win32.platform("win32")
fzf_win32.bin("fzf.exe")
fzf_win32.source("https://github.com/junegunn/fzf/releases/latest", r"fzf-(\d+\.\d+\.\d+)-windows")

#-------------------------------------------------------------------------------
shell_cmd = flow.describe.Command()
shell_cmd.source("shell_cmd.py", "Cmd")
shell_cmd.invoke("boot")
shell_cmd.prefix("$")

#-------------------------------------------------------------------------------
shell_pwsh = flow.describe.Command()
shell_pwsh.source("shell_pwsh.py", "Pwsh")
shell_pwsh.invoke("boot")
shell_pwsh.prefix("$")

#-------------------------------------------------------------------------------
channel = flow.describe.Channel()
channel.version("1")
channel.parent("flow.core")
