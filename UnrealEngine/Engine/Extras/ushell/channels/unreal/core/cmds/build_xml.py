# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import flow.cmd
import unrealcmd

#-------------------------------------------------------------------------------
class Show(unrealcmd.Cmd):
    """ Display the current build configuration for the current project. To
    modify the configuration use the sub-commands set, clear, and edit. """

    def main(self):
        root = {}

        context = self.get_unreal_context()
        ubt = context.get_engine().get_ubt()
        for config in (x for x in ubt.read_configurations() if x.exists()):
            level = config.get_level()
            try:
                for section, name, value in config.read_values():
                    section = root.setdefault(section, {})
                    name = section.setdefault(name, [])
                    name.append((value, level))
            except IOError as e:
                self.print_warning(f"Failed loading '{config.get_path()}'")
                self.print_warning(str(e))
                continue

        col0 = max(len(x) for x in root.keys()) if root else 0
        col0 += 2

        col1 = 0
        for section in root.values():
            n = max(len(x) for x in section.keys())
            col1 = max(col1, n)
        col1 += 2

        def spam(n, x, decorator=None):
            dots = "." * (n - len(x))
            if decorator: x = decorator(x)
            print(x, flow.cmd.text.grey(dots), "", end="")

        self.print_info("Current configuration")
        if not root:
            print("There isn't any!")
        for section, payload in root.items():
            for name, payload in payload.items():
                for value, level in payload:
                    value = value + " (" + level.name.title() + ")"
                    spam(col0, section, flow.cmd.text.cyan)
                    spam(col1, name, flow.cmd.text.white)
                    spam(0, value)
                    print()

        self.print_info("File paths")
        for config in ubt.read_configurations():
            red_green = flow.cmd.text.green if config.exists() else flow.cmd.text.red
            spam(10, config.get_level().name.title(), decorator=red_green)
            spam(0, os.path.normpath(config.get_path()))
            print()

#-------------------------------------------------------------------------------
class _SetClearBase(unrealcmd.Cmd):
    section  = unrealcmd.Arg(str, "Configuration section to modify")
    name     = unrealcmd.Arg(str, "The name of the value to change")
    branch   = unrealcmd.Opt(False, "Only apply changes to this branch (otherwise global)")

    def _complete_schema(self, prefix):
        context = self.get_unreal_context()
        ubt = context.get_engine().get_ubt()
        schema = ubt.get_configuration_schema()

        def read_node_names():
            if self.args.section: yield self.args.section
            if self.args.name: yield self.args.name

        if node := schema.get_node(*read_node_names()):
            yield from (x.get_name() for x in node.read_children())

    complete_section  = _complete_schema
    complete_name = _complete_schema

    def _load_configuration(self):
        self.print_info("Modifying build configuration")

        context = self.get_unreal_context()
        ubt = context.get_engine().get_ubt()
        configuration = ubt.get_configuration(self.args.branch, create=True)

        print("Scope:", configuration.get_level().name.title())
        print("Path:", configuration.get_path())

        return configuration


#-------------------------------------------------------------------------------
class _SetImpl(object):
    """ Modify build configuration. """
    value = unrealcmd.Arg(str, "A value to set the property to")

    complete_value = _SetClearBase._complete_schema

    def main(self):
        section, name, value = self.args.section, self.args.name, self.args.value
        configuration = self._load_configuration()

        prev_value = None
        try:
            prev_value = configuration.get_value(section, name)
        except LookupError:
            pass

        configuration.set_value(section, name, value)
        configuration.save()

        print(f"Setting '{section}.{name}' to '{value}' (was '{prev_value}')")
        with flow.cmd.text.green:
            print("Success")

#-------------------------------------------------------------------------------
# This exists simply to get arguments (i.e. unreal.Arg) in the correct order.
class Set(_SetClearBase, _SetImpl):
    pass

#-------------------------------------------------------------------------------
class Clear(_SetClearBase):
    """ Clears a value from the build configuration. """

    def main(self):
        section, name = self.args.section, self.args.name
        configuration = self._load_configuration()

        prev_value = None
        try:
            prev_value = configuration.get_value(section, name)
        except LookupError:
            pass

        try:
            configuration.clear_value(section, name)
            configuration.save()
        except LookupError as e:
            self.print_error(e)
            return False

        print(f"Cleared '{section}.{name}' (was '{prev_value}')")
        with flow.cmd.text.green:
            print("Success")

#-------------------------------------------------------------------------------
class Edit(unrealcmd.Cmd):
    """ Opens a build configuration in a text editor. The text editor used is
    determined in the following order; $GIT_EDITOR, $P4EDITOR, system default. """
    branch = unrealcmd.Opt(False, "Edit the build configuration for this branch")

    def main(self):
        context = self.get_unreal_context()
        ubt = context.get_engine().get_ubt()
        configuration = ubt.get_configuration(self.args.branch, create=True)

        path = configuration.get_path()
        print("Editing", path)
        self.edit_file(path)
