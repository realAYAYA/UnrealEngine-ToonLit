# Copyright Epic Games, Inc. All Rights Reserved.

#-------------------------------------------------------------------------------
class Printer(object):
    def _thread(self):
        try:
            for line in iter(self._stdout.readline, b""):
                self._print(line.decode(errors="replace").rstrip())
        except (IOError, ValueError):
            pass
        finally:
            self._stdout.close()

    def run(self, runnable):
        if not hasattr(self, "_print"):
            name = type(self).__name__
            raise SyntaxError("Printer '{name}' must implement a _print() method")

        self._stdout = runnable.launch(stdout=True)

        import threading
        thread = threading.Thread(target=self._thread)
        thread.start()
        runnable.wait()
        thread.join()
