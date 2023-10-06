# Copyright Epic Games, Inc. All Rights Reserved.

import sys

#-------------------------------------------------------------------------------
def backtrace(exc_type, exc_value, tb):
    from textwrap import wrap
    from os.path import normpath

    def red_text(x):    return "\x1b[91m" + x + "\x1b[0m"
    def white_text(x):  return "\x1b[97m" + x + "\x1b[0m"

    max_func_len = 0
    frames = []
    while tb:
        frame = tb.tb_frame
        code = frame.f_code
        max_func_len = max(len(code.co_name), max_func_len)
        frames.insert(0, (code.co_name, normpath(code.co_filename), frame.f_lineno))
        tb = tb.tb_next

    def spam(x=""):
        sys.stderr.write(str(red_text("##")) + " " + str(x) + "\n")

    sys.stderr.write("\n")
    message = str(exc_value)
    if len(message):
        spam()
        for line in wrap(str(message), 75):
            spam(white_text(line))
    spam()
    spam(red_text("[%s]") % exc_type.__name__)
    spam()

    max_file_len = max(16, 55 - max_func_len)
    for func, file, line in frames[:4]:
        if len(file) > max_file_len:
            file = "_" + file[-max_file_len:]
        dots = "." * (max_func_len - len(func))
        x = "%s ..%s %s:%d" % (func, dots, file, line)
        spam(red_text(x))

    spam()



sys.excepthook = backtrace
