# Copyright Epic Games, Inc. All Rights Reserved.

import os

#-------------------------------------------------------------------------------
def read_files(dir):
    try:
        files = os.scandir(dir)
        yield from files
        files.close()
    except FileNotFoundError:
        pass

#-------------------------------------------------------------------------------
def read_dirs(dir):
    return (x for x in read_files(dir) if x.is_dir())

#-------------------------------------------------------------------------------
def get_mtime(x):
    try: return os.path.getmtime(x)
    except FileNotFoundError: return 0

#-------------------------------------------------------------------------------
def _rmdir_nt(dir, count=0):
    # shutil.rmtree() deletes the contents of junctions so we can't use it.
    try:
        dir_attr = os.stat(dir, follow_symlinks=False).st_file_attributes
        if dir_attr & 0x400: # 0x400 = FILE_ATTRIBUTE_REPARSE_POINT:
            os.rmdir(dir)
            return
    except FileNotFoundError:
        pass

    return _rmdir_impl(dir, count)

#-------------------------------------------------------------------------------
def _rmdir_posix(dir, count=0):
    if os.path.islink(dir):
        os.remove(dir)
        return

    return _rmdir_impl(dir, count)

#-------------------------------------------------------------------------------
def _rmdir_impl(dir, count=0):
    if not os.path.isdir(dir):
        return

    for item in read_files(dir):
        if item.is_dir():
            rmdir(item.path, count + 1)
        else:
            try: os.unlink(item.path)
            except PermissionError:
                # Try again but with the read-only flag cleared
                os.chmod(item.path, 0xff)
                os.unlink(item.path)

    # Windows takes its time to delete a directory which can cause timing-
    # dependent 'access denied' errors. So we'll move directories to be deleted
    # to known and unique location prior to deletion
    t = dir[:-1] + "~" + str(count)
    os.rename(dir, t)
    os.rmdir(t)

#-------------------------------------------------------------------------------
if os.name == "nt":
    rmdir = _rmdir_nt
else:
    rmdir = _rmdir_posix



#-------------------------------------------------------------------------------
def import_script(script_path):
    import importlib.util as import_util
    spec = import_util.spec_from_file_location("", script_path)
    module = import_util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module



#-------------------------------------------------------------------------------
class WorkPath(object):
    def __init__(self, target_path):
        self._path = target_path
        try: os.makedirs(os.path.dirname(target_path))
        except (FileExistsError, FileNotFoundError): pass

    def __del__(self):
        if self._path:
            rmdir(self._path)

    def __add__(self, x): return self._path + x
    def __str__(self):    return self._path
    def __fspath__(self): return self._path
    def keep(self):       self._path = None



#-------------------------------------------------------------------------------
class DirLock(object):
    def __init__(self, lock_dir):
        self._lock_file = None
        self._dir = os.path.abspath(lock_dir) + "/"
        self._mtime = get_mtime(self._dir)

    def __del__(self):
        self.release()

    def can_claim(self):
        try: os.unlink(self._dir + "lock")
        except FileNotFoundError: pass
        except: return False
        return True

    def claim(self):
        if self._lock_file:
            return True

        # Don't allow a claim if the dir was changed since this lock was created
        now_mtime = get_mtime(self._dir)
        if now_mtime > self._mtime:
            return False

        # Clean up
        try: os.unlink(self._dir + "lock")
        except FileNotFoundError: pass
        except: return False

        # Claim the lock
        try: self._lock_file = open(self._dir + "lock", "wb")
        except: pass

        return True

    def release(self):
        if self._lock_file:
            self._lock_file.close()
            os.unlink(self._dir + "lock")
            self._lock_file = None
            return True
        return False
