
 ushell - A command line interface for the Unreal Engine

# Quick Start Guide

1. Create a shortcut to 'ushell.bat'
2. Set the start up directory to the location of a .uproject
3. Start ushell.
4. Run '.help' to see available commands.

The properties of the shortcut should look like this;

    - Target:   d:\ushell\ushell.bat
    - Start in: d:\perforce\branch\QAGame

Alternatively you can use the '--project=' command line argument;

    - Target:   d:\ushell\ushell.bat --project=d:\perforce\branch\QAGame
    - Start in: d:\my\favourite\directory

The `.project [path_to_uproject]` command can be used to change the session's
active project from within ushell.

# Mac and Linux

On POSIX-based platforms ushell works by establishing itself in the current
shell;

  source ushell.sh

The arguments and working directory rules in the Quick Start Guide above also
apply when sourcing ushell.sh. Ensuring a suitable version of Python is available
(and possibly a toolchain to build Pips) is left to the user to take care of.

# Commands

Perhaps the best way to understand some commands available in ushell and what
they can do is to list a few examples;

 1. .build editor
 2. .build game ps4
 3. .build program UnrealInsights shipping
 4. .cook game ps4
 5. .stage game ps4
 6. .run editor
 7. .run game ps4 --trace -- -ExecCmds="Zippy Bungle Rainbow"
 8. .run program UnrealInsights shipping
 9. .p4 cherrypick 1234567
10. .sln generate
11. .info

Each of ushell's commands accept a '--help' argument which will show
documentation for the command, details on how to its invoked, and descriptions
for the available options.

# Tab Completion And Command History

Anyone familiar with editing commands and recalling previous ones in Bash (i.e.
Readline) will feel at home at a ushell prompt.

There is extensive context-sensitive Tab completion available for commands and
their arguments. Hitting Tab can help both to discover commands and their
arguments, and also aid in fast convenient command entry. For example;

1. .<tab><tab>            : displays available commands
2. .b<tab>                : completes ".build "
3. .run <tab><tab>        : shows options for the .run commands first argument
4. .build editor --p<tab> : adds "--platform=" (further Tabs complete platforms)

Ushell maintains a history of commands run which is persisted from one session to
the next. Previous commands can be conveniently recalled in a few ways. To step
backwards through prior commands by prefix use PgUp;

1. .bu<pgup>                : Cycle back through commands that started with ".bu"
2. .run game switch<pgup>   : Iterate through previous runs on Switch.

A more thorough incremental history search is done with Ctrl-R. This displays a
prompt to enter a search string and will display the latest command with a match.
Further Ctrl-R hits will step backwards through commands that match the search
string (with Ctrl-S stepping forwards). History searching is case-sensitive.

# Scripting

There is modest support for scripting ushell with Batch scripts. This can be
useful for example to schedule overnight sync-builds. Here is a minimal example;

```
@echo off
cd /d d:\branch\myuproj
call d:\ushell\ushell.bat
.p4 sync --all
.build editor
.build client ps4
```

# Chaining Commands

It is also possible to chain commands entered on the command line with '&&';

  .build editor && .cook game ps4

The '&&' will only execute the subsequent command if the previous one succeeded.
Using a single '&' unconditionally runs the next command.

# Customising the Shell

If the host shell is the standard Windows command prompt then ushell will check
for and run `$USERPROFILE/.ushell/hooks/startup.bat` allowing the user to extend
the session. Here is an example script that adds a simple `.bcr` alias;

```
@echo off
doskey .bcr=.uat BuildCookRun -- $*
```

# Alternative Terminals

To use ushell in an alternative terminal such as Windows Terminal or VSCode's
integrated terminal ushell should be started as follows;

  cmd.exe /d/k d:\ushell\ushell.bat --project=d:\branch\myuproj\myuproj.uproject

This is required because by default ushell.bat detects if it was launched
explicitly through Explorer (or a shortcut) and if not the assumption is it is
running in a non-interactive scripting context (see `Scripting`)

# Alternative Host Shells

A lot of the above assumes that cmd.exe is the host shell. Other shells are also
supported; Bash or Zsh on POSIX-based platforms, and PowerShell. There are
scripts in ushell's root folder for using these alternative shells.

## PowerShell

PowerShell integration works by importing ushell as a module. Set your
`PSModulePath` to contain the ushell's directory (this can be done in your
`$PROFILE` file);

```
$env:PSModulePath = "$($env:PSModulePath);c:\path\to\ushell\"
Import-Module ushell
```

If you use a PowerShell prompt enhancer like oh-my-posh you can extract
environment variables from ushell to populate your prompt/window title/etc. The
ushell-related environment variables can be list with `dir env:USHELL*`.

```
function Set-MyPoshContext {
    if ($Global:WhatIfPreference) {
        $env:POSH_WHATIF = "What If"
    }
    else {
        $env:POSH_WHATIF = ""
    }
    if ( $null -ne (Get-Module ushell)) {
        Update-UShellEnvVars | Out-Null
    }
}
New-Alias -Name 'Set-PoshContext' -Value 'Set-MyPoshContext' -Scope Global -Force
```

# Extending ushell

## Case Study

A ushell instance is consists of "flow" which provides the framework for building
a shell and commands for it, and channels, where a channel is a group of commands
and tools. Flow enumerates channels by looking in the `channels/` folder for a
well-known file that describes the channel. On disk a channel may look like;

```
channels/
  mychannel/
    describe.flow.py
    cmds/
      mycmd.py
```

The `describe.flow.py` is how ushell detects the subfolder as a channel. The file
describes what commands the channel adds and how they are invoked. There is also
an API for describing tools and how to fetch them, and a basic plugin mechanism.
Here is a basic example channel with a single command;

```
import flow.describe

# Describe the channel's commands. source() states where the source .py is and
# the class that implements the command. The invoke() method sets how to the
# command should be invoked from the shell - "d:\>.mycmd zippy" in this example
my_cmd = flow.describe.Command()
my_cmd.source("cmds/mycmd.py", "MyCmdClass")
my_cmd.invoke("mycmd", "zippy")

# Describe the channel. The pip() method can be used to install pip from the
# PyPi repository. version() can be used to invalidate a channel when updates
# are pulled. The parent() forms channels into a tree and informs the order of
# inheritance when overriding commands.
channel = flow.describe.Channel()
channel.parent("unreal.core")
channel.version("0")
#channel.pip(...)
```

A command is implemented by deriving a class from `flow.cmd.Cmd` and using a
docstring and `flow.cmd.Arg/Opt` classes to set a description and specify the
commands arguments. Given the example channel above the `mycmd.py` might look
as follows;

```
import flow.cmd

class MyCmdClass(flow.cmd.Cmd):
    """ This is the description of my command shown when --help is given """

    # Describe the commands paramsters with Arg and Opt objects
    argone = flow.cmd.Arg(str,   "A positional argument that must be given")
    argtwo = flow.cmd.Arg("two", "A positional argument with a default value")
    option = flow.cmd.Opt(False, "An boolean optional argument; --option")
    param  = flow.cmd.Opt(5,     "An optional argument that takes a parameter; --param=6")

    # Add completion for arguments with a complete_[argname] method. Completion
    # methods return something iterable (thus they can be generator functions).
    def complete_argone(self, prefix):
        return ("zippy", "bungle", "george")

    # The entry point of the command. Arguments are prepopulated in self.args
    def main(self):
        if self.args.option:
            print(self.args.argument)
```

For working with Unreal branches there is an `unrealcmd` module that specialises
a `flow.cmd.Cmd` object. The most useful method is `get_unreal_context()` which
gives access to the branch, project, targets, and more (details can be found by
reading `channels/unreal/core/pylib/unreal/_context.py`). An example;

```
import unrealcmd

class MyUnrealCmdClass(unrealcmd.Cmd):
    """ This command works with an Unreal branch and/or project """
    argument = unrealcmd.Arg(int,   "...")
    optional = unrealcmd.Opt(False, "...")

    ue_context = self.get_unreal_context()
    if project := ue_context.get_project():
        print("Project name", project.get_name())
```

## Miscellaneous

It possible to inject into existing commands by matching the words used to invoke
them. So if the case study example was changed to `my_cmd.invoke("build", "editor")`
then executing `.build editor` would in fact call `MyCmdClass.main()`. Calling
`super().main()` would in turn run the proper ushell command to build the editor.

There are a few more details about channels not covered thus far. Briefly; a
channel's `pylib` folder is added to PYTHON_PATH so channels can define modules
(handy for sharing between commands). Channels can participate in the boot
process and prompt building - examples can be found in channels with boot and
prompt commands.

Channel names are derived from their file system location; `channels/geoffrey`
gets the name `geoffrey`, and `channels/mr/hayes` becomes `mr.hayes".

## Setting Yourself up to Extend ushell

Currently the best way to add channels is to sync create a clientspec with the
depot path `//depot/usr/martin.ridgers/ushell` in view along with your own
channel mapped into the `channel/X` folder.

Extending the UGS-deployed ushell is in essence impossible at this time for two
reasons; 1) the deployment is overwritten by UGS when a new version becomes
available, and 2) flow only looks in the channels/ sub-folder of where it is
deployed. _This is going to change_ so that users can add channels in a
well-known location that any distribution of ushell can find. To. Do.

# Contact

martin.ridgers@epicgames.com



vim: tw=80 fo=wnt ft=markdown nosi spell
