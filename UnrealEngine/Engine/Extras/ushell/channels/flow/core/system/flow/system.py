# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import marshal
import importlib.util

#-------------------------------------------------------------------------------
class _FlowObject(object):
    def __init__(self, system):
        self._system = system

    def get_system(self):
        return self._system



#-------------------------------------------------------------------------------
class _LoadableClass(_FlowObject):
    def __init__(self, system, name, specs):
        super().__init__(system)
        self._name = name
        self._specs = specs
        self._class_type = None

    def get_specs(self):
        return self._specs

    def _load_spec(self, cmd_spec):
        channel_id, py_path, py_class = cmd_spec

        channel = self.get_system().get_channel(channel_id)
        full_py_path = channel.get_path() + py_path

        spec = importlib.util.spec_from_file_location("", full_py_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        ret = getattr(module, py_class, None)
        if not ret:
            raise LookupError(f"Failed to create class '{channel}/{py_path}:{py_class}'")

        return ret

    def get_class_type(self):
        if not self._class_type:
            specs = tuple(self._load_spec(x) for x in self._specs)
            self._class_type = type(self._name, specs, {})
        return self._class_type

    def construct(self, *args, **kwargs):
        return self.get_class_type()(*args, **kwargs)



#-------------------------------------------------------------------------------
class _CommandClass(_LoadableClass):
    def __init__(self, system, channel_id, specs):
        super().__init__(system, "flow.cmd", specs)
        self._channel_id = channel_id

    def construct(self):
        instance = super().construct()
        instance._channel = self.get_system().get_channel(self._channel_id)
        instance.post_construct()
        return instance

#-------------------------------------------------------------------------------
class _CommandNode(_FlowObject):
    _MAGIC = 493

    def __init__(self, system, node):
        super().__init__(system)
        self._node = node

    def read_children(self):
        system = self.get_system()
        for name, child_node in self._node.items():
            if isinstance(name, str):
                yield name, _CommandNode(system, child_node)

    def find_child(self, name):
        child = self._node.get(name, None)
        if child:
            return _CommandNode(self.get_system(), child)

    def get_command_class(self):
        specs = self._node.get(_CommandNode._MAGIC, None)
        if specs:
            return _CommandClass(self.get_system(), specs[0], specs[1:])

    def is_invokeable(self):
        return self._node.get(_CommandNode._MAGIC, None) != None

#-------------------------------------------------------------------------------
class _CommandTree(_FlowObject):
    def __init__(self, system, root_node):
        super().__init__(system)
        self._root_node = root_node

    def get_root_node(self):
        return _CommandNode(self.get_system(), self._root_node)

    def find(self, arg_iter):
        arg = None
        node = self._root_node
        for arg in arg_iter:
            try:
                node = node[arg]
            except KeyError:
                break
        else:
            arg = None

        def read_ret_args():
            if arg != None: yield arg
            yield from arg_iter

        return _CommandNode(self.get_system(), node), read_ret_args()



#-------------------------------------------------------------------------------
class _ExtensionClass(_LoadableClass):
    pass

#-------------------------------------------------------------------------------
class _Extendable(_FlowObject):
    def __init__(self, system, data):
        super().__init__(system)
        self._data = data

    def read_extensions(self):
        system = self.get_system()
        yield from ((k, _ExtensionClass(system, k, v)) for k,v in self._data.items())

    def get_extension_class(self, name):
        specs = self._data.get(name, None)
        if specs:
            return _ExtensionClass(self.get_system(), "ext." + name, specs)

        raise ValueError(f"Unknown extendable '{name}'")



#-------------------------------------------------------------------------------
class _Tool(_FlowObject):
    def __init__(self, system, name, data):
        super().__init__(system)
        self._name = name
        self._data = data

    def get_name(self):
        return self._name

    def get_dir(self):
        return self.get_system().get_tools_dir() + f"{self._name}-{self._data[0]}/"

    def get_version(self):
        return self._data[0]

    def get_bin_paths(self):
        return self._data[1]



#-------------------------------------------------------------------------------
class _Channel(_FlowObject):
    def __init__(self, system, data):
        super().__init__(system)
        self._data = data

    def get_name(self):
        return self._data["name"]

    def get_path(self):
        return self._data["path"]

    def get_parent(self):
        parent = self._data["parent"]
        if parent >= 0:
            return self.get_system().get_channel(parent)

    def get_extendable(self, name):
        extendable = self._data["extendables"].get(name, None)
        if extendable:
            return _Extendable(self.get_system(), extendable)

        parent = self.get_parent()
        if parent:
            return parent.get_extendable(name)

        raise ValueError(f"Unknown extendable '{name}'")

    def read_tools(self):
        for name, tool in self._data["tools"].items():
            yield _Tool(self.get_system(), name, tool)

    def get_tool(self, name):
        tool = self._data["tools"].get(name, None)
        if tool:
            return _Tool(self.get_system(), name, tool)

        parent = self.get_parent()
        if parent:
            return parent.get_tool(name)

        raise ValueError(f"Unknown tool '{name}'")



#-------------------------------------------------------------------------------
class System(object):
    def __init__(self, working_dir):
        self._working_dir = working_dir
        self._manifest = None
        self._mtime = 0

    def _get_manifest(self):
        if not self._manifest:
            with open(self.get_working_dir() + "manifest", "rb") as x:
                self._manifest = marshal.load(x)

            for pylib_dir in self._manifest["pylib_dirs"]:
                sys.path.append(pylib_dir)
        return self._manifest

    def get_working_dir(self):
        return self._working_dir

    def get_temp_dir(self):
        ret = self.get_working_dir() + "temp/"
        try: os.mkdir(ret)
        except FileExistsError: pass
        return ret

    def get_tools_dir(self):
        manifest = self._get_manifest()
        return manifest["tools_dir"]

    def get_command_tree(self):
        manifest = self._get_manifest()
        return _CommandTree(self, manifest["cmd_tree"])

    def read_channels(self):
        manifest = self._get_manifest()
        yield from (_Channel(self, x) for x in manifest["channels"])

    def get_channel(self, index):
        manifest = self._get_manifest()
        return _Channel(self, manifest["channels"][index])

    def is_path_stale(self, path):
        manifest_path = self.get_working_dir() + "manifest"
        try:
            self._mtime = self._mtime or os.path.getmtime(manifest_path)
            test_mtime = os.path.getmtime(path)
            return test_mtime <= self._mtime
        except FileNotFoundError:
            return True
