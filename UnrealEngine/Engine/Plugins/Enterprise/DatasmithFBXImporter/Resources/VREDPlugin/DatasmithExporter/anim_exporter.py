import sys
import os
import logging

import vrScenegraph
import vrAnimWidgets
import vrFieldAccess
import vrNodePtr
import vrOSGTypes
import vrNodeUtils
import vrController
import vrOptimize

# Adds the ScriptPlugins folder to path so that we can find the other files
# in our module
scriptPluginsFolder = os.path.dirname(os.path.realpath(os.curdir))
if scriptPluginsFolder not in sys.path:
    sys.path.append(scriptPluginsFolder)

import DatasmithExporter.utils as dsutils
import DatasmithExporter.sanitizer as dss

# Time where we place the tag key frame to mark the curves
TIME_OF_TAG_KEY = 100

class DSTransformStorage:
    """
    We use these objects to store the transform of vrNodePtr before we call resetAnim(clipname) on
    the clips, since there is no intended way of returning back to the "unanimated" transform
    """
    def __init__(self):
        self.Translation = []
        self.Rotation = []
        self.Scale = []
        self.RotationOrientation = []
        self.Visible = True

class DSAnimNode:
    def __init__(self, name):
        self.Name = name
        self.AnimBlocks = []

class DSAnimBlock:
    def __init__(self, id, name):
        self.DSID = id
        self.Name = name
        self.ChildCurveNamesToCurves = {}

    # Define these so that we can use the same sanitizer functions that
    # we use for VRED scene nodes
    def getName(self):
        return self.Name
    def setName(self, value):
        self.Name = value
    def getID(self):
        return id(self)

class DSAnimCurve:
    def __init__(self, id, animCurve, startSeconds):
        self.DSID = id
        self.AnimCurve = animCurve
        self.StartSeconds = startSeconds

class DSAnimNodeUsage:
    """
    Represents the usage of an AnimNode within an AnimClip. Has Start and End times for the block,
    which may or may not correspond to the node's natural size (i.e. having scaled versions of the block)
    """
    def __init__(self, animNode):
        self.AnimNode = animNode
        self.StartSeconds = 0.0
        self.EndSeconds = 0.0
        self.IsActive = True
        self.Flipped = False

    def getName(self):
        return self.AnimNode.getName()
    def setName(self, value):
        self.AnimNode.setName(value)

class DSAnimClip:
    """
    Represents a parsed animation clip (a vrNodePtr with type AnimClip or AnimWizClip), containing
    a name and the AnimNodeUsages that are used by the clip. The AnimNodeUsages are store in a simple
    list (so the track structure of animation blocks within clips is not respected).
    """
    def __init__(self, name):
        self.Name = name
        self._OriginalName = name
        self.AnimNodeUsages = []
        # Top level clips can be flipped if they're created with 'Duplicate and Flip'
        self.Flipped = False

    # Define these so that we can use the same sanitizer functions that
    # we use for VRED scene nodes
    def getName(self):
        return self.Name
    def setName(self, value):
        self.Name = value
    def getOriginalName(self):
        return self._OriginalName
    def getID(self):
        return id(self)

class DSAnimationLibrary:
    """
    This class serves as a temporary storage for animation data that we extract from the scene.
    It is also capable of sanitizing the names of objects it creates, and finally it is responsible
    for serializing itself to a file with an XML structure.
    """
    __DSID = 0

    def __init__(self):
        DSAnimationLibrary.__DSID = 0

        self.TagTime = sys.float_info.max
        self.BaseTime = 24.0
        self.PlaybackSpeed = 24.0

        self.DSAnimNodes = []
        self.DSClips = []

        # VRED guarantees all AnimNodes have unique names
        self.BlockNameToDSBlock = {}

        self.ClipOriginalNamesToNewNames = {}

    @staticmethod
    def getID():
        """
        Gets an unique identifier and internally increments it

        Returns:
            (int): Datasmith ID, uniquely identifies an animation-related object during export
        """
        DSAnimationLibrary.__DSID += 5
        return DSAnimationLibrary.__DSID - 5

    def sanitize(self):
        """
        Sanitizes the names of blocks and clips. Assumes node names are already sanitized
        """
        def sanitizeRecursive(clip, totalList):
            try:
                for usage in clip.AnimNodeUsages:
                    sanitizeRecursive(usage, totalList)
                totalList.append(clip)
            except AttributeError:
                pass

        # AnimClips can never have repeated names, and even in nested clips you can't have
        # clip references. So every single clip should have a unique name, as it will become
        # an asset in UnrealEditor
        clipsToSanitize = []
        for clip in self.DSClips:
            sanitizeRecursive(clip, clipsToSanitize)

        dss.renameObjectsAndKeepOriginals(clipsToSanitize)

        self.ClipOriginalNamesToNewNames = {}
        for clip in clipsToSanitize:
            self.ClipOriginalNamesToNewNames[clip.getOriginalName()] = clip.getName()

        # The VRED AnimBlocks are sanitized before we even create the DSBlocks, so this
        # might not be needed
        blocksToSanitize = []
        for node in self.DSAnimNodes:
            blocksToSanitize += node.AnimBlocks
        dss.renameObjectsAndKeepOriginals(blocksToSanitize)

    def serialize(self, outFile):
        """
        Writes the animation library to 'outFile' as an XML file, describing the catalogged AnimNodes and AnimClips

        Args:
            outFile (file handle): File to write the library to
        """
        dsutils.writeXMLTag(outFile, 0, 'Root', [], [], False)

        # Serialize blocks and curves
        dsutils.writeXMLTag(outFile, 1, 'KeyTime', ['value'], [self.TagTime], True)
        dsutils.writeXMLTag(outFile, 1, 'BaseTime', ['value'], [self.BaseTime], True)
        dsutils.writeXMLTag(outFile, 1, 'PlaybackSpeed', ['value'], [self.PlaybackSpeed], True)
        dsutils.writeXMLTag(outFile, 1, 'Blocks', [], [], False)
        for node in self.DSAnimNodes:
            dsutils.writeXMLTag(outFile, 2, 'AnimNode', ['name'], [node.Name], False)

            for block in node.AnimBlocks:
                dsutils.writeXMLTag(outFile, 3, 'AnimBlock', ['name'], [block.Name], False)

                for curveName, curve in block.ChildCurveNamesToCurves.items():
                    dsutils.writeXMLTag(outFile, 4, 'AnimCurve', ['name', 'DSID', 'startSeconds'], [curveName, curve.DSID, curve.StartSeconds], True)

                dsutils.writeXMLTag(outFile, 3, '/AnimBlock', [], [], False)

            dsutils.writeXMLTag(outFile, 2, '/AnimNode', [], [], False)

        dsutils.writeXMLTag(outFile, 1, '/Blocks', [], [], False)

        # Serialize clips and used blocks (and sub-clips)
        dsutils.writeXMLTag(outFile, 1, 'Clips', [], [], False)
        for clip in self.DSClips:
            dsutils.writeXMLTag(outFile, 2, 'Clip', ['name', 'isFlipped'], [clip.Name, clip.Flipped], False)

            for blockUsage in clip.AnimNodeUsages:
                dsutils.writeXMLTag(outFile, 3, 'BlockUsage', ['blockName', 'startSeconds', 'endSeconds', 'isActive', 'isFlipped'], \
                                    [blockUsage.getName(), blockUsage.StartSeconds, blockUsage.EndSeconds, blockUsage.IsActive, blockUsage.Flipped], True)

            dsutils.writeXMLTag(outFile, 2, '/Clip', [], [], False)

        dsutils.writeXMLTag(outFile, 1, '/Clips', [], [], False)

        dsutils.writeXMLTag(outFile, 0, '/Root', [], [], False)

    def catalogClip(self, animClip):
        """
        Recursively catalogs top-level animation clips and their child nodes.

        Args:
            animClip (vrNodePtr): Node that is of type AnimClip or AnimWizClip and contains other clips or AnimBlocks
        """

        def catalogClipInner(animClip, blockDestination):
            """
            Catalogs the AnimClip, recursively scanning so that all of its AnimBlocks children (and children of
            children, etc) are added to the simple list in blockDestination.

            Args:
                animClip (vrNodePtr): Node that is of type AnimClip or AnimWizClip and contains other clips or AnimBlocks
                blockDestination (list of AnimBlock vrNodePtr): Where to place the found AnimNode usages into
            """
            nChildren = animClip.getNChildren()
            for childIndex in range(nChildren):
                child = animClip.getChild(childIndex)
                childName = child.getName()
                childType = child.getType()

                bb = child.getBoundingBox()

                createdDSAnimNode = None

                if childType == 'AnimClip' or childType == 'AnimWizClip':
                    # Treat the sub-level clip like a top-level clip
                    createdDSAnimNode = self.catalogClip(child)
                elif childType == 'AnimBlock':
                    # For animblocks we need to find the DSAnimBlock we're
                    # referencing
                    try:
                        createdDSAnimNode = self.BlockNameToDSBlock[childName]
                    except KeyError:
                        logging.warning('Did not find AnimBlock \"' + str(childName) + '\"')
                        continue
                else:
                    continue

                childDSAnimBlockUsage = DSAnimNodeUsage(createdDSAnimNode)
                childDSAnimBlockUsage.StartSeconds = bb[0]  # minX
                childDSAnimBlockUsage.EndSeconds = bb[3]  # maxX
                childDSAnimBlockUsage.IsActive = child.getActive()
                childDSAnimBlockUsage.Flipped = dsutils.getIsNodeFlipped(child)

                blockDestination.append(childDSAnimBlockUsage)

        clipName = animClip.getName()

        # Important to cause boundingBox evaluation. Without this start/end times would come back as zero
        vrAnimWidgets.resetAnim(clipName)

        DSClip = DSAnimClip(clipName)
        DSClip.Flipped = dsutils.getIsNodeFlipped(animClip)
        self.DSClips.append(DSClip)

        catalogClipInner(animClip, DSClip.AnimNodeUsages)

        return DSClip

def getAnimRoot(node):
    """
    Returns the animationRoot of the AnimAttachment of 'node' if it has one.
    Returns None otherwise.

    Args:
        node (vrNodePtr): Node to get the animRoot from

    Returns:
        (vrNodePtr): animationRoot of 'node' or None
    """
    animRoot = None
    if node.hasAttachment("AnimAttachment"):
        animAttachment = node.getAttachment("AnimAttachment")
        animRootId = vrFieldAccess.vrFieldAccess(animAttachment).getFieldContainerID("animationRoot")
        animRoot = vrNodePtr.vrNodePtr(animRootId)
    return animRoot

def splitAnimationFromGeometry(node):
    """
    If 'node' is an animated Geometry node, create a new parent node with its transformation and
    animations, and add the node to it as a child, with its transformation reset

    Args:
        node (vrNodePtr): Node to split

    Returns:
        (vrNodePtr): parent node with animations and transformation (whose child is 'node'), or None
    """
    if node.getType() == 'Geometry' and getAnimRoot(node) is not None:
        parentNode = vrScenegraph.createNode('Transform3D', node.getName() + '_Anim', node.getParent())
        parentNode.addChild(node)

        at = node.getAttachment('AnimAttachment')

        parentNode.addAttachment(at)
        node.subAttachment(at)
        vrScenegraph.copyTransformation(node, parentNode)
        dsutils.resetTransformNode(node)

        logging.debug('Split animated geometry node \"' + str(node.getName()) + '\"')

        return parentNode
    return None

def getFreeAnimCurves(animRoot):
    """
    Returns a list of nodes that are AnimCurves attached directly to animRoot, as opposed
    to being attached to an AnimBlock that is attached to animRoot. Will only return curves
    that have more than one key frame (i.e. valid animation curves)

    Args:
        animRoot (vrNodePtr): animationRoot node of an AnimAttachment

    Returns:
        (list of vrNodePtr): List of 'AnimCurve' nodes
    """
    result = []

    nChildren = animRoot.getNChildren()
    for childIndex in range(nChildren):
        child = animRoot.getChild(childIndex)

        if child.getType() == "AnimCurve":
            bb = child.getBoundingBox()
            # bb[0] and bb[3] are minX and maxX respectively, startSeconds and endSeconds of the curve
            if not dsutils.isClose(bb[0], bb[3]):
                result.append(child)

    return result

def getDSIDforChannelSuffix(channelName, suffix, curveDictionary):
    """
    Attempts to return the DSID of a curve in 'curveDictionary' that has the name 'channelName' + 'suffix'

    Args:
        channelName (str): Something like 'translation', 'scale' or 'visible'
        suffix (str): Something like 'X', 'Y' or 'Z' or just ''
        curveDictionary (dict of curve names to curves): Contains curves to search into

    Returns:
        (int): Found DSID, or zero if the name is not in dict
    """
    try:
        return curveDictionary[channelName + suffix].DSID
    except KeyError:
        return 0

def getDSIDVecForChannel(channelName, curveDictionary):
    """
    Attempts to return a Vec3f of DSIDs for the curves with 'channelName' within 'curveDictionary'

    Args:
        channelName (str): Something like 'translation', 'scale' or 'visible'
        curveDictionary (dict of curve names to curves): Contains curves to search into

    Returns:
        (vrOSGTypes.Vec3f): Vector with the DSIDs of curves with suffixes 'X', 'Y' and 'Z' and 'channelName'
    """
    return vrOSGTypes.Vec3f(getDSIDforChannelSuffix(channelName, 'X', curveDictionary), \
                            getDSIDforChannelSuffix(channelName, 'Y', curveDictionary), \
                            getDSIDforChannelSuffix(channelName, 'Z', curveDictionary))

def getKeyTime():
    """
    Scans all nodes in the scene and returns a time, in seconds, that is guaranteed
    to be at least 1 second earlier than the start of any AnimCurve in the scene.

    We use this to decide where to insert our DSID keys/value paris. We pack this in
    the .clips file to be used when parsing those key/value pairs.

    Returns:
        (float): Time in seconds
    """
    minStartTime = sys.float_info.max

    for node in vrScenegraph.getAllNodes():
        animRoot = getAnimRoot(node)

        if animRoot is None:
            continue

        numBlocks = animRoot.getNChildren()
        for blockIndex in range(numBlocks):
            block = animRoot.getChild(blockIndex)

            numCurves = block.getNChildren()
            for curveIndex in range(numCurves):
                curve = block.getChild(curveIndex)

                bb = curve.getBoundingBox()
                startTimeSeconds = bb[0]

                minStartTime = min(minStartTime, startTimeSeconds)

    return minStartTime

def catalogSceneAnimations(unitScale, baseTime, playbackSpeed):
    """
    Scan the scenegraph for all nodes that have AnimCurves, tags each curve with a marker,
    then builds and returns an AnimationLibrary object that keeps track of Node, Clip and
    Block animation names, as well as their corresponding DSID.

    Args:
        unitScale (float): The unit scale of the scene. 1.0 = mm, 10.0 = cm, 1000.0 = m
                           This needs to be extracted from the UI, so its passed in from the plugin
        baseTime (float): Native framerate of the animations in fps
        playbackSpeed(float): Playback speed of the animations in fps. Animations that are natively
                              1 second long are displayed with total duration 1s * (baseTime / playbackSpeed)
    """
    # Remove vertex animation blocks because those are not yet supported and will lead to the nodes being
    # permanently locked on the END position of the animation
    for node in vrScenegraph.getAllNodes():
        animRoot = getAnimRoot(node)
        if animRoot is None:
            continue

        numBlocks = animRoot.getNChildren()
        for i in range(numBlocks-1, -1, -1):
            block = animRoot.getChild(i)

            if block.getType() == "TimeShapeCache":
                logging.warning('Unsupported vertex animation block \"' + str(block.getName()) + '\". Removing...')
                block.sub()

    # Sanitize block names because if the user managed to create blocks with non-unique names
    # all of this will fail
    dss.renameObjectsAndKeepOriginals(vrAnimWidgets.getAnimBlockNodes(True))
    library = DSAnimationLibrary()
    library.BaseTime = baseTime
    library.PlaybackSpeed = playbackSpeed

    # Moves free curves to newly created nodename_BaseAnimation anim blocks
    # Needs to happen before catalogging clips so that we keep track of these blocks too
    for node in vrScenegraph.getAllNodes():
        animRoot = getAnimRoot(node)

        if animRoot is None:
            continue

        freeCurves = getFreeAnimCurves(animRoot)
        if len(freeCurves) > 0:
            # This block doesn't "work" within VRED, it will just serve as a container for us
            newAnimBlock = vrScenegraph.createNode('AnimBlock', node.getName() + "_BaseAnimation", animRoot)
            for curve in freeCurves:
                newAnimBlock.addChild(curve)

            # Register blocks by their names, to fill them with data later.
            # Do this now so that we can use direct DSAnimBlock references within
            # AnimNodeUsages, automatically renaming them when we sanitize the DSAnimBlock names. Additionally, doing this
            # now allows us to export clips BEFORE we poison the curves with our tags
            library.BlockNameToDSBlock[newAnimBlock.getName()] = DSAnimBlock(library.getID(), newAnimBlock.getName())

    library.TagTime = getKeyTime() - 1.0

    # We need to call resetAnim to calculate bounding boxes and get blockUsage durations for all clips.
    # Creating an 'unanimated' block and using it to store our original transform allows us to reset to it later and
    # export it with our original transform
    unanimatedTransforms = {}
    for node in vrScenegraph.getAllNodes():
        animRoot = getAnimRoot(node)

        if animRoot is None:
            continue

        rotOri = vrNodeUtils.getTransformNodeRotationOrientation(node)

        storage = DSTransformStorage()
        storage.Translation = node.getTranslation()
        storage.Rotation = node.getRotation()
        storage.Scale = node.getScale()
        storage.Visible = node.getActive()
        storage.RotationOrientation = [rotOri.x(), rotOri.y(), rotOri.z()]

        unanimatedTransforms[node.getID()] = storage

    # Register blocks by their names, to fill them with data later.
    # Do this now so that we can use direct DSAnimBlock references within
    # AnimNodeUsages, automatically renaming them when we sanitize the DSAnimBlock names. Additionally, doing this
    # now allows us to export clips BEFORE we poison the curves with our tags
    for block in vrAnimWidgets.getAnimBlockNodes(True) :
        library.BlockNameToDSBlock[block.getName()] = DSAnimBlock(library.getID(), block.getName())

    # Catalogue all clips and their blocks
    # Needs to happen before we poison the curves with our tags, or else the AnimNodeUsages will have distorted start/end times
    clips = vrAnimWidgets.getAnimClipNodes()
    for clip in clips:
        library.catalogClip(clip)

    # Restore nodes to their unanimated states
    for nodeID, storage in unanimatedTransforms.items():
        node = vrNodePtr.vrNodePtr(nodeID)

        node.setTranslation(storage.Translation[0], storage.Translation[1], storage.Translation[2])
        node.setRotation(storage.Rotation[0], storage.Rotation[1], storage.Rotation[2])
        node.setScale(storage.Scale[0], storage.Scale[1], storage.Scale[2])
        node.setActive(storage.Visible)
        vrNodeUtils.setTransformNodeRotationOrientation(node, storage.RotationOrientation[0], storage.RotationOrientation[1], storage.RotationOrientation[2])

    # Catalogs all nodes, their blocks and their curves
    for node in vrScenegraph.getAllNodes():
        animRoot = getAnimRoot(node)

        if animRoot is None:
            continue

        # Use this bool to prevent creating DSAnimNodes for nodes without animBlocks
        createdANodeAlready = False

        # Tag curves in every block, block by block
        numBlocks = animRoot.getNChildren()
        for blockIndex in range(numBlocks):
            block = animRoot.getChild(blockIndex)
            if block.getType() != "AnimBlock":
                continue

            if not createdANodeAlready:
                animNode = DSAnimNode(node.getName())
                library.DSAnimNodes.append(animNode)
                createdANodeAlready = True

            dsBlock = None
            try:
                dsBlock = library.BlockNameToDSBlock[block.getName()]
            except KeyError:
                logging.warning('Animation block with name \"' + str(block.getName()) + '\" does not exist!')
                pass
            if dsBlock is None:
                continue

            animNode.AnimBlocks.append(dsBlock)
            numCurves = block.getNChildren()

            for curveIndex in range(numCurves-1, -1, -1):
                curve = block.getChild(curveIndex)
                if curve.getType() != "AnimCurve":
                    continue

                vrScenegraph.moveNode(curve, block, animRoot)

                bb = curve.getBoundingBox()
                startTimeSeconds = bb[0]

                # We want all rotation curves to have the same dsid, and
                # all scale curves to have another (but consistent) id
                dsid = dsBlock.DSID
                channelName = curve.fields().getString('channelName')
                if channelName == "translation":
                    pass
                elif channelName == "rotation":
                    dsid += 1
                elif channelName == "scale":
                    dsid += 2
                elif channelName == "rotationOrientation":
                    dsid += 3
                elif channelName == "visible":
                    dsid += 4
                else:
                    dsid = -1

                dsCurve = DSAnimCurve(dsid, curve, startTimeSeconds)
                dsBlock.ChildCurveNamesToCurves[curve.fields().getString('channelName') + curve.fields().getString('suffix')] = dsCurve

            # Get tag vectors
            transVec = getDSIDVecForChannel('translation', dsBlock.ChildCurveNamesToCurves)
            rotVec   = getDSIDVecForChannel('rotation', dsBlock.ChildCurveNamesToCurves)
            scaleVec = getDSIDVecForChannel('scale', dsBlock.ChildCurveNamesToCurves)
            visible  = getDSIDforChannelSuffix('visible', '', dsBlock.ChildCurveNamesToCurves)

            frameTime = 1.0 / baseTime

            # We used to have a different one for each component, but now we just take x(),
            # as they will be the same
            transTime = library.TagTime - transVec.x() * frameTime
            rotTime = library.TagTime - rotVec.x() * frameTime
            scaleTime = library.TagTime - scaleVec.x() * frameTime
            visibleTime = library.TagTime - visible * frameTime

            someValue = vrOSGTypes.Vec3f(0.0, 0.0, 0.0)

            # Apply tags
            if not vrAnimWidgets.addTranslationControlPoint(node, transTime, someValue, False):
                logging.warning('Failed to add translation tags for block ' + str(block.getName()))
            if not vrAnimWidgets.addRotationControlPoint(node, rotTime, someValue):
                logging.warning('Failed to add rotation tags for block ' + str(block.getName()))
            if not vrAnimWidgets.addScaleControlPoint(node, scaleTime, someValue):
                logging.warning('Failed to add scale tags for block ' + str(block.getName()))
            if not vrAnimWidgets.addVisibleControlPoint(node, visibleTime, 1.0):
                logging.warning('Failed to add visible tags for block ' + str(block.getName()))

            # Move them back to the block so that they will be exported modified
            for dsCurve in dsBlock.ChildCurveNamesToCurves.values():
                vrScenegraph.moveNode(dsCurve.AnimCurve, animRoot, block)

    return library

