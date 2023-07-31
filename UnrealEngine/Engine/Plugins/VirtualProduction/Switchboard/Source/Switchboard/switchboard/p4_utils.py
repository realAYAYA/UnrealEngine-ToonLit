# Copyright Epic Games, Inc. All Rights Reserved.

import marshal
import pathlib
import subprocess
from functools import wraps

from . import switchboard_utils as sb_utils
from .switchboard_logging import LOGGER


def p4_login(f):
    @wraps(f)
    def wrapped(*args, **kwargs):
        try:
            return f(*args, **kwargs)
        except Exception as e:
            LOGGER.error(f'{repr(e)}')
            LOGGER.error('Error running P4 command. Please make sure you are logged into Perforce and environment variables are set')
            return None

    return wrapped


@p4_login
def p4_latest_changelist(p4_path, working_dir, num_changelists=10):
    """
    Return (num_changelists) latest CLs
    """
    p4_command = f'p4 -ztag -F "%change%" changes -m {num_changelists} {p4_path}/...'
    LOGGER.info(f"Executing: {p4_command}")

    p4_result = subprocess.check_output(p4_command, cwd=working_dir, shell=True, startupinfo=sb_utils.get_hidden_sp_startupinfo()).decode()

    if p4_result:
        return p4_result.split()

    return None

def run(cmd, args=[], input=None):
    ''' Runs the provided p4 command and arguments with -G python marshaling 
    Args:
        cmd(str): The p4 command to run. e.g. clients, where
        args(list): List of extra string arguments to include after cmd.
        input: Python object that you want to marshal into p4, if any.

    Returns:
        list: List of marshalled objects output by p4.
    '''

    c = "p4 -z tag -G " + cmd + " " + " ".join(args)

    p = subprocess.Popen(c, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)

    if input:
        marshal.dump(input, p.stdin, 0)
        p.stdin.close()

    r = []

    try:
        while True:
            x = marshal.load(p.stdout)
            r = r + [ x ]
    except EOFError: 
        pass

    return r


def valueForMarshalledKey(obj: dict, key: str):
    ''' The P4 marshal is using bytes as keys instead of strings,
    so this makes the conversion and returns the desired value for the given key.

    Args:
        obj: The marshalled object, typically the element of a list returned by a p4 -G command.
        key(str): The key identifying the dict key desired from the object.
    '''
    return obj[key.encode('utf-8')].decode()


def hasValueForMarshalledKey(obj: dict, key: str):
    '''
    Checks whether a key exists.
    See valueForMarshalledKey.
    '''
    return key.encode('utf-8') in obj


def workspaceInPath(ws, localpath):
    ''' Validates if the give localpath can correspond to the given workspace.

    Args:
        localpath(str): Local path that we are checkings against
        ws: Workspace, as returned by p4 -G clients

    Returns:
        bool: True if the workspace is the same or a base folder of the give localpath
    '''
    localpath = pathlib.Path(localpath)
    wsroot = pathlib.Path(valueForMarshalledKey(ws,'Root'))

    return (wsroot == localpath) or (wsroot in localpath.parents)


def p4_from_localpath(localpath, workspaces, preferredClient):
    ''' Returns the first client and p4 path that matches the given local path.
    Normally you would pre-filter the workspaces by hostname.
    
    Args:
        localpath(str): The local path that needs to match the workspace
        workspaces(list): The list of candidate p4 workspaces to consider, as returned by p4 -G clients.
        preferredClient(str): Client to prefer, if there are multiple candidates.

    Returns:
        str,str: The workspace name and matching p4 path.
    '''

    # Only take into account workspaces with the same give local path
    wss = [ws for ws in workspaces if workspaceInPath(ws, localpath)]

    # Having a single candidate ws is unambiguous, so we use it regardless of manual entry. 
    # But if we have more candidate workspaces, prefer manual entry and if none then pick the first candidate.
    if len(wss) == 0:
        raise FileNotFoundError
    elif len(wss) == 1:
        client = valueForMarshalledKey(wss[0],'client')
    else:
        client = next((valueForMarshalledKey(ws, 'client') for ws in wss if valueForMarshalledKey(ws, 'client')==preferredClient), preferredClient)
        if not client:
            client = valueForMarshalledKey(wss[0],'client')
    
    # now we need to determine the corresponding p4 path to the full engine path
    wheres = run(cmd=f'-c {client} where', args=[localpath])

    if not len(wheres):
        raise FileNotFoundError

    p4path = valueForMarshalledKey(wheres[0], 'depotFile')

    return client, p4path

