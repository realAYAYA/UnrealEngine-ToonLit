# Copyright Epic Games, Inc. All Rights Reserved.

import sys
import flow.describe

#-------------------------------------------------------------------------------
fzf_linux = flow.describe.Tool()
fzf_linux.version("0.33.0")
fzf_linux.platform("linux")
fzf_linux.payload("https://github.com/junegunn/fzf/releases/download/0.33.0/fzf-0.33.0-linux_amd64.tar.gz")
fzf_linux.bin("fzf")
fzf_linux.sha1("41aeb1c7bc01c700614768a777c5df91919a180c")

fzf_darwin = flow.describe.Tool()
fzf_darwin.version("0.33.0")
fzf_darwin.platform("darwin")
fzf_darwin.payload("https://github.com/junegunn/fzf/releases/download/0.33.0/fzf-0.33.0-darwin_amd64.zip")
fzf_darwin.bin("fzf")
fzf_darwin.sha1("f83505967ca3d0ad849a461029d60cd414e3ed76")



#-------------------------------------------------------------------------------
ripgrep_win32 = flow.describe.Tool()
ripgrep_win32.version("13.0.0")
ripgrep_win32.platform("win32")
ripgrep_win32.payload("https://github.com/BurntSushi/ripgrep/releases/download/13.0.0/ripgrep-13.0.0-x86_64-pc-windows-msvc.zip")
ripgrep_win32.bin("rg.exe")
ripgrep_win32.sha1("f54ffef82c50d6079f211ed5348a81aa98274881")
ripgrep_win32.source("https://github.com/BurntSushi/ripgrep/releases/latest", "ripgrep-(\d+\.\d+\.\d+)-x86_64")

ripgrep_linux = flow.describe.Tool()
ripgrep_linux.version("13.0.0")
ripgrep_linux.platform("linux")
ripgrep_linux.payload("https://github.com/BurntSushi/ripgrep/releases/download/13.0.0/ripgrep-13.0.0-x86_64-unknown-linux-musl.tar.gz")
ripgrep_linux.bin("rg")
ripgrep_linux.sha1("eae550a5ed0fcef8bc555c9ec41c593b8b5aea46")

ripgrep_darwin = flow.describe.Tool()
ripgrep_darwin.version("13.0.0")
ripgrep_darwin.platform("darwin")
ripgrep_darwin.payload("https://github.com/BurntSushi/ripgrep/releases/download/13.0.0/ripgrep-13.0.0-x86_64-apple-darwin.tar.gz")
ripgrep_darwin.bin("rg")
ripgrep_darwin.sha1("f09500e866c49c641ecb42fd8a0089d664d7d385")



#-------------------------------------------------------------------------------
build_target = flow.describe.Command()
build_target.source("cmds/build.py", "Build")
build_target.invoke("build", "target")

for target_type in ("Editor", "Program", "Server", "Client", "Game"):
    cmd = flow.describe.Command()
    cmd.source("cmds/build.py", target_type)
    cmd.invoke("build", target_type.lower())
    globals()["build_" + target_type] = cmd

    cmd = flow.describe.Command()
    cmd.source("cmds/build.py", "Clean" + target_type)
    cmd.invoke("build", "clean", target_type.lower())
    globals()["build_clean" + target_type] = cmd

    del cmd # so as not to pollute global scope with flow commands

#-------------------------------------------------------------------------------
build_xml_show = flow.describe.Command()
build_xml_show.source("cmds/build_xml.py", "Show")
build_xml_show.invoke("build", "xml")

build_xml_edit = flow.describe.Command()
build_xml_edit.source("cmds/build_xml.py", "Edit")
build_xml_edit.invoke("build", "xml", "edit")

build_xml_set = flow.describe.Command()
build_xml_set.source("cmds/build_xml.py", "Set")
build_xml_set.invoke("build", "xml", "set")

build_xml_clear = flow.describe.Command()
build_xml_clear.source("cmds/build_xml.py", "Clear")
build_xml_clear.invoke("build", "xml", "clear")

#-------------------------------------------------------------------------------
build_clangdb = flow.describe.Command()
build_clangdb.source("cmds/clangdb.py", "ClangDb")
build_clangdb.invoke("build", "misc", "clangdb")

#-------------------------------------------------------------------------------
run_editor = flow.describe.Command()
run_editor.source("cmds/run.py", "Editor")
run_editor.invoke("run", "editor")

run_commandlet = flow.describe.Command()
run_commandlet.source("cmds/run.py", "Commandlet")
run_commandlet.invoke("run", "commandlet")

run_program = flow.describe.Command()
run_program.source("cmds/run.py", "Program")
run_program.invoke("run", "program")

run_server = flow.describe.Command()
run_server.source("cmds/run.py", "Server")
run_server.invoke("run", "server")

run_client = flow.describe.Command()
run_client.source("cmds/run.py", "Client")
run_client.invoke("run", "client")

run_game = flow.describe.Command()
run_game.source("cmds/run.py", "Game")
run_game.invoke("run", "game")

run_target = flow.describe.Command()
run_target.source("cmds/run.py", "Target")
run_target.invoke("run", "target")

#-------------------------------------------------------------------------------
run_include_tool = flow.describe.Command()
run_include_tool.source("cmds/run_include_tool.py", "RunIncludeTool")
run_include_tool.invoke("run", "includetool")

#-------------------------------------------------------------------------------
kill = flow.describe.Command()
kill.source("cmds/kill.py", "Kill")
kill.invoke("kill")

#-------------------------------------------------------------------------------
cook = flow.describe.Command()
cook.source("cmds/cook.py", "Cook")
cook.invoke("cook")

cook_server = flow.describe.Command()
cook_server.source("cmds/cook.py", "Server")
cook_server.invoke("cook", "server")

cook_client = flow.describe.Command()
cook_client.source("cmds/cook.py", "Client")
cook_client.invoke("cook", "client")

cook_game = flow.describe.Command()
cook_game.source("cmds/cook.py", "Game")
cook_game.invoke("cook", "game")

#-------------------------------------------------------------------------------
ddc_auth = flow.describe.Command()
ddc_auth.source("cmds/ddc.py", "Auth")
ddc_auth.invoke("ddc", "auth")

#-------------------------------------------------------------------------------
sln_generate = flow.describe.Command()
sln_generate.source("cmds/sln.py", "Generate")
sln_generate.invoke("sln", "generate")

sln_open = flow.describe.Command()
sln_open.source("cmds/sln.py", "Open")
sln_open.invoke("sln", "open")

#-------------------------------------------------------------------------------
uat = flow.describe.Command()
uat.source("cmds/uat.py", "Uat")
uat.invoke("uat")

#-------------------------------------------------------------------------------
stage = flow.describe.Command()
stage.source("cmds/stage.py", "Stage")
stage.invoke("stage")

#-------------------------------------------------------------------------------
deploy = flow.describe.Command()
deploy.source("cmds/stage.py", "Deploy")
deploy.invoke("deploy")

#-------------------------------------------------------------------------------
info = flow.describe.Command()
info.source("cmds/info.py", "Info")
info.invoke("info")

#-------------------------------------------------------------------------------
info_projects = flow.describe.Command()
info_projects.source("cmds/info.py", "Projects")
info_projects.invoke("info", "projects")

#-------------------------------------------------------------------------------
info_config = flow.describe.Command()
info_config.source("cmds/info.py", "Config")
info_config.invoke("info", "config")

#-------------------------------------------------------------------------------
notify = flow.describe.Command()
notify.source("cmds/notify.py", "Notify")
notify.invoke("notify")

#-------------------------------------------------------------------------------
project_change = flow.describe.Command()
project_change.source("cmds/project.py", "Change")
project_change.invoke("project")

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
tips = flow.describe.Command()
tips.source("tips.py", "Tips")
tips.invoke("tip")
tips.prefix("$")



#-------------------------------------------------------------------------------
vswhere = flow.describe.Tool()
vswhere.version("3.0.1")
vswhere.platform("win32")
vswhere.payload("https://github.com/microsoft/vswhere/releases/download/3.0.1/vswhere.exe")
vswhere.bin("vswhere.exe")
vswhere.sha1("209d10bd07c6dc032ad26b40e4d93227c7c385fb")
vswhere.source("https://github.com/microsoft/vswhere/releases", "(\d+\.\d+\.\d+)/vswhere.exe")



#-------------------------------------------------------------------------------
def _add_debugger(name):
    extension = flow.describe.Extension()
    extension.source(f"debuggers/{name}.py", "Debugger")
    extension.mount("debuggers", name)
    globals()[name] = extension

_add_debugger("lldb")
if sys.platform == "win32":
    _add_debugger("vs")



#-------------------------------------------------------------------------------
unreal = flow.describe.Channel()
unreal.version("0")
unreal.extendable("debuggers")
