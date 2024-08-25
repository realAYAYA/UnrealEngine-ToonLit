# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import flow.cmd
import importlib.util as imputil

#-------------------------------------------------------------------------------
def _set_title_inner(t, s):
    print("\x1b]2;", t, " - ushell", s, sep="", file=sys.stdout)

if os.name == "nt":
    def _set_title(t):
        _set_title_inner(t, "\x1b\x5c")
else:
    def _set_title(t):
        _set_title_inner(t, "\x07")

#-------------------------------------------------------------------------------
def _reply(line):   sys.stdout.write(line + "\n")
def _reply_ok():    sys.stdout.write("\x01\n")
def _reply_error(): sys.stdout.write("\x02\n")

#-------------------------------------------------------------------------------
def _get_prompt(system, line_iter, reply):
    cmd_tree = system.get_command_tree()
    prompt_node = cmd_tree.get_root_node().find_child("$prompt")
    if not prompt_node:
        return

    if not prompt_node.is_invokeable():
        return

    prompt_class = prompt_node.get_command_class()
    if not prompt_class:
        return

    context = prompt_class.get_class_type().Context()

    prompt = prompt_class.construct()
    prompt.prompt(context)

    tags = {k:v for k,v in context.read_tags()}
    def expand_tags(line):
        for key, value in tags.items():
            line = line.replace(f":{key}:", value)
        return line

    if title := (os.getenv("FLOW_TITLE") or getattr(context, "TITLE", None)):
        title = expand_tags(title)
        _set_title(title)

    for line in line_iter:
        line = expand_tags(line)
        _reply(line)

#-------------------------------------------------------------------------------
def _get_tags(system, reply):
    cmd_tree = system.get_command_tree()
    prompt_node = cmd_tree.get_root_node().find_child("$prompt")
    if not prompt_node:
        return

    if not prompt_node.is_invokeable():
        return

    prompt_class = prompt_node.get_command_class()
    if not prompt_class:
        return

    context = prompt_class.get_class_type().Context()

    prompt = prompt_class.construct()
    prompt.prompt(context)

    for k,v in context.read_tags():
        _reply(k)
        _reply(v)

#-------------------------------------------------------------------------------
def _serve(system, file=sys.stdin):
    def read_lines():
        inner = (x.rstrip("\r\n") for x in file)
        for line in inner:
            if line == "\x01":
                break

        for line in inner:
            if line == "\x02":
                break
            yield line

    delim = " "
    lines = []
    for line in read_lines():
        if line.startswith("\x03"):
            delim = line[1:]
        else:
            lines.append(line)

    _reply_ok() if _complete(system, lines, _reply, delim) else _reply_error()

#-------------------------------------------------------------------------------
def _complete(system, word_iter, reply, delim):
    words = list(word_iter)

    if words and words[0] == "$":
        _get_prompt(system, words[1:], reply)
        return True

    if words and words[0] == "$$":
        _get_tags(system, reply)
        return True

    word_iter = (x for x in words[:-1])
    cmd_tree = system.get_command_tree()
    tree_node, tail_args = cmd_tree.find(word_iter)

    tail_args = list(tail_args)

    ok = False
    if not tail_args:
        for name,_ in tree_node.read_children():
            reply(name)
            ok = True

    command_class = tree_node.get_command_class()
    if not command_class:
        return True

    try:
        command = command_class.construct()
        for result in command.read_completion(tail_args, prefix=words[-1], delim=delim):
            reply(str(result))
    except LookupError:
        if not ok:
            return False
    except:
        pass

    return True



#-------------------------------------------------------------------------------
class Complete(flow.cmd.Cmd):
    """ Provides support for adding completion to host shells and for debugging
    cmds' completion routines. Suffix the last word of 'words' with "..." to use
    that as a prefix. Use "$" to print the prompt """
    words   = flow.cmd.Arg([str], "Words to parse and print completion for")
    daemon  = flow.cmd.Opt(False, "Run as a daemon serving requests over stdin/out")
    delim   = flow.cmd.Opt(" ", "The character that delimits the last word")

    def _main_daemon(self, system):
        primary_dir = os.getcwd()
        try:
            while True:
                _serve(system)
                os.chdir(primary_dir)
        except:
            pass

    def main(self):
        channel = self.get_channel()
        system = channel.get_system()

        if self.args.daemon:
            return self._main_daemon(system)

        words = self.args.words
        if words and words[-1].endswith("..."):
            word_iter = (*words[:-1], words[-1][:-3])
        else:
            word_iter = (*words, "")

        return _complete(system, word_iter, print, self.args.delim)
