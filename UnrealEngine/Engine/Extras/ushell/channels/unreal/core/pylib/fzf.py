# Copyright Epic Games, Inc. All Rights Reserved.

import subprocess

#-------------------------------------------------------------------------------
def run(item_iter, *, height=10, prompt=">", multi=False, ansi=False, sort=True, ext_stdin=None):
    fzf_args = (
        "--prompt=" + prompt if prompt else None,
        None if sort else "--no-sort",
        "--multi" if multi == True else "--multi=" + str(multi) if multi > 0 else None,
        "--height=" + str(height + 2), # "+2" because fzf adds two lines of prompt
        "--color=16",
        "--layout=reverse",
        "--no-mouse",
        "--ansi" if ansi else None,
    )
    proc = subprocess.Popen(
        ("fzf", *(x for x in fzf_args if x)),
        universal_newlines=True,
        stdout=subprocess.PIPE,
        stdin=ext_stdin if ext_stdin else subprocess.PIPE,
    )

    try:
        if not ext_stdin:
            try:
                for item in item_iter:
                    item = str(item) + "\n"
                    proc.stdin.write(item)
            except (BrokenPipeError, OSError):
                pass

            try: proc.stdin.close()
            except: pass

        yield from (x.strip() for x in proc.stdout.readlines())
        proc.stdout.close()

        proc.wait()
    finally:
        proc.kill()
