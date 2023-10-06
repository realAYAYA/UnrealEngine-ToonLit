import sys
import os
import re
from collections import defaultdict

# Adds the ScriptPlugins folder to path so that we can find the other files
# in our module
scriptPluginsFolder = os.path.dirname(os.path.realpath(os.curdir))
if scriptPluginsFolder not in sys.path:
    sys.path.append(scriptPluginsFolder)

# Illegal characters and names for nodes, materials, variants and variant sets
illegalCharacters = re.compile(r"[^A-Za-z0-9_-]+")
illegalSuffixes = ('_Light', '_Mesh', '_SharedObject')
illegalNames = ['SceneRoot', 'VRED2Unreal', 'None']
illegalPrefixes = ('_')

class XMLNodeAdapter:
    """
    This class bridges getName/setName/getID functions to
    xml node get('name') and set('name', val) functions, so that
    we can use our renameObjectsAndKeepOriginals function with a list
    of xml nodes
    """
    def __init__(self, xmlNode):
        self.xmlNode = xmlNode

    def getName(self):
        return self.xmlNode.get('name')
    def setName(self, value):
        self.xmlNode.set('name', value)
    def getID(self):
        return id(self)
    def getType(self):
        return self.xmlNode.tag

    def __str__(self):
        return self.__repr__()
    def __repr__(self):
        return 'Adapter[name="' + self.getName() + '", type="' + self.getType() + '", id="' + str(self.getID()) + '"]'

def sanitize(name):
    replaced = re.subn(illegalCharacters, '_', name)
    modified = replaced[1] > 0
    result = replaced[0]

    if result.endswith(illegalSuffixes) or result in illegalNames:
        result += "_"
        modified = True

    if result.startswith(illegalPrefixes):
        result = 'Object' + result
        modified = True

    return result, modified

def renameObjectsAndKeepOriginals(objs):
    """
    This function iterates over all objects in objs, sanitizing their names
    and keeping the original names in a dictionary, so that they can be restored later.

    Args:
        objs (enumerable of T): Enumerable container of hashable objects that have
                                the 'getName()' and 'setName(value)' functions

    Returns:
        (dict of T.getID() to str): Dictionary with object IDs and their original names
    """
    renamedObjs = {}

    # Replace illegal characters and handle illegal suffixes or illegal names
    for obj in objs:
        objID = obj.getID()
        originalName = obj.getName()

        sanitized = sanitize(originalName)
        if sanitized[1]:
            # Just keep track of the original names
            if objID not in renamedObjs:
                renamedObjs[objID] = originalName

            obj.setName(sanitized[0])

    # Group objs with the same name in a dict, ignoring case
    objectNamesToObjs = defaultdict(list)
    for obj in objs:
        objectNamesToObjs[obj.getName().lower()].append(obj)

    # Rename objs with the same name
    for _, objectList in objectNamesToObjs.items():
        if len(objectList) < 2:
            continue

        for count, objToRename in enumerate(objectList):
            objToRenameID = objToRename.getID()

            if objToRenameID not in renamedObjs:
                renamedObjs[objToRenameID] = objToRename.getName()

            newName = objToRename.getName() + "_" + str(count)
            objToRename.setName(newName)

    return renamedObjs