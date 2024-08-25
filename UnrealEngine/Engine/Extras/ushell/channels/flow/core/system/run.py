# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys

#-------------------------------------------------------------------------------
def _print_sub_commands(*args):
    from flow.text import text
    with text.light_cyan:
        _print_sub_commands_impl(*args)

#-------------------------------------------------------------------------------
def _print_sub_commands_impl(tree_node):
    import shutil
    max_width = shutil.get_terminal_size(fallback=(80,0))[0]

    once = True
    for child_name, child_node in tree_node.read_children():
        if once:
            print("\nSUB-COMMANDS:")
            once = False

        # Get a description for the sub-command
        if child_class := child_node.get_command_class():
            child_command = child_class.construct()
            desc = child_command.get_desc().lstrip()
        else:
            desc = "<%s>" % "|".join(x for x,_ in child_node.read_children())
        desc = desc.replace("\n", "")
        desc = desc.replace("    ", " ")

        if len(desc) > max_width - 16:
            desc = desc[:max_width - 16 - 3] + "..."
        print("  %-12s" % child_name, desc)

#-------------------------------------------------------------------------------
def main(args):
    # Imperfectly undo state that leaks from the command shims on Windows
    if sys.executable == os.getenv("PYTHONEXECUTABLE"):
        del os.environ["PYTHONEXECUTABLE"]

    # Import all the flow goodies
    sys.path.append(os.path.abspath(__file__ + "/../"))
    import flow.system

    manifest, *args = args

    # Traverse the command tree using the given arguments
    working_dir = os.path.dirname(manifest) + "/"
    system = flow.system.System(working_dir)
    cmd_tree = system.get_command_tree()
    tree_node, tail_args = cmd_tree.find(iter(args))

    # Does the found tree node have a command we can execute?
    command_class = tree_node.get_command_class()
    if command_class:
        command = command_class.construct()
        return command.invoke(tail_args)

    # We've reached a point in the command tree that has no command. Inform the
    # user about the available sub-commands.

    next_arg = next(tail_args, None)
    if next_arg and next_arg != "--help":
        from flow.text import text
        with text.light_red:
            print("ERRROR: Unrecognized arguments:", next_arg, *tail_args)

    _print_sub_commands(tree_node)



if __name__ == "__main__":
    ret = main(sys.argv[1:])
    ret = not ret if isinstance(ret, bool) else ret
    raise SystemExit(ret)
