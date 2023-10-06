# Copyright Epic Games, Inc. All Rights Reserved.

#-------------------------------------------------------------------------------
def _optional_api(func):
    def inner(self, *args, **kwargs):
        if not hasattr(self, "_" + func.__name__):
            raise NotImplementedError(f"Platform '{self.get_name()}' does not implement method '{func.__name__}()'")
        return func(self, *args, **kwargs)

    return inner

#-------------------------------------------------------------------------------
class Debugger(object):
    def __init__(self, ue_context):
        self._ue_context = ue_context

    def get_unreal_context(self):
        return self._ue_context

    def get_name(self):
        return type(self).name

    @_optional_api
    def debug(self, exec_context, cmd, *args):
        return self._debug(exec_context, cmd, *args)

    @_optional_api
    def attach(self, pid, transport=None, host_ip=None):
        return self._attach(pid, transport, host_ip)
