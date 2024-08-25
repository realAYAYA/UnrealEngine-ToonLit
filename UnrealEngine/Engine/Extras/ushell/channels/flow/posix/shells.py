# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import shlex

#-------------------------------------------------------------------------------
class Shells(object):
    def register_shells(self, registrar):
        registrar.add("bash", _Bash)
        registrar.add("zsh", _Zsh)
        return super().register_shells(registrar)



#-------------------------------------------------------------------------------
class _Base(object):
    def __init__(self, system):
        self._system = system

    def get_system(self):
        return self._system

    def boot_shell(self, env, cookie):
        try: os.makedirs(os.path.dirname(cookie))
        except: pass

        with open(cookie, "wt") as out:
            def printer(*args, **kwargs):
                print(*args, **kwargs, file=out)
            self.write_cookie(env, printer)

    def write_cookie(self, env, out):
        out("cd", shlex.quote(os.getcwd()))

        if os.name == "nt":
            pieces = []
            for x in env["PATH"].split(";"):
                x = x.replace("\\", "/")
                if x[1] == ":":
                    x = "/" + x[0] + x[2:]
                pieces.append(x)
            env["PATH"] = ":".join(pieces)

        for key, value in env.read_changes():
            if "-" not in key:
                value = str(value).replace("\n", "\\n")
                out(f"export {key}=$'{value}'")

        out(r"\$tip")
        out("echo")



#-------------------------------------------------------------------------------
class _Bash(_Base):
    def boot_shell(self, env, cookie):
        # Add '\[...\]' around ANSI codes so Bash knows what's displayable
        if prompt := env.get("FLOW_PROMPT", None):
            out_prompt = ""
            prev = 0
            for m in re.finditer("\x1b[^m]+m", prompt):
                out_prompt += prompt[prev:m.start()]
                out_prompt += rf"\[\\033{m.group()[1:]}\]"
                prev = m.end()
            out_prompt += prompt[prev:]
            env["FLOW_PROMPT"] = out_prompt

        return super().boot_shell(env, cookie)

    def write_cookie(self, env, out):
        super().write_cookie(env, out)
        out("source", shlex.quote(self._add_complete_sh()))

    def _add_complete_sh(self):
        system = self.get_system()
        working_dir = system.get_working_dir()
        script_path = working_dir + "complete.bash"

        out_sh = _get_complete_bash()
        cmd_tree = system.get_command_tree()
        tree_root = cmd_tree.get_root_node()
        for name,_ in tree_root.read_children():
            out_sh += " " + shlex.quote(name)

        with open(script_path, "w") as out:
            out.write(out_sh)

        return script_path

def _get_complete_bash():
    return r"""
function _flow_prompt() {
    if [[ ! -z "$FLOW_PROMPT" ]]; then
        local prompt=${FLOW_PROMPT/:FLOW_CWD:/$(dirs +0)}
        PS1=$(\$complete -- \$ "$prompt")
    fi
}
if [[ ! -z "$FLOW_PROMPT" ]]; then
    PROMPT_COMMAND=_flow_prompt
fi

function _flow_complete() {
    local choices=$(\$complete -- ${COMP_WORDS[@]}...)
    COMPREPLY=($(compgen -W "$choices" -- ${COMP_WORDS[$COMP_CWORD]}))
}
complete -F _flow_complete""" # purposely left no trailing CRLF



#-------------------------------------------------------------------------------
class _Zsh(_Base):
    def boot_shell(self, env, cookie):
        if prompt := env.get("FLOW_PROMPT", None):
            # Add '%{...%}' around ANSI codes so Zsh knows what's displayable
            out_prompt = ""
            prev = 0
            for m in re.finditer("\x1b[^m]+m", prompt):
                out_prompt += prompt[prev:m.start()]
                out_prompt += "%{" + m.group() + "%}"
                prev = m.end()
            prompt = out_prompt + prompt[prev:]

            # Convert :TAG: to $TAG
            out_prompt = ""
            prev = 0
            for m in re.finditer(":[A-Z_]+:", prompt):
                out_prompt += prompt[prev:m.start()]
                out_prompt += "$" + m.group()[1:-1]
                prev = m.end()
            prompt = out_prompt + prompt[prev:]

            env["FLOW_PROMPT"] = prompt

        return super().boot_shell(env, cookie)

    def write_cookie(self, env, out):
        super().write_cookie(env, out)
        out("source", shlex.quote(self._add_complete_sh()))

    def _add_complete_sh(self):
        system = self.get_system()
        working_dir = system.get_working_dir()
        script_path = working_dir + "complete.zsh"

        cmd_tree = system.get_command_tree()
        tree_root = cmd_tree.get_root_node()
        flow_cmds = "' '".join(name for name,_ in tree_root.read_children())

        out_sh = _get_complete_zsh()
        out_sh = out_sh.replace("$$$FLOW_CMDS$$$", flow_cmds)

        with open(script_path, "w") as out:
            out.write(out_sh)

        return script_path

def _get_complete_zsh():
    return r"""
_flow_prompt() {
    FLOW_CWD=$(dirs)
    eval $(\$prompt --format=sh)
}
setopt promptsubst
if [[ $FLOW_PROMPT ]]; then
    PROMPT=$FLOW_PROMPT
fi
if [[ -z "$FLOW_CMDS" ]]; then
    precmd_functions+=(_flow_prompt)
fi

function _flow_complete() {
    # setopt local_options xtrace
    compadd $('$complete' -- ${^^words[@]:0:-1} $PREFIX...)
}
compdef _flow_complete '$$$FLOW_CMDS$$$'
"""
