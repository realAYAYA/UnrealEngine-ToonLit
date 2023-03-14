from PySide2 import QtCore, QtGui, QtWidgets
from PySide2.QtWidgets import QMessageBox

import sys
import os
import logging
import xml.etree.ElementTree as ET
from collections import defaultdict
from itertools import chain

import vrFileIO
import vrFileDialog
import vrVariantSets
import vrMaterialPtr
import vrScenegraph
import vrNodeUtils
import vrFieldAccess
import vrController
import vrOptimize
import vrNodePtr
import vrOSGWidget
import vrCamera

# Adds the ScriptPlugins folder to path so that we can find the other files
# in our module
scriptPluginsFolder = os.path.dirname(os.path.realpath(os.curdir))
if scriptPluginsFolder not in sys.path:
    sys.path.append(scriptPluginsFolder)

import DatasmithExporter.utils as dsutils
import DatasmithExporter.anim_exporter as dsanim
import DatasmithExporter.sanitizer as dss
import DatasmithExporter.mat as dsmat

# Current plugin version
exporterVersion = "4"

class Light:
    def __init__(self):
        self.name = ""
        self.type = ""
        self.enabled = True
        self.useTemperature = False
        self.temperature = 0.0
        self.visualizationVisible = True
        self.intensity = 0.0
        self.glossyColor = []
        self.diffuseColor = []
        self.coneAngle = 0.0
        self.penumbraAngle = 0.0
        self.areaLightUseConeAngle = False
        self.attenuationType = 0
        self.unit = 0
        self.useIESProfile = False
        self.IESPath = ""

class UserSettings:
    def __init__(self):
        self.animBaseTime = 24.0
        self.animPlayRate = 24.0

    def tryFetchFromFile(self):
        try:
            appdata = os.getenv('APPDATA')
            path = os.path.join(appdata, r"VREDPro\\preferences.xml")

            if os.path.exists(path):
                settingsFile = ET.parse(path)
                settingsRoot = settingsFile.getroot()

                timeNodes = settingsRoot.findall('./key[@name="Animation time"]')
                self.animBaseTime = float(timeNodes[0].text)

                playbackRateNodes = settingsRoot.findall('./key[@name="Animation playback speed"]')
                playbackMode = int(playbackRateNodes[0].text)
                if playbackMode == 0:  # Every Frame ~ 60fps
                    self.animPlayRate = 60.0
                elif playbackMode == 1:  # Film
                    self.animPlayRate = 24.0
                elif playbackMode == 2:  # PAL
                    self.animPlayRate = 25.0
                elif playbackMode == 3:  # NTSC
                    self.animPlayRate = 30.0
                else:  # Custom
                    customRateNodes = settingsRoot.findall('./key[@name="Animation playback custom speed"]')
                    self.animPlayRate = float(customRateNodes[0].text)

                logging.info('Found preferences file "'  + str(path) + '"')
                logging.info('Read animation base time of ' + str(self.animBaseTime) + ' fps')
                logging.info('Read animation playback speed of ' + str(self.animPlayRate) + ' fps')

        except BaseException as e:
            self.animBaseTime = 24.0
            self.animPlayRate = 24.0
            logging.warning('Problem accessing preferences file: "' + repr(e) + '"')
            logging.info('Set animation base time to default of 24.0 fps')
            logging.info('Set animation playback speed to default of 24.0 fps')


class DatasmithFBXExporter():
    def __init__(self, mainWindow):
        self.mainWindow = mainWindow

    def getMaterialsToPreserve(self):
        """
        If an object has a Switch Material currently set to MaterialA, when exported to the
        FBX it will be written as simply just having MaterialA. This means that the unselected variants
        of a Switch Material may not be used by any node, which causes them to not be packed into the
        FBX at all. This function identifies these unused material variants and returns a list with
        the material pointers

        Returns:
            (list of vrMaterialPtr): List of vrMaterialPtr of materials to preserve
        """
        # Go over all switch materials and keep track of the IDs of
        # submaterials that are not currently chosen
        idsOfMatsToPreserve = []
        for mat in vrMaterialPtr.getAllMaterials():
            if mat.getNodes() == 0:
                continue

            submats = mat.getSubMaterials()
            if len(submats) > 1:
                choice = mat.getChoice()
                if choice is None or choice >= len(submats):
                    logging.warning('Found invalid choice of submaterial "' + str(choice) + '" for material ' + str(mat.getName()))
                    continue

                chosenId = submats[choice].getID()
                for submat in submats:
                    if submat.getID() != chosenId:
                        idsOfMatsToPreserve.append(submat.getID())

        # Go over the list again discarding all materials that are used by every switch.
        # This because a material that is unused by one switch might be used by another, and so
        # doesn't actually need to be preserved
        for mat in vrMaterialPtr.getAllMaterials():
            if mat.getNodes() == 0:
                continue

            submats = mat.getSubMaterials()

            # If its a switch material, try removing its selected variant from the list
            if len(submats) > 1:
                choice = mat.getChoice()
                if choice is None or choice >= len(submats):
                    continue
                
                chosenId = submats[choice].getID()
                try:
                    idsOfMatsToPreserve.remove(chosenId)
                except ValueError:
                    pass

            # If its a regular material used by an active node, try removing it from the list
            # since it will be packed in the FBX already
            else:
                try:
                    for node in mat.getNodes():
                        if node.getActive():
                            idsOfMatsToPreserve.remove(mat.getID())
                except ValueError:
                    pass

        return [vrMaterialPtr.toMaterial(matId) for matId in idsOfMatsToPreserve]

    def assignMatsToTempGeometry(self, mats):
        """
        For each vrMaterialPtr in mats, creates a temp plane and assigns the material to it,
        returning a list of all vrNodePtr that it created.

        Args:
            mats (list of vrMaterialPtr): Materials to assign to temp geometry

        Returns:
            List of vrNodePtr containing the temp geometry created
        """
        newNodes = []
        for mat in mats:
            newNode = vrNodeUtils.createPlane(1, 1, 1, 1, 0, 0, 0)
            newNode.setName("__temp_" + str(id(mat)))  # Use python object ID for uniqueness
            newNode.setActive(True)
            newNode.setMaterial(mat)
            newNodes.append(newNode)

        # Disable in scene graph and hide in perspective view
        # will still be briefly visible in the graph though
        vrScenegraph.hideNodes(newNodes)

        return newNodes

    def injectSwitchMaterialUsers(self, xmlRoot):
        """
        If an object has a Switch Material currently set to MaterialA, when exported to the
        FBX it will be written as simply just having MaterialA. We want to preserve that
        these objects had a switch material, so we'll find the Switch Material entry in the
        variants xml tree and inject the name of the nodes that use them with <UsedBy name="nodeName"/>
        tags

        Assumes that switch materials have already been renamed in the variants file

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
        """
        for matNode in xmlRoot.iterfind('.//MaterialVariant'):
            # 'base' is the actual material name
            materialName = matNode.get('base')

            logging.debug('Injecting users for switchmat variant referencing material ' + str(materialName))

            matPtrs = vrMaterialPtr.findMaterials(materialName)
            if len(matPtrs) != 1:
                logging.warning('Found ' + str(len(matPtrs)) + ' switch materials with name ' + str(materialName) + ' when injecting users!')

            if len(matPtrs) == 0:
                continue

            mat = matPtrs[0]
            for node in mat.getNodes():
                logging.debug('\t\tinjecting user ' + str(node.getName()))
                ET.SubElement(matNode, 'UsedBy', {'name':node.getName()})

    def extractTransformVariants(self):
        """
        Uses the vrFileAccess module to extract TransformVariants from nodes that have them.
        It will return a dictionary of node names to other dictionaries, these in turn containing
        the actual transform variants keyed by their names. Example:

        {'BoxNode': {'Variant1': [1, 0, 0, ..., 1], 'Variant2': [1, 2, ..., 1]},
         'SphereNode': {'Variant1': [1, 0, 0, ..., 1]}}

        Returns:
            (dict): Keys are node names. Each value is a dict, where variant names are keys, and
                    values are float arrays describing the actual transform variants
        """
        nodes = vrScenegraph.getAllNodes()

        unitScale = self.getUnitScale()

        nodeNamesToTransVars = {}
        for node in nodes:
            nodeName = node.getName()

            # Some objects have the TransformVariantsAttachment attached directly to them, some have it
            # attached to their transform field container
            if node.fields().hasField('transform'):
                transformContainer = vrFieldAccess.vrFieldAccess(node.fields().getFieldContainer('transform'))
            else:
                transformContainer = node.fields()

            # Carefully drill down to the transform variant data
            if transformContainer.isValid() and transformContainer.hasAttachment('TransformVariantsAttachment'):
                variantsAttachment = transformContainer.getAttachment('TransformVariantsAttachment')
                variantsAttachment = vrFieldAccess.vrFieldAccess(variantsAttachment)

                if variantsAttachment.isValid() and variantsAttachment.hasField('transformVariants'):
                    variantsList = variantsAttachment.getMFieldContainer('transformVariants')

                    # Iterate over all TransformVariants that this node has
                    for variant in variantsList:
                        var = vrFieldAccess.vrFieldAccess(variant)

                        transformationContainer = vrFieldAccess.vrFieldAccess(var.getFieldContainer('transformation'))

                        # Pack transform data in output dictionary
                        transName = var.getString('name')
                        transform = self.processTransform(transformationContainer.getMatrix('matrix'), unitScale)

                        if nodeName not in nodeNamesToTransVars:
                            nodeNamesToTransVars[nodeName] = defaultdict(list)

                        nodeNamesToTransVars[nodeName][transName].append(transform)

        return nodeNamesToTransVars

    def processTransform(self, trans, scale):
        """
        Flip and transpose the transform so it can be used directly in UnrealEditor. Also, scales
        the translation by the unit scale, which is something that's easier to do here than
        in the UnrealEditor importer.

        Args:
            trans (list of 16 floats): Describes the transform to process
            scale (float): Scale unit factor, where 1.0 means mm, 10.0 means cm, 1000.0 means m, etc

        Returns:
            (list of 16 floats): Processed transform
        """
        assert(len(trans) == 16)

        # Transpose transform
        transposed = [0]*16
        index = 0
        for row in range(0, 4):
            for col in range(0, 4):
                transposed[index] = trans[col*4 + row]
                index += 1

        # Flip around the Y plane
        for pos in range(0, 4):
            transposed[pos*4 + 1] *= -1
        for pos in range(0, 4):
            transposed[4 + pos] *= -1

        # # Scale translation by unit
        multiplier = scale / 10
        for pos in range(0, 3):
            transposed[12 + pos] *= multiplier

        return transposed

    def injectTransformVariants(self, xmlRoot, nodeNamesToTransVars):
        """
        Injects the actual transform float arrays in the variants file describing transform
        variants.

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
            nodeNamesToTransVars (dict): Keys are node names. Each value is a dict, where
                                         variant names are keys, and values are float arrays
                                         describing the actual transform variants
        """
        # We'll use try/except KeyError below, so its better to make sure its not a defaultdict
        nodeNameToTransVarsDict = dict(nodeNamesToTransVars)

        logging.debug('Injecting transform variants ' + str(nodeNameToTransVarsDict))

        for transVarXmlNode in xmlRoot.iterfind('./TransformVariant'):
            nodeName = transVarXmlNode.get('base')
            for state in transVarXmlNode.findall('State'):
                stateName = state.get('name')

                logging.debug('\tAnalyzing transformVariants for node ' + str(nodeName) + ', state ' + str(stateName))

                if nodeName in nodeNamesToTransVars:
                    if stateName in nodeNamesToTransVars[nodeName]:
                        transVars = nodeNamesToTransVars[nodeName][stateName]

                        state.set('value', str(transVars[0]))
                        logging.debug('\tinjecting ' + str(transVars[0]) + ' to state ' + str(stateName) + ' of transVar of node ' + str(nodeName))
                        del transVars[0]

                        # If we have multiple transform variant nodes, VRED won't have inserted an extra line in
                        # the variants file, let's fix that since its easy
                        while len(transVars) != 0:
                            ET.SubElement(transVarXmlNode, 'State', {'name':stateName, 'value':str(transVars[0])})
                            logging.debug('\tinjecting additional transVar ' + str(transVars[0]) + ' to state ' + str(stateName) + ' of transVar of node ' + str(nodeName))
                            del transVars[0]
                    else:
                        if stateName not in ['!Previous', '!Next', '!Previous (Loop)', '!Next (Loop)']:
                            logging.warning('Did not find transform for transVar state ' + str(stateName) + ' of node ' + str(nodeName))
                else:
                    logging.warning('Did not find transform of node ' + str(nodeName))

    def sanitizeTransformVariantStateNames(self, xmlRoot, transVariants):
        """
        Sanitizes the names of transform variant states, updating the default to match

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
        """
        commandNames = set(['!Next', '!Next (Loop)', '!Previous', '!Previous (Loop)'])

        renamedNodeVariantStates = defaultdict(dict)
        for transVarXmlNode in xmlRoot.iterfind('./TransformVariant'):
            nodeName = transVarXmlNode.get('base')
            logging.debug('Analyzing transform variant state names for node ' + str(nodeName))

            adapters = [dss.XMLNodeAdapter(xmlNode) for xmlNode in transVarXmlNode.findall('State') if xmlNode.get('name') not in commandNames]
            adapterDict = {ad.getID(): ad for ad in adapters}

            # Rename states
            # Note how since we're using a dict here two states with the same name will shadow eachother,
            # and our varset refs will end up all pointing at the same one.
            # That is fine though, since they wouldn't have worked fine in VRED anyway
            renamedAdapters = dss.renameObjectsAndKeepOriginals(adapters)
            stateOldToNewNames = {orig: adapterDict[adId].getName() for adId, orig in dsutils.dict_items_gen(renamedAdapters)}
            logging.debug('\trenamed states: ' + str(stateOldToNewNames))

            # Keep track of renamed names so that we can update varset references
            transVarName = transVarXmlNode.get('name')
            renamedNodeVariantStates[transVarName] = stateOldToNewNames

            # Rename default state
            for defNode in transVarXmlNode.findall('Default'):
                stateName = defNode.get('ref')

                if stateName in stateOldToNewNames:
                    newStateName = stateOldToNewNames[stateName]
                    defNode.set('ref', newStateName)
                    logging.debug('\trenaming default state from  ' + str(stateName) + ' to ' + str(newStateName))

        # Rename references to node variants from variant sets
        for xmlNode in xmlRoot.iterfind(".//TransformRef"):
            maybeRenamedRef = xmlNode.get('ref')
            oldStateName = xmlNode.get('state')

            logging.debug('Analyzing TransformRef ' + str(maybeRenamedRef))

            if maybeRenamedRef in renamedNodeVariantStates:
                renamedStates = renamedNodeVariantStates[maybeRenamedRef]

                if oldStateName in renamedStates:
                    newStateName = renamedStates[oldStateName]
                    logging.debug('\tstate ' + str(oldStateName) + ' seems to have been renamed to ' + str(newStateName))
                    xmlNode.set('state', newStateName)

    def fixupXmlContents(self, root):
        """ Works around a bug in UnrealEditor by ensuring that element contents that spans multiple lines end in a newline

        Without this pass, an xml file like this:
        <A>
        e"</A>
        Will not be able to be parsed by UnrealEditor's XmlParser

        Args:
            root (xml.etree.Element): Root of the xml subtree that will be fixed        
        """
        for element in root.iter():
            if element.text and element.text.strip().count('\n') > 0:
                element.text += '\n'

    def extractLights(self):
        """
        Extract all lights from the current scene, packs each into a Light object
        and returns a list of them

        Returns:
            (list of Light objects): List of lights in the scene
        """
        lights = []
        nodes = vrScenegraph.getAllNodes()
        for node in nodes:
            if node.isLight():
                light = Light()
                light.name = node.getName()
                light.worldTransform = node.getWorldTransform()

                if node.isSpotLight():
                    light.type = "spotlight"
                elif node.isSphericalLight():
                    light.type = "spherical"
                elif node.isRectangularLight():
                    light.type = "rectangular"
                elif node.isPointLight():
                    light.type = "point"
                elif node.isDiskLight():
                    light.type = "disk"
                elif node.isDirectionalLight():
                    light.type = "directional"
                elif node.isAreaLight():
                    light.type = "area"

                light.enabled = node.isLightOn()
                light.useTemperature = node.getUseLightTemperature()
                light.temperature = node.getLightTemperature()
                light.visualizationVisible = node.getLightVisualizationVisible()
                light.intensity = node.getLightIntensity()
                light.glossyColor = node.getGlossyLightColor()
                light.diffuseColor = node.getDiffuseLightColor()
                light.coneAngle = node.getLightConeAngle()
                light.penumbraAngle = node.getLightPenumbraAngle()
                light.areaLightUseConeAngle = node.getAreaLightUseConeAngle()
                light.useIESProfile = node.getUseIESProfile()

                # Extract attenuationType using the fieldaccess API
                try:
                    if node.fields().hasField('attenuationType'):
                        light.attenuationType = node.fields().getUInt32('attenuationType')
                    else:
                        light.attenuationType = 0
                except AttributeError:
                    light.attenuationType = 0

                # Extract unit using the fieldaccess API
                try:
                    if node.fields().hasField('unit'):
                        light.unit = node.fields().getUInt32('unit')
                    else:
                        light.unit = 0
                except AttributeError:
                    light.unit = 0

                # Extract IES filename using the fieldaccess API
                try:
                    fc = vrFieldAccess.vrFieldAccess(node.fields().getFieldContainer('iesProfile'))
                    if fc.isValid():
                        light.IESPath = fc.getString('filename')
                    else:
                        light.IESPath = ""
                except AttributeError:
                    light.IESPath = ""

                lights.append(light)
        return lights

    def writeLightsFile(self, filename, lights):
        """
        Creates a file with 'filename' and repeatedly calls writeXMLTag to write all the
        lights in the 'lights' array to the XML file

        Args:
            filename (str): Full path of the file to write
            lights (list of Light objects): List of lights to write to the file
        """

        with open(filename, 'w') as lightsfile:
            dsutils.writeXMLTag(lightsfile, 0, 'Lights', [], [])

            for light in lights:
                dsutils.writeXMLTag(lightsfile, 1, 'Light', ['name'], [light.name])

                dsutils.writeXMLTag(lightsfile, 2, 'WorldTransform', ['value'], [light.worldTransform], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'Type', ['value'], [light.type], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'Enabled', ['value'], [light.enabled], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'UseTemperature', ['value'], [light.useTemperature], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'Temperature', ['value'], [light.temperature], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'Intensity', ['value'], [light.intensity], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'GlossyColor', ['value'], [light.glossyColor], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'DiffuseColor', ['value'], [light.diffuseColor], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'ConeAngle', ['value'], [light.coneAngle], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'PenumbraAngle', ['value'], [light.penumbraAngle], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'AreaLightUseConeAngle', ['value'], [light.areaLightUseConeAngle],
                                 close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'VisualizationVisible', ['value'], [light.visualizationVisible],
                                 close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'AttenuationType', ['value'], [light.attenuationType], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'Unit', ['value'], [light.unit], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'UseIESProfile', ['value'], [light.useIESProfile], close=True)
                dsutils.writeXMLTag(lightsfile, 2, 'IESPath', ['value'], [light.IESPath], close=True)

                dsutils.writeXMLTag(lightsfile, 1, '/Light', [], [])

            dsutils.writeXMLTag(lightsfile, 0, '/Lights', [], [])

    def writeTextures(self, folder, mats):
        """
        Writes all textures in 'mats' as png files within folder, also updates tex objects
        with the exported filepath

        Args:
            folder (str): Full path folder to contain all pngs (e.g. C:/TextureFolder)
            mats (list of Mat objects): List of mats to write to the file
        """
        exportedTextures = {}

        for mat in mats:
            for texName in mat.textures:
                tex = mat.textures[texName]

                srcName = os.path.split(tex.srcPath)[1]
                srcName = os.path.splitext(srcName)[0]
                if len(srcName) == 0:
                    srcName = str(mat.vredMatId) + str(texName)
                filename = os.path.join(folder, srcName)
                filename += '_' + str(tex.texType).replace('Component', '')

                # If we already have a texture from the same file used in the same way,
                # give up. The way it is used is important as VRED does some processing for
                # displacement/normal maps
                # Disregard how this will be like 'file.pngglossyColor', this is just a key
                usageKey = str(tex.srcPath) + str(tex.texType)
                if usageKey in exportedTextures:
                    tex.exportedPath = exportedTextures[usageKey]
                    continue

                arr = bytearray(tex.pixelData)

                # If it's invalid we'll just dump the data directly. Some environment/hdr formats
                # are RGBA64 for example, which this version of Qt doesn't support
                imgFormat = dsutils.getQImageFormat(tex.bytesPerPixel, tex.channels)
                if imgFormat == QtGui.QImage.Format_Invalid or tex.dataType != 5121:
                    filename += '.tex'

                    logging.warning('Texture "' + str(srcName) + '" has an unsupported format (width: ' + str(tex.width) + ', height: ' + str(tex.height) + ', bytesPerPixel: ' + str(tex.bytesPerPixel) + ', pixelFormat: ' + str(tex.pixelFormat) + ', dataType: "' + str(tex.dataType) + '"), so it will be dumped in raw binary format to "' + str(filename) + '". It will have no extra padding per row.')

                    with open(filename, 'wb') as f:
                        f.write(arr)
                else:
                    filename += '.png'
                    img = QtGui.QImage(arr, tex.width, tex.height, tex.stride, imgFormat)
                    img.save(filename, "PNG", 100)

                if os.path.exists(filename):
                    tex.exportedPath = filename
                    exportedTextures[usageKey] = filename

    def writeMatFile(self, filename, mats):
        """
        Creates a file with 'filename' and repeatedly calls writeXMLTag to write all the
        mats in the 'mats' array to the XML file

        Args:
            filename (str): Full path of the file to write
            mats (list of Mat objects): List of mats to write to the file
        """

        with open(filename, 'w') as f:
            dsutils.writeXMLTag(f, 0, 'Mats', [], [])

            for mat in mats:
                dsutils.writeXMLTag(f, 1, 'Mat', ['name', 'matType', 'vredMatId'], [mat.name, mat.matType, mat.vredMatId])

                dsutils.writeXMLTag(f, 2, 'Users', [], [])
                for user in mat.users:
                    dsutils.writeXMLTag(f, 3, 'User', ['name'], [user], close=True)
                dsutils.writeXMLTag(f, 2, '/Users', [], [])

                dsutils.writeXMLTag(f, 2, 'Submaterials', [], [])
                for submat in mat.submaterials:
                    dsutils.writeXMLTag(f, 3, 'Submaterial', ['name'], [submat], close=True)
                dsutils.writeXMLTag(f, 2, '/Submaterials', [], [])

                dsutils.writeXMLTag(f, 2, 'Properties', [], [])
                for prop in mat.properties:
                    dsutils.writeXMLTag(f, 3, prop, ['value'], [mat.properties[prop]], close=True)
                dsutils.writeXMLTag(f, 2, '/Properties', [], [])

                dsutils.writeXMLTag(f, 2, 'Textures', [], [])
                for texName in mat.textures:
                    tex = mat.textures[texName]
                    dsutils.writeXMLTag(f, 3, 'Tex', ['exportedPath', 'srcPath', 'texType', 'width', 'height', 'bytesPerPixel', 'channels', 'dataType', 'stride'], [tex.exportedPath, tex.srcPath, tex.texType, tex.width, tex.height, tex.bytesPerPixel, tex.channels, tex.dataType, tex.stride])

                    dsutils.writeXMLTag(f, 4, 'Properties', [], [])
                    for texProp in tex.properties:
                        dsutils.writeXMLTag(f, 5, texProp, ['value'], [tex.properties[texProp]], close=True)
                    dsutils.writeXMLTag(f, 4, '/Properties', [], [])

                    dsutils.writeXMLTag(f, 3, '/Tex', [], [])

                dsutils.writeXMLTag(f, 2, '/Textures', [], [])

                dsutils.writeXMLTag(f, 1, '/Mat', [], [])

            dsutils.writeXMLTag(f, 0, '/Mats', [], [])

    def removePreviewTags(self, xmlRoot):
        """
        VRED encodes variant set preview thumbnails in base64 in the variants file itself.
        Those are massive, unused and just hang the re evaluation, so this function removes them.

        For performance, make sure this is called as soon as you read back the variants file.

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
        """
        for node in xmlRoot.iterfind(".//Preview/.."):
            for prev in node.iterfind('Preview'):
                node.remove(prev)

    def sanitizeVariantNamesAndVariantSetRefs(self, xmlRoot):
        """
        Sanitizes names of variants and variant sets directly on the xml tree.
        We will only sanitize variant name and variant set ref fields, since 'state' and 'base'
        fields correspond to actual nodes and material nodes, sanitized and renamed elsewhere

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
        """
        allVariantNodes = []

        # This isn't so bad since we only ever go one level down anyway
        allVariantNodes += xmlRoot.findall("./NodeVariant")
        allVariantNodes += xmlRoot.findall("./MaterialVariant")
        allVariantNodes += xmlRoot.findall("./LightVariant")
        allVariantNodes += xmlRoot.findall("./TransformVariant")
        allVariantNodes += xmlRoot.findall("./VariantSet")
        allVariantNodes += xmlRoot.findall("./VariantGroup/VariantSet")

        # Use adapters so that we can reuse our sanitizing function
        variantNodeAdapters = [dss.XMLNodeAdapter(xmlNode) for xmlNode in allVariantNodes]

        # We'll sanitize all of them together since they will become rows in a datatable in Unreal, meaning
        # they must have unique names even between different types of variants
        renamedVariants = dss.renameObjectsAndKeepOriginals(variantNodeAdapters)

        # Group all adapters by originalName
        origNamesToAdapters = defaultdict(list)
        for adapter in variantNodeAdapters:
            adapterID = adapter.getID()
            if adapterID in renamedVariants:
                origName = renamedVariants[adapterID]
                origNamesToAdapters[origName].append(adapter)
        origNamesToAdapters = dict(origNamesToAdapters)

        logging.debug('Found the renamed variant references ' + str(origNamesToAdapters))

        for variantRefNode in xmlRoot.iterfind(".//VariantSet/*[@ref]"):
            oldRef = variantRefNode.get('ref')
            refTag = variantRefNode.tag.replace('Ref', '').replace('Variant', '')  # Node, Material, Transform, etc

            if oldRef in origNamesToAdapters:
                adapters = origNamesToAdapters[oldRef]

                for adapter in adapters:
                    variantType = adapter.getType()
                    variantTypePruned = variantType.replace('Variant', '')  # Also Node, Material, Transform, etc

                    logging.debug('\tcomparing ref type ' + str(refTag) + ' with var type ' + str(variantTypePruned))

                    if refTag == variantTypePruned:
                        newRef = adapter.getName()
                        variantRefNode.set('ref', newRef)
                        logging.debug('\tmatch! renaming ' + str(oldRef) + ' to ' + str(newRef))

    def unshareNodes(self):
        """
        Unshares all cloned and transformable roots in the scene. This is not an undoable operation.
        """
        # Every time we unshare a node, *something* happens to the scenegraph that invalidates
        # all the references and vrNodePtr that have to other nodes. For some reason, we need
        # to call vrScenegraph.getAllNodes() and get new references, or else we end up with invalid/broken nodes
        # Particularly, unshared nodes seem to not each get a copy of the cloned 'Name' attachment, so when
        # we call .getName() we get a full crash

        def stillHaveClones():
            """
            Gets the first shared node in the scene that it finds, then climbs the hierarchy to find the
            root of the cloned subtree and unshares it.

            Returns:
                (bool): False if there are no more shared nodes in the scenegraph
            """
            try:
                aClone = next(node for node in vrScenegraph.getAllNodes() if dsutils.isSharedNode(node))
            except StopIteration:
                return False

            parent = aClone.getParent()
            while parent != None and parent.isValid() and parent != aClone and dsutils.isSharedNode(parent):
                aClone = parent
                parent = aClone.getParent()

            vrScenegraph.unshareNode(aClone)
            return True

        while stillHaveClones():
            pass

    def renameNodesAndKeepOriginals(self):
        """
        Sanitize the names of all nodes in the scene, returning a dict that points
        renamed nodes to their original names so that they can be later restored.

        Returns:
            (dict of id to str): Dictionary with node ids and their original names
        """
        return dss.renameObjectsAndKeepOriginals(vrScenegraph.getAllNodes())

    def renameMatsAndKeepOriginals(self):
        """
        Sanitize the names of all materials in the scene, returning a name that points
        renamed materials to their original names so that they can be restored.

        Returns:
            (dict of id to str): Dictionary with material ids and their original names
        """
        return dss.renameObjectsAndKeepOriginals(vrMaterialPtr.getAllMaterials())

    def renameNodesInVariantsFile(self, xmlRoot, nodeIDToOriginalNames, transVariants):
        """
        Uses the received dicts to update the variants xml tree in place with the names of nodes
        that have been renamed. This includes NodeVariants, LightVariants and TransformVariants,
        as well as NodeVariant states. Also updates VariantSet states to match.

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
            nodeIDToOriginalNames (dict of id to str): Dictionary with node ids and their original names
            transVariants (dict): Keys are node names. Each value is a dict, where variant names are keys, and
                                  values are float arrays describing the actual transform variants
        """
        # Invert map, but consider that there might be multiple nodes with the same origName
        origNameToNodeIDs = defaultdict(list)
        logging.debug('nodeToOriginalNames:')
        for nodeID, origName in dsutils.dict_items_gen(nodeIDToOriginalNames):
            logging.debug('\tnode: ' + str(vrNodePtr.vrNodePtr(nodeID).getName()) + ', origName: ' + str(origName))
            origNameToNodeIDs[origName].append(nodeID)

        # We don't need the defaultdict behaviour anymore, so lets prevent some mistakes
        origNameToNodeIDs = dict(origNameToNodeIDs)

        logging.debug('origNamesToNodes:')
        for origName, nodeIDs in dsutils.dict_items_gen(origNameToNodeIDs):
            logging.debug('\torigName: ' + str(origName) + ', nodes: ' + str([vrNodePtr.vrNodePtr(id).getName() for id in nodeIDs]))

        # Ignore these states
        commandNames = set(['!All', '!Next', '!Next (Loop)', '!None', '!Previous', '!Previous (Loop)', '!Enable', '!Disable', '!Toggle'])

        # Rename light variant nodes
        for xmlNode in xmlRoot.iterfind(".//LightVariant"):
            lightNameVar = xmlNode.get('base')
            logging.debug('Analyzing LightVariant ' + str(lightNameVar))
            if lightNameVar in origNameToNodeIDs:
                # Not much we can do since they don't have States for each child.
                # "Getting the first node with the name" mirrors what VRED does anyway though
                nodeID = origNameToNodeIDs[lightNameVar][0]
                node = vrNodePtr.vrNodePtr(nodeID)
                newName = node.getName()
                logging.debug('\tRenaming ' + str(lightNameVar) + ' to ' + str(newName))
                xmlNode.set('base', newName)

        # Renames transform variants by comparing variant names if we must
        # Note that we will sanitize the Transform variant names later, as these don't need any
        # special connection with the fbx file. Both the name and the transform are in the xml
        for xmlNode in xmlRoot.iterfind(".//TransformVariant"):
            transNameVar = xmlNode.get('base')
            logging.debug('Analyzing TransformVariant ' + str(transNameVar))

            # The node of this transform variant hasn't been renamed
            if transNameVar not in origNameToNodeIDs:
                continue

            renamedNodeIDList = origNameToNodeIDs[transNameVar]
            numRenamedNodes = len(renamedNodeIDList)

            if numRenamedNodes == 1:
                node = vrNodePtr.vrNodePtr(renamedNodeIDList[0])
                newName = node.getName()
                logging.debug('\tonly one node renamed away from ' + str(transNameVar) + ': ' + str(newName))
                xmlNode.set('base', newName)
            elif numRenamedNodes > 1:
                states = [stateNode.get('name') for stateNode in xmlNode.iterfind('State')]
                states = [state for state in states if state not in commandNames]
                states.sort()

                logging.debug('\tmore than one node renamed away from ' + str(transNameVar))
                logging.debug('\tstates: ' + str(states))

                foundNode = False
                for renamedNodeID in renamedNodeIDList:
                    renamedNode = vrNodePtr.vrNodePtr(renamedNodeID)
                    if renamedNode.getName() in transVariants:
                        nodeTransVariants = transVariants[renamedNode.getName()]
                        nodeTransVariants = nodeTransVariants.keys()

                        logging.debug('\t\tcomparing against ' + str(renamedNode.getName()))
                        logging.debug('\t\tchildren: ' + str(nodeTransVariants))

                        if set(states) == set(nodeTransVariants):
                            logging.debug("\t\tthat's it! they both have the same children")
                            foundNode = True
                            newName = renamedNode.getName()
                            xmlNode.set('base', newName)
                            break

                if not foundNode:
                    logging.warning('Did not find a match for TransformVariant ' + str(transNameVar))

        # Renames node variant switches by trying to identify them by name and children
        # Note that we rename the 'base' attribute only. 'name' is the actual name of the variant
        # and is sanitized separately, since it needs to match potential variant set references
        renamedNodeVariants = {}
        for xmlNode in xmlRoot.iterfind(".//NodeVariant"):
            parentNameVar = xmlNode.get('base')
            logging.debug('Analyzing NodeVariant ' + str(parentNameVar))

            # The geometry switch node of this node variant hasn't been renamed
            if parentNameVar not in origNameToNodeIDs:
                continue

            renamedNodeIDList = origNameToNodeIDs[parentNameVar]
            numRenamedNodes = len(renamedNodeIDList)
            # Only one node was renamed away from parentNameVar, so just rename node
            # to its new name
            if numRenamedNodes == 1:
                node = vrNodePtr.vrNodePtr(renamedNodeIDList[0])
                newName = node.getName()

                if newName in renamedNodeVariants.values():
                    logging.warning(str(newName) + ' was already used!')
                renamedNodeVariants[parentNameVar] = newName

                logging.debug('\tonly one node renamed away from ' + str(parentNameVar) + ': ' + str(newName))
                xmlNode.set('base', newName)
            # There are multiple switch nodes that were renamed from parentNameVar.
            # Try to match as best as we can by comparing the node's children with
            # the variant node's 'State' subnodes
            # origNameToNodeIDs is a defaultdict, so it will always create an empty list
            elif numRenamedNodes > 1:
                states = [stateNode.get('name') for stateNode in xmlNode.iterfind('State')]
                states = [state for state in states if state not in commandNames]

                logging.debug('\tmore than one node renamed away from ' + str(parentNameVar))
                logging.debug('\tstates: ' + str(states))

                foundNode = False
                for renamedNodeID in renamedNodeIDList:
                    renamedNode = vrNodePtr.vrNodePtr(renamedNodeID)
                    renamedNodeChildren = dsutils.getChildren(renamedNode)
                    childrenOrigNames = []
                    for child in renamedNodeChildren:
                        if child.getID() in nodeIDToOriginalNames:
                            childrenOrigNames.append(nodeIDToOriginalNames[child.getID()])
                        else:
                            childrenOrigNames.append(child.getName())

                    logging.debug('\t\tcomparing against ' + str(renamedNode.getName()))
                    logging.debug('\t\tchildren: ' + str(childrenOrigNames))

                    if set(states) == set(childrenOrigNames):
                        logging.debug("\t\tthat's it! they both have the same children")
                        foundNode = True
                        newName = renamedNode.getName()

                        if newName in renamedNodeVariants.values():
                            logging.warning(str(newName) + ' was already used!')
                        renamedNodeVariants[parentNameVar] = newName

                        xmlNode.set('base', newName)
                        break

                if not foundNode:
                    logging.warning('Did not find a match for NodeVariant ' + str(parentNameVar))

        # Rename node variant states using the recently renamed parents
        # Note that we couldn't do that in the above loop since we had no way of knowing which parent
        # was which, but we could use direct node object references for the children there, which removes
        # ambiguities. Now that we know the parent and they have unique names, we can easily use that to rename
        # duplicate child states (especially if they are duplicate between different parents)
        renamedNodeVariantStates = defaultdict(dict)
        for xmlNode in xmlRoot.iterfind(".//NodeVariant"):
            parentName = xmlNode.get('base')

            # Pairs all 'Default' nodes with the 'ref' statenamekey and 'State' nodes with the 'name' statenamekey
            for state, stateNameKey in chain(dsutils.zip_longest(xmlNode.findall('Default'), [], fillvalue='ref'), \
                                             dsutils.zip_longest(xmlNode.findall('State'), [], fillvalue='name')):
                stateName = state.get(stateNameKey)
                logging.debug('Analyzing NodeVariant state ' + str(stateName) + ' from parent ' + str(parentName))

                # The node this state refers to hasn't been renamed
                if stateName not in origNameToNodeIDs:
                    continue

                renamedNodeIDList = origNameToNodeIDs[stateName]

                logging.debug('\tnodes that were renamed away from ' + str(stateName) + ': ' + str([vrNodePtr.vrNodePtr(n).getName() for n in renamedNodeIDList]))

                foundNode = False
                for renamedNodeID in renamedNodeIDList:
                    renamedNode = vrNodePtr.vrNodePtr(renamedNodeID)
                    renamedParentNode = renamedNode.getParent().getName()
                    logging.debug('\t' + str(renamedNode.getName()) + ' has parent ' + str(renamedParentNode))
                    if renamedParentNode == parentName:
                        foundNode = True
                        newName = renamedNode.getName()
                        logging.debug("\tthat's it, renaming " + str(stateName) + ' to ' + str(newName))

                        nodeVariantName = xmlNode.get(stateNameKey)
                        renamedNodeVariantStates[nodeVariantName][stateName] = newName

                        state.set(stateNameKey, newName)
                        break

                if not foundNode:
                    logging.warning('Did not find a match for NodeVariant state ' + str(stateName) + ' for NodeVariant ' + str(parentName))

        # Rename references to node variants from variant sets
        for xmlNode in xmlRoot.iterfind(".//NodeVariantRef"):
            maybeRenamedRef = xmlNode.get('ref')
            oldStateName = xmlNode.get('state')

            logging.debug('Analyzing NodeVariantRef ref ' + str(maybeRenamedRef))

            if maybeRenamedRef in renamedNodeVariantStates:
                renamedStates = renamedNodeVariantStates[maybeRenamedRef]

                if oldStateName in renamedStates:
                    newStateName = renamedStates[oldStateName]
                    logging.debug('\tstate ' + str(oldStateName) + ' seems to have been renamed to ' + str(newStateName))
                    xmlNode.set('state', newStateName)

    def renameClipsInVariantsFile(self, xmlRoot, clipOrigNamesToNewNames):
        """
        Uses the passed in dictionary to update references to a renamed animation clip in the variants xml tree

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
            clipOrigNamesToNewNames (dict of str to str): Dictionary with clips original names mapping to their new names
        """
        # In case of clips with duplicate names, our hands are tied since its impossible
        # to get which clip the variant set used via anything other than its name string
        for xmlNode in xmlRoot.iterfind('.//AnimationRef'):
            oldRef = xmlNode.get('ref')
            logging.debug('Analyzing AnimationRef ' + str(oldRef))
            if oldRef in clipOrigNamesToNewNames:
                newRef = clipOrigNamesToNewNames[oldRef]
                logging.debug('\trenaming ' + str(oldRef) + ' to ' + str(newRef))
                xmlNode.set('ref', newRef)

    def renameMaterialsInVariantsFile(self, xmlRoot, matIDsToOriginalNames):
        """
        Uses the passed in dictionary to update references to a renamed material in the variants file

        Args:
            xmlRoot (xml.etree.ElementTree.Element): Root of the variants XML tree
            matIDsToOriginalNames (dict of id to str): Dictionary with material ids and their original names
        """
        # Invert map, but consider that there might be multiple nodes with the same origName
        origNameToMatIDs = defaultdict(list)
        logging.debug('matIDsToOriginalNames:')
        for matID, origName in dsutils.dict_items_gen(matIDsToOriginalNames):
            logging.debug('\tmat: ' + str(vrMaterialPtr.vrMaterialPtr(matID).getName()) + ', origName: ' + str(origName))
            origNameToMatIDs[origName].append(matID)

        # We don't need the defaultdict behaviour anymore, so lets prevent some mistakes
        origNameToMatIDs = dict(origNameToMatIDs)

        logging.debug('origNamesToMatIDs:')
        for origName, matIDs in dsutils.dict_items_gen(origNameToMatIDs):
            logging.debug('\torigName: ' + str(origName) + ', mats: ' + str([vrMaterialPtr.vrMaterialPtr(matID).getName() for matID in matIDs]))

        # Ignore these states
        commandNames = set(['!All', '!Next', '!Next (Loop)', '!None', '!Previous', '!Previous (Loop)', '!Enable', '!Disable', '!Toggle'])

        # Renames material variants by trying to identify them by name and submaterials
        # Note that we rename the 'base' attribute only. 'name' is the display name of the variant
        # and is sanitized separately, since it needs to match potential variant set references
        renamedMatVariants = {}
        for matXmlNode in xmlRoot.iterfind(".//MaterialVariant"):
            switchMatNameVar = matXmlNode.get('base')
            logging.debug('Analyzing MaterialVariant ' + str(switchMatNameVar))

            # This switch material has not been renamed
            if switchMatNameVar not in origNameToMatIDs:
                continue

            renamedMatIDList = origNameToMatIDs[switchMatNameVar]
            numRenamedNodes = len(renamedMatIDList)
            # Only one node was renamed away from switchMatNameVar, so just rename node
            # to its new name
            if numRenamedNodes == 1:
                mat = vrMaterialPtr.vrMaterialPtr(renamedMatIDList[0])
                newName = mat.getName()

                if newName in renamedMatVariants.values():
                    logging.warning(str(newName) + ' was already used!')
                renamedMatVariants[switchMatNameVar] = newName

                logging.debug('\tonly one mat renamed away from ' + str(switchMatNameVar) + ': ' + str(newName))
                matXmlNode.set('base', newName)
            # There are multiple materials that were renamed away from switchMatNameVar.
            # Try to match as best as we can by comparing the mats's submaterials with
            # the variant node's 'State' subnodes
            elif numRenamedNodes > 1:
                # We'll use sets for these comparisons because VRED gets confused if there are multiple
                # submaterials with the same name under a switch material, only showing one in the variants
                # window and in the emitted variants file.
                states = [stateNode.get('name') for stateNode in matXmlNode.iterfind('State')]
                states = [stateNode for stateNode in states if stateNode not in commandNames]
                states = set(states)

                logging.debug('\tmore than one mat renamed away from ' + str(switchMatNameVar))
                logging.debug('\txml node states: ' + str(states))

                foundMat = False
                for renamedMatID in renamedMatIDList:
                    renamedMat = vrMaterialPtr.vrMaterialPtr(renamedMatID)
                    logging.debug('\t\tcomparing against ' + str(renamedMat.getName()))

                    renamedMatSubmats = renamedMat.getSubMaterials()
                    submatOrigNames = set()
                    for submat in renamedMatSubmats:
                        submatID = submat.getID()
                        if submatID in matIDsToOriginalNames:
                            submatOrigNames.add(matIDsToOriginalNames[submatID])
                        else:
                             submatOrigNames.add(submat.getName())
                    logging.debug('\t\tsubmaterial names: ' + str(submatOrigNames))

                    if states == submatOrigNames:
                        logging.debug("\t\tthat's it! they both have the same (non-repeating) submaterials")
                        foundMat = True
                        newName = renamedMat.getName()

                        if newName in renamedMatVariants.values():
                            logging.warning(str(newName) + ' was already used!')
                        renamedMatVariants[switchMatNameVar] = newName

                        matXmlNode.set('base', newName)
                        break

                if not foundMat:
                    logging.warning('Did not find a match for SwitchMaterial ' + str(switchMatNameVar))

        # Rename switch material states using the recently renamed switch materials
        renamedSwitchMaterialStates = defaultdict(dict)
        for switchMatXmlNode in xmlRoot.iterfind(".//MaterialVariant"):
            switchMatName = switchMatXmlNode.get('base')
            logging.debug('Analyzing state nodes for switch material ' + str(switchMatName))

            # We have already removed duplicate material names in the scene and renamed the switch material names
            # in the xml hierarchy, so there should be only one material that has switchMatName
            matPtrs = vrMaterialPtr.findMaterials(switchMatName)
            if len(matPtrs) != 1:
                logging.warning('Found ' + str(len(matPtrs)) + ' switch materials named ' + str(switchMatName) + '. Using the first if we can')

            if len(matPtrs) == 0:
                continue

            submats = matPtrs[0].getSubMaterials()

            # Find out how those submats were renamed, if they ever were
            # Two cases:
            #   Submaterials with the same name but only one appears in this switch material:
            #       We'll cover this by comparing against actual submats of the switch material (which is presumed unique by now)
            #   Submaterials with the same name and more than one appears in this switch material:
            #       We need to have a state for each, so we need a list of tuples here, so that we can have multiple keys
            #       pointing to the same value
            submatOrigNamesToNewNames = []
            for submat in submats:
                submatID = submat.getID()
                if submatID in matIDsToOriginalNames:
                    submatOrigNamesToNewNames.append((matIDsToOriginalNames[submatID], submat.getName()))
            logging.debug('\tsubmaterial old names to new names: ' + str(submatOrigNamesToNewNames))

            # Rename default state
            for defaultNode in switchMatXmlNode.iterfind("Default"):
                stateName = defaultNode.get('ref')

                # See if it has been renamed or not. We'll pick the first new name if we have
                old2new = next((tup for tup in submatOrigNamesToNewNames if tup[0] == stateName), None)
                if old2new is None:
                    continue

                newStateName = old2new[1]
                defaultNode.set('ref', newStateName)
                logging.debug('\t\trenamed default ' + str(stateName) + ' to ' + str(newStateName))

            # Rename all other states
            for stateNode in switchMatXmlNode.iterfind("State"):
                stateName = stateNode.get('name')

                old2new = next((tup for tup in submatOrigNamesToNewNames if tup[0] == stateName), None)
                if old2new is None:
                    continue

                # Cover case #2, allowing us to pick the other newName whenever we search for this stateName again
                submatOrigNamesToNewNames.remove(old2new)

                logging.debug('\t\tsubmatsOrigNamesToNewNames now has ' + str(len(submatOrigNamesToNewNames)) + ' tuples')

                newStateName = old2new[1]

                # Keep track of renamed states so that we can update switch material references in variant sets if we need to
                switchMatDisplayName = switchMatXmlNode.get('name')  # Variant sets reference variants by name, not node
                renamedSwitchMaterialStates[switchMatDisplayName][stateName] = newStateName

                stateNode.set('name', newStateName)
                logging.debug('\t\trenamed ' + str(stateName) + ' to ' + str(newStateName))

        # Renamed switch material references from variant sets
        for matRefXmlNode in xmlRoot.iterfind(".//MaterialVariantRef"):
            maybeRenamedRef = matRefXmlNode.get('ref')
            oldStateName = matRefXmlNode.get('state')
            logging.debug('Analyzing MaterialVariantRef ref ' + str(maybeRenamedRef))

            if maybeRenamedRef in renamedSwitchMaterialStates:
                renamedStates = renamedSwitchMaterialStates[maybeRenamedRef]
                if oldStateName in renamedStates:
                    newStateName = renamedStates[oldStateName]
                    logging.debug('\tstate ' + str(oldStateName) + ' seems to have been renamed to ' + str(newStateName))
                    matRefXmlNode.set('state', newStateName)

    def restoreOriginalNames(self, objToOriginalNames):
        """
        Receives a dictionary from objects to strings of their original names.
        Calls key.setName(value) on every item of the dictionary

        Args:
            objToOriginalNames (dict of obj to str): Dictionary with objects and their original names
        """
        for pair in objToOriginalNames.items():
            pair[0].setName(pair[1])

    def getSafeVREDVersion(self):
        """
        Returns a floating point number indicating the current VRED version, or zero if something fails

        Returns:
            (float): floating point with current major and minor VRED versions
        """
        try:
            return dss.sanitize(str(vrController.getVredVersion()))[0]
        except:
            return "Unknown"

    def getUnitScale(self):
        """
        Returns the unit scale of the scene. 1.0 = mm, 10.0 = cm, 1000.0 = m

        Returns:
            (float): Value of the unit scale
        """
        # Since there is no API for getting this, we'll dig through the GUI for a combobox
        # in the status bar that kind of looks like what we want.
        # The combobox has no objectName, and although we could try filtering by the toolTip,
        # that is likely to change without warning, while these units are not
        units = {'mm': 1.0, 'cm': 10.0, 'm': 1000.0}

        try:
            for item in self.mainWindow.statusBar().findChildren(QtWidgets.QComboBox):
                currText = item.currentText()
                if currText in units.keys():
                    return units[currText]
        except AttributeError:
            pass

        return 1.0

    def getUnitScaleFactorTocm(self):
        """
        Returns the multiplicative factor to convert whatever scale is currently selected
        to cm, which will be the units of the exported .fbx file
        """
        return 10.0 / self.getUnitScale()

    def removeNURBS(self):
        """
        Travels the entire hierarchy, finds NURBS nodes and converts them. Also temporarily stores
        the node's transform in a sibling node and restores it back after conversion, since VRED doesn't
        maintain it during the conversion for some reason
        """
        def removeNURBSRecursive(node, nurbsNodes):
            for index in range(node.getNChildren()):
                child = node.getChild(index)

                # Nodes with this attachment *may* be NURBs, but all NURBs have this attachment
                if dsutils.isNURBS(child):
                    nurbsNodes.append(child)
                else:
                    removeNURBSRecursive(child, nurbsNodes)

        nodesToFix = []
        removeNURBSRecursive(vrScenegraph.getRootNode(), nodesToFix)

        logging.debug('Removing NURBS from nodes ' + str([a.getName() for a in nodesToFix]))

        count = 0
        for node in nodesToFix:
            nodeOrigName = node.getName()
            parent = node.getParent()

            newNode = vrScenegraph.createNode('Transform3D', str(nodeOrigName) + "_Temp", parent)

            vrScenegraph.copyTransformation(node, newNode)

            vrOptimize.removeNURBS(node)
            count += 1

            vrScenegraph.copyTransformation(newNode, node)

            newNode.sub()

        return count

    def isSceneDirty(self):
        titleText = ' - Autodesk'
        windowTitle = self.mainWindow.windowTitle()

        try:
            starPos = windowTitle.rfind(titleText)
            if starPos != -1:
                if windowTitle[starPos-1] != '*':
                    return False
        except:
            pass

        return True

    def exportSceneDialog(self):
        """
        Called when File->Export->Export to Datasmith... is called. Exports the scene FBX as well
        as auxilliary files like the variants *.var file and the lights *.lights file, all with the
        same name as the FBX file, exported on the same folder
        """
        # Open the file dialog to export the scene
        filename = vrFileDialog.getSaveFileName("Export scene", "", ["fbx(*.fbx)"], True)
        filename = str(QtCore.QDir.toNativeSeparators(filename))
        self.exportScene(filename)

    def exportScene(self, filename, isTest=False):
        if filename:
            filenameNoExt = os.path.splitext(filename)[0]
            originalMatNames = {}
            filenameTemp = ""
            vred2UnrealNode = None
            exporterVersionNode = None
            savedvpbPath = ""
            exportCompleted = False
            userCanceled = False
            cameraTransform = None

            try:
                if not isTest and self.isSceneDirty():
                    msgBox = QMessageBox()
                    msgBox.setText('The scene has unsaved changes, so it will be saved before exporting to Datasmith. Do you wish to proceed?\n')
                    msgBox.setDetailedText('In order to extract all scene information with maximum fidelity, some operations are required which, due to limitations in the VRED API, are undoable or destructive to the scene.\n\nThe export process will automatically save, perform those changes, then reload the scene.')
                    msgBox.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
                    msgBox.setDefaultButton(QMessageBox.No)
                    msgBox.setIcon(QMessageBox.Icon.Warning)
                    chosenOption = msgBox.exec_()
                    if chosenOption != QMessageBox.Yes:
                        userCanceled = True
                        return

                logging.info('=========================================')
                logging.info('||      Starting Datasmith export      ||')
                logging.info('=========================================')
                logging.info('Target file: ' + str(filename))
                logging.info('Exporter version: ' + str(exporterVersion))
                logging.info('VRED version: ' + str(self.getSafeVREDVersion()).replace('_', '.'))

                # Preserve camera position before/after export as loading will
                # reset it
                cameraTransform = vrCamera.getActiveCameraNode().getWorldTransform()

                # disable rendering to speed up things
                vrOSGWidget.enableRender(False)
                # disable scenegraph update to speed up things
                vrScenegraph.enableScenegraph(False)

                # Save scene since some operations are destructive and there is no workaround around some
                # API limitations
                if not isTest and self.isSceneDirty():
                    logging.info('Saving temp scene file...')
                    savedvpbPath, saveWorked = dsutils.save()
                    if not saveWorked:
                        return
                if len(savedvpbPath) == 0:
                    savedvpbPath = vrFileIO.getFileIOFilePath()

                # Clear undo stack to free deleted nodes that haven't been actually deleted yet
                vrController.clearUndoStack()

                settings = UserSettings()
                settings.tryFetchFromFile()

                # Remove cloned and transformable root nodes
                logging.info('Unsharing nodes...')
                self.unshareNodes()

                # Removing invalid nodes
                logging.info('Removing invalid nodes...')
                vrOptimize.removeEmptyGeometries(vrScenegraph.getRootNode())
                vrOptimize.removeInvalidTexCoords(vrScenegraph.getRootNode())

                # Convert NURBs to meshes
                logging.info('Removing NURBS...')
                numRemoved = self.removeNURBS()
                logging.info('Removed ' + str(numRemoved) + ' NURBS. Remaining NURBS: ' + str(dsutils.getNumSceneNURBs()))

                # Sanitize node names
                logging.info('Sanitizing node names...')
                originalNodeNames = self.renameNodesAndKeepOriginals()

                # Build an animation library so that we can keep track of clips and blocks
                logging.info('Creating animation library...')
                animLib = dsanim.catalogSceneAnimations(self.getUnitScaleFactorTocm(), settings.animBaseTime, settings.animPlayRate)
                animLib.sanitize()
                with open(filenameNoExt + '.clips', 'w') as animFile:
                    animLib.serialize(animFile)

                # Emit empty nodes to signal metadata
                exporterVersionStr = str(exporterVersion).replace('.', '_')
                vredVersionStr = self.getSafeVREDVersion()
                logging.info('Creating metadata hierarchy...')
                vred2UnrealNode = vrScenegraph.createNode('Transform', 'VRED2Unreal', vrScenegraph.getRootNode())
                exporterVersionNode = vrScenegraph.createNode('Transform', exporterVersionStr, vred2UnrealNode)
                vrScenegraph.createNode('Transform', vredVersionStr, exporterVersionNode)

                # Extract transform variants from nodes that have them
                logging.info('Extracting transform variants...')
                transVariants = self.extractTransformVariants()
                logging.debug('\tExtracted transform variants: ' + str(transVariants))

                # Save variants file with a tmp extension
                logging.info('Saving unprocessed variants file...')
                filenameTemp = filenameNoExt + '.tmp'
                vrVariantSets.saveVariants(filenameTemp)

                # Read the variants file back so that we can post-process it
                logging.info('Reading back unprocessed variants file...')
                varfileXML = ET.parse(filenameTemp)
                xmlRoot = varfileXML.getroot()

                # Post-process VAR file
                logging.info('Pre-processing variants file...')
                self.removePreviewTags(xmlRoot)
                self.renameNodesInVariantsFile(xmlRoot, originalNodeNames, transVariants)
                if animLib is not None:
                    self.renameClipsInVariantsFile(xmlRoot, animLib.ClipOriginalNamesToNewNames)

                # Post-process materials in variants file
                logging.info('Injecting switch material users...')
                originalMatNames = self.renameMatsAndKeepOriginals()
                self.renameMaterialsInVariantsFile(xmlRoot, originalMatNames)
                self.injectSwitchMaterialUsers(xmlRoot)

                # Post-process variants file
                self.sanitizeVariantNamesAndVariantSetRefs(xmlRoot)
                self.injectTransformVariants(xmlRoot, transVariants)
                self.sanitizeTransformVariantStateNames(xmlRoot, transVariants)
                self.fixupXmlContents(xmlRoot)

                # Write final var file
                logging.info("Writing variants file '" + str(filenameNoExt) + ".var'...")
                varfileXML.write(filenameNoExt + '.var')

                # Save lights to lights file
                logging.info("Writing lights file '" + str(filenameNoExt) + ".lights'...")
                lights = self.extractLights()
                self.writeLightsFile(filenameNoExt + '.lights', lights)

                # Write textures
                logging.info("Extracting material data...")
                mats = dsmat.extractMaterials(vrMaterialPtr.getAllMaterials())
                rootFolder = os.path.split(filename)[0]
                texFolder = os.path.join(rootFolder, 'Textures')
                try:
                    os.mkdir(texFolder)
                except OSError:  # Dir already exists
                    pass
                self.writeTextures(texFolder, mats)

                # Save materials to mat file
                logging.info("Writing mat file '" + str(filenameNoExt) + ".mat'...")
                self.writeMatFile(filenameNoExt + '.mats', mats)

                # Preserve unused Switch Material variants
                # After injecting switch material users so temp nodes don't show
                # up on the var file
                # This is useless if we're exporting with mats file, but will preserve these
                # materials in the FBX file if we're importing without them, for some reason
                logging.info('Preserving unused switch material variants...')
                matsToPreserve = self.getMaterialsToPreserve()
                self.assignMatsToTempGeometry(matsToPreserve)

                # Save FBX scene
                logging.info('Saving FBX scene...')
                vrFileIO.save(filename)

                exportCompleted = True

            except Exception as e:
                logging.error(e)

            finally:
                if userCanceled:
                    return False

                if os.path.exists(filenameTemp):
                    os.remove(filenameTemp)

                vrController.newScene()
                if not isTest and len(savedvpbPath) > 0 and os.path.exists(savedvpbPath):
                    logging.info('Loading back saved scene...')
                    vrFileIO.load([savedvpbPath], vrScenegraph.getRootNode().getParent(), True, False)

                # Restore camera position to what it was before the export
                if cameraTransform is not None:
                    vrCamera.getActiveCameraNode().setTransformMatrix(cameraTransform, False)

                vrScenegraph.enableScenegraph(True) # reenable scenegraph updates
                vrOSGWidget.enableRender(True) # reenable rendering

                if exportCompleted:
                    logging.info('=========================================')
                    logging.info('||          Export  succeeded          ||')
                    logging.info('=========================================')
                else:
                    logging.info('=========================================')
                    logging.info('||           Export   failed           ||')
                    logging.info('=========================================')
                logging.info('Target file: ' + str(filename))
                logging.info('Exporter version: ' + str(exporterVersion))
                logging.info('VRED version: ' + str(self.getSafeVREDVersion()).replace('_', '.'))

                return exportCompleted
