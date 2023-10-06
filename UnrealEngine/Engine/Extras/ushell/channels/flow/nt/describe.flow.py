# Copyright Epic Games, Inc. All Rights Reserved.

import os
import flow.describe

#-------------------------------------------------------------------------------
clink = flow.describe.Tool()
clink.version("1.0.0a4")
clink.platform("win32")
clink.payload("https://github.com/mridgers/clink/releases/download/1.0.0a4/clink-1.0.0a4.zip")
clink.bin("clink_x64.exe")
clink.sha1("6a642bc48e21a3093698067f0fe09eff3ccdfaa5")

#-------------------------------------------------------------------------------
fd = flow.describe.Tool()
fd.version("8.4.0")
fd.platform("win32")
fd.payload("https://github.com/sharkdp/fd/releases/download/v8.4.0/fd-v8.4.0-x86_64-pc-windows-msvc.zip")
fd.bin("fd.exe")
fd.sha1("aef6f9809b7d86705fe828689453aae61f71365f")
fd.source("https://github.com/sharkdp/fd/releases/latest", "fd-v([0-9.]+)-x86_64")

#-------------------------------------------------------------------------------
fzf_win32 = flow.describe.Tool()
fzf_win32.version("0.33.0")
fzf_win32.platform("win32")
fzf_win32.payload("https://github.com/junegunn/fzf/releases/download/0.33.0/fzf-0.33.0-windows_amd64.zip")
fzf_win32.bin("fzf.exe")
fzf_win32.sha1("be7fc7b271069f7da5c27d570c81287d23cd328e")
fzf_win32.source("https://github.com/junegunn/fzf/releases/latest", "fzf-(\d+\.\d+\.\d+)-windows")

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
