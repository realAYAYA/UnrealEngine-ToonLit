# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
from pathlib import Path

#-------------------------------------------------------------------------------
def _enable_vt100():
    if os.name != "nt":
        return

    import ctypes
    _win_dll = ctypes.LibraryLoader(ctypes.WinDLL)
    _get_std_handle = _win_dll.kernel32.GetStdHandle
    _get_console_mode = _win_dll.kernel32.GetConsoleMode
    _set_console_mode = _win_dll.kernel32.SetConsoleMode

    con_mode = ctypes.c_int()
    stdout_handle = _get_std_handle(-11)
    _get_console_mode(stdout_handle, ctypes.byref(con_mode))
    con_mode = con_mode.value | 4
    _set_console_mode(stdout_handle, con_mode)

#-------------------------------------------------------------------------------
def _main():
    # Flow is started like this; boot.py [working_dir] [channel_dir] -- [shell_args]
    boot_dirs = []
    shell_args = []
    collector = boot_dirs
    for arg in sys.argv[1:]:
        if arg == "--":
            collector = shell_args
            continue

        collector.append(arg)

    lib_dir = Path(__file__).resolve().parent / "lib"
    sys.path.append(str(lib_dir))

    # Get all the channels ready and aggregated
    import bootstrap
    working_dir, *channels_dirs = (Path(x) for x in boot_dirs)
    working_suffix = bootstrap.impl(working_dir, *channels_dirs)

    # Run the '$boot' command
    import run
    ret = run.main((f"{working_dir}/{working_suffix}/manifest", "$boot", *shell_args))
    raise SystemExit(ret)



if __name__ == "__main__":
    _enable_vt100()
    _main()
