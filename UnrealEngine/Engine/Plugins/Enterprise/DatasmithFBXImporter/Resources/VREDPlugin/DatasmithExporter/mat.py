import sys
import os
import logging

import vrScenegraph
import vrFieldAccess
import vrNodePtr
import vrNodeUtils
import vrMaterialPtr

# Adds the ScriptPlugins folder to path so that we can find the other files
# in our module
scriptPluginsFolder = os.path.dirname(os.path.realpath(os.curdir))
if scriptPluginsFolder not in sys.path:
    sys.path.append(scriptPluginsFolder)

import DatasmithExporter.utils as dsutils

class Tex:
    def __init__(self):
        self.srcPath = ''  # Path stored in VRED, used by the artist
        self.exportedPath = ''  # Path of the exported pixel data during export process
        self.texType = ''
        self.pixelData = []
        self.width = -1
        self.height = -1
        self.bytesPerPixel = -1
        self.channels = -1
        self.stride = 0  # Corrected stride, since we need to have pixels and rows aligned to 32-bit
        self.pixelFormat = -1
        self.dataType = -1  # unsigned char, etc
        self.properties = {}

    def __repr__(self):
        text = '\tTex at path "' + str(self.srcPath) + '", Type: ' + str(self.texType) + '\n'
        text += '\tProperties:\n'

        text += '\t\twidth: ' + str(self.width) + '\n'
        text += '\t\theight: ' + str(self.height) + '\n'
        text += '\t\tbytesPerPixel: ' + str(self.bytesPerPixel) + '\n'
        text += '\t\tchannels: ' + str(self.channels) + '\n'
        text += '\t\tstride: ' + str(self.stride) + '\n'
        text += '\t\tpixelFormat: ' + str(self.pixelFormat) + '\n'
        text += '\t\tdataType: ' + str(self.dataType) + '\n'

        for prop in self.properties:
            text += '\t\t' + str(prop) + ': ' + str(self.properties[prop]) + '\n'

        return text

class Mat:
    def __init__(self):
        self.name = ""
        self.matType = ""
        self.vredMatId = 0
        self.users = []  # Name of nodes that use this material
        self.properties = {}  # indexed by name e.g. diffuseColor, specularColor, roughness
        self.textures = {}  # indexed by name e.g. diffuseTexture, roughnessTexture
        self.submaterials = []  # name of submaterials (if we're a composite material like a switch material)

    def __repr__(self):
        text = 'Mat "' + str(self.name) + '", Type: ' + str(self.matType) + ', VREDID: ' + str(self.vredMatId) + '\nUsers:\n'

        for user in self.users:
            text += '\t' + str(user) + '\n'

        text += 'Properties:\n'

        for prop in self.properties:
            text += '\t' + str(prop) + ': ' + str(self.properties[prop]) + '\n'

        text += 'Textures:\n'

        for tex in self.textures:
            text += '\t' + str(tex) + ': ' + str(self.textures[tex]) + '\n'

        text += '\n'
        return text

# mat.fields().hasField('diffuseColor')
fieldsPropList = {
    'diffuseColor':'SFVec3f',
    'specularColor':'SFVec3f',
    'occlusionColor':'SFVec3f',  # Common
    'occlusionIntensity':'SFReal32',  # Common
    'lightingMode':'SFInt8',  # Common
    'useStructure':'SFBool',  # Roughness texture
    'parallaxIntensity':'SFReal32',  # Roughness texture
    'bumpIntensity':'SFReal32',  # Roughness texture
    'structureSize':'SFReal32',  # Roughness texture
    'structureIntensity':'SFReal32',  # Roughness texture
    #'bumpType':'SFInt32',  # Roughness texture
    'incandescenceColor':'SFVec4f',  # Incandescence texture, intensity is alpha
    'roughness':'SFReal32',  # Plastic, flake roughness
    'reflectivity':'SFReal32',  # Plastic, metallic carpaint, brushed metal
    'seeThrough':'SFVec3f',  # 1 - alpha
    'diffuseBackscattering':'SFReal32',  # Phong
    'fresnelQuality':'SFUInt32',  # Reflective Plastic
    'reflectionColor':'SFVec3f',  # Chrome
    'smear':'SFReal32',  # Chrome
    'contrast':'SFReal32',  # Chrome
    'saturation':'SFReal32',  # Chrome
    'metalType':'SFInt32',  # Chrome
    'isRough':'SFBool',  # Chrome
    'roughnessU':'SFReal32',  # Brushed metal
    'roughnessV':'SFReal32',  # Brushed metal
    'exteriorColor':'SFVec3f',  # Glass, value in HSV is its alpha (??)
    'interiorColor':'SFVec3f',  # Glass, value in HSV is its alpha (??)
    'useFrostedGlass':'SFBool',  # Glass, useRoughness in UI
    'useReflectivity':'SFBool',  # Glass, Use Reflectivity in UI
    'refractionIndex':'SFReal32',  # Glass, IOR
    'paintType':'SFUInt32',  # Metallic carpaint
    'baseColor':'SFVec3f',  # Metallic carpaint
    'flakeColor':'SFVec3f',  # Metallic carpaint
    'useFlipFlop':'SFBool',  # Metallic carpaint
    'flipFlopFlakeColor':'SFVec3f',  # Metallic carpaint
    'flipFlopBlending':'SFReal32',  # Metallic carpaint
    'flakeReflectivity':'SFReal32',  # Metallic carpaint
    'flakeSize':'SFReal32',  # Metallic carpaint, flipflop carpaint, 1/ui value
    'flakeIntensity':'SFReal32',  # Metallic carpaint, Flake Perturbation in ui, flipflop carpaint
    'clearcoatColor':'SFVec3f',  # Metallic carpaint
    'clearcoatThickness':'SFReal32',  # Metallic carpaint
    'clearcoatDensity':'SFReal32',  # Metallic carpaint
    'clearcoatRefractionIndex':'SFReal32',  # Metallic carpaint
    'useOrangePeel':'SFBool',  # Metallic carpaint
    'orangePeelFrequency':'SFReal32',  # Metallic carpaint, wavy bump map
    'orangePeelIntensity':'SFReal32',  # Metallic carpaint
    'pigmentColor':'SFVec3f',  # Metallic carpaint
    'pigmentDensity':'SFReal32',  # Metallic carpaint, Pigment concentration in UI
    'flakeDensity':'SFReal32',  # Metallic carpaint, Flake Density in ui
    'flakeColor1':'SFVec3f',  # Flipflop carpaint
    'flakeColor2':'SFVec3f',  # Flipflop carpaint
    'blending':'SFReal32',  # Flipflop carpaint
    'brushAxis':'SFInt8',  # Brushed metal
    'triplanarRotate':'SFVec3f',  # Brushed metal
}

# vrFieldAccess(mat.fields().getFieldContainer('colorComponentData')).hasField('iorType')
ccdPropList = {
    'clearcoatReflectivity':'SFReal32',  # Brushed metal
    'displacementHeight':'SFReal32',  # Disp texture
    'displacementOffset':'SFReal32',  # Disp texture, value added to disp
    'displacementSilhouetteMode':'SFUInt32',  # Disp texture
    'scatterType':'SFInt8',  # Subsurface scattering
    'scatterColor':'SFVec3f',  # Subsurface scattering
    'roughnessTextureRange':'SFVec4f',  # Roughness texture, min is [0], max is [2]
    'iorType':'SFUInt32',  # Glass
    'clearcoatType':'SFUInt32'  # Metallic carpaint
}

# vrFieldAccess(mat.fields().getFieldContainer('secondFlakeLayerSettings')).hasField('flakeColor')
flakeLayerPropList = {
    'flakeColor':'SFVec3f',  # Metallic carpaint
    'flakeSize':'SFReal32',  # Metallic carpaint, flipflop carpaint, 1/ui value
    'flakeRoughness':'SFReal32',  # Metallic carpaint, flipflop carpaint, 1/ui value
    'flakeIntensity':'SFReal32',  # Metallic carpaint, Flake Perturbation in ui, flipflop carpaint
    'flakeDensity':'SFReal32',  # Metallic carpaint, Flake Density in ui
    'flakeReflectivity':'SFReal32',  # Metallic carpaint
    'pigmentColor':'SFVec3f',  # Metallic carpaint
    'pigmentDensity':'SFReal32',  # Metallic carpaint, Pigment concentration in UI
}

# vrFieldAccess(colorComponentData.getFieldContainer('diffuseComponent'))
texTypes = [
    'diffuseComponent',
    'glossyComponent',
    'specularComponent',
    'incandescenceComponent',
    'bumpComponent',
    'transparencyComponent',
    'scatterComponent',
    'roughnessComponent',
    'displacementComponent',
    'fresnelComponent',
    'rotationComponent',
    'indexOfRefractionComponent',
    'specularBumpComponent'
]

# vrFieldAccess(colorComponentData.getFieldContainer('diffuseComponent')).getReal32('anisotropy')
texPropList = {
    #'useTexture':'SFBool', We check this explicitly
    'repeatMode':'SFUInt32',
    'repeat':'SFVec2f',
    'offset':'SFVec2f',
    'rotate':'SFReal32',
    'anisotropy':'SFReal32',
    'gamma':'SFVec3f',
    'invertTexture':'SFBool',
    'textureTarget':'SFUInt32',
    'color':'SFVec3f',
    'connectRepeatModes':'SFBool',
    'useImageSequence':'SFBool',
    'imageNumber':'SFUInt32',
    'frameOffset':'SFUInt32',
    'componentType':'SFString',
    'hasLinkedSettings':'SFBool',
    'textureProjectionType':'SFUInt32',
    'planarProjectionFlags':'SFUInt32',
    'triplanarRepeatU':'SFVec3f',
    'triplanarRepeatV':'SFVec3f',
    'triplanarOffsetU':'SFVec3f',
    'triplanarOffsetV':'SFVec3f',
    'triplanarBlend':'SFReal32',
    'triplanarRotation':'SFVec3f',
    'textureSizeMM':'SFVec2f',
    'useRealWorldScale':'SFBool'
}

def getPropValue(fieldAccessObj, propName, propType):
    if propType == 'SFReal32':
        return fieldAccessObj.getReal32(propName)
    elif propType == 'SFUInt32':
        return fieldAccessObj.getUInt32(propName)
    elif propType == 'SFBool':
        return fieldAccessObj.getBool(propName)
    elif propType == 'SFVec2f':
        return fieldAccessObj.getVec(propName, 2)
    elif propType == 'SFVec3f':
        return fieldAccessObj.getVec(propName, 3)
    elif propType == 'SFVec4f':
        return fieldAccessObj.getVec(propName, 4)
    elif propType == 'SFInt8':
        return fieldAccessObj.getInt8(propName)
    elif propType == 'SFUInt8':
        return fieldAccessObj.getUInt8(propName)
    elif propType == 'SFInt32':
        return fieldAccessObj.getInt32(propName)
    elif propType == 'SFString':
        return fieldAccessObj.getString(propName)
    else:
        logging.error('Invalid property type ' + str(propType))
        return None

def unProcessTexture(tex):
    """
    Reverts some processing that VRED applies to bump and displacement maps
    so that we can export an image that is as close as possible to whatever was originally
    imported in VRED. This might cause a 32-bit pixel/row-aligned image to become unaligned!
    """
    logging.debug('\tUnprocessing texture ' + str(tex.srcPath))

    if tex.texType == 'bumpComponent':
        # There are many different processes that VRED does to a bump map when it is first imported:
        # I've seen it discard the B channel and flip R and G; Do nothing; Flip R and G and do some
        # kind of histogram normalization on B.
        # Additionally it will, internally, create an RGB texture for the bump map regardless of
        # whether it was originally a grayscale or an RGB image.
        # In the end, we have no idea what has been done to this image when we have access to it.
        # 'nothing' seems to be the majority of cases here, so we'll ignore them for now.
        # Besides, we have to consider that this is a regular normal map on import, so the user
        # can quickly switch to another source image on UnrealEditor side and things will just work
        return

    elif tex.texType == 'displacementComponent':
        # If this was an RGB image on import, VRED seems to convert it to grayscale.
        # The stored texture will be RGB regardless though, but the grayscale image will be stored on the B
        # channel only. R and G channels seems to store some kind of processed version (erode/dilate?)
        if tex.channels == 3 and tex.bytesPerPixel == 3 and tex.dataType == 5121:
            newBpp = 1
            newChannels = 1
            newStride = tex.width * newBpp

            newPixelData = [0]*(newBpp * tex.width * tex.height)
            for i in range(tex.height):
                for j in range(tex.width):
                    newPixelData[i * newStride + j] = tex.pixelData[i * tex.stride + j * tex.bytesPerPixel + 2]

            tex.pixelData = newPixelData
            tex.bytesPerPixel = 1
            tex.channels = newChannels
            tex.stride = newStride
            return

        logging.warning('Unknown problem unProcessing this displacement texture: ' + str(tex))

def prepareTexturePixelData(tex):
    """
    Aligns each pixel (and each row) to 32-bit boundaries so that it can be exported with PySide2's PNG
    exporter. It will ignore strange formats (like non-uchar images and so on) as those will be exported
    as raw binary data anyway.

    Due to the padding required and supported export image formats, we only have access to
    8-bit 1-channel and 8-bit 4-channel, really.

    Also flips the image vertically
    """
    logging.debug('\tPreparing texture pixel data ' + str(tex.srcPath))

    # someVals = allMips[:50]
    # logging.warning('Tex ' + str(tex.srcPath) + ' was originally: \n\ttype: ' + str(tex.texType) + '\n\tbpp: ' + str(tex.bytesPerPixel) + '\n\tchannels: ' + str(tex.channels) + '\n\tpixFormat: ' + str(tex.pixelFormat) + '\n\tsome vals: ' + str(someVals))

    validDataType = tex.dataType == 5121  # unsigned byte

    # Realign RGB24 images to RGBA32 so they can be exported via PNG
    # PySide2 only handles up to RGBA32 (no RGBA64),
    # meaning there is no point trying to fixup bpps of 5 or 7, or other channel types, if those even exist
    if validDataType and tex.bytesPerPixel == 3 and tex.channels == 3:
        newBpp = 4
        newChannels = 4

        logging.debug('\t\tRealigning RGB24 image...')

        # Pad rows to 32-bit if necessary
        newStride = tex.width * newBpp
        remainder = newStride % 4
        if remainder != 0:
            newStride = newStride + remainder
        zeroMip = [0]*(newStride*tex.height)

        for i in range(tex.height):
            for j in range(tex.width):
                zeroMip[i*newStride + j*newBpp + 0] = tex.pixelData[(tex.height-1-i)*tex.stride + j*tex.bytesPerPixel + 0]
                zeroMip[i*newStride + j*newBpp + 1] = tex.pixelData[(tex.height-1-i)*tex.stride + j*tex.bytesPerPixel + 1]
                zeroMip[i*newStride + j*newBpp + 2] = tex.pixelData[(tex.height-1-i)*tex.stride + j*tex.bytesPerPixel + 2]
                zeroMip[i*newStride + j*newBpp + 3] = 255

        tex.bytesPerPixel = newBpp
        tex.channels = newChannels
        tex.stride = newStride
        tex.pixelData = zeroMip
        return

    # Like GL_LUMINANCE_ALPHA: Need to replicate first channel to RGB and put the second
    # channel on alpha. This because the Qt exporter doesn't have support for any 16 bit
    # 2-channel image
    elif validDataType and tex.bytesPerPixel == 2 and tex.channels == 2:
        newBpp = 4
        newChannels = 4

        logging.debug('\t\tHandling RG16 image...')

        # Pad rows to 32-bit if necessary
        newStride = tex.width * newBpp
        remainder = newStride % 4
        if remainder != 0:
            newStride = newStride + remainder
        zeroMip = [0]*(newStride*tex.height)

        for i in range(tex.height):
            for j in range(tex.width):
                lumin = tex.pixelData[(tex.height-1-i)*tex.stride + j*tex.bytesPerPixel + 0]
                alpha = tex.pixelData[(tex.height-1-i)*tex.stride + j*tex.bytesPerPixel + 1]

                zeroMip[i*newStride + j*newBpp + 0] = lumin
                zeroMip[i*newStride + j*newBpp + 1] = lumin
                zeroMip[i*newStride + j*newBpp + 2] = lumin
                zeroMip[i*newStride + j*newBpp + 3] = alpha

        tex.bytesPerPixel = newBpp
        tex.channels = newChannels
        tex.stride = newStride
        tex.pixelData = zeroMip
        return

    # Pixels are 32-bit aligned (rows might not be)
    elif validDataType and (4 % tex.bytesPerPixel == 0 or tex.bytesPerPixel % 4 == 0):

        logging.debug('\t\tAligning rows...')

        newStride = tex.stride
        remainder = tex.stride % 4
        if remainder != 0:
            newStride = tex.stride + remainder

        zeroMip = [0]*(newStride*tex.height)
        for i in range(tex.height):
            zeroMip[i*newStride:i*newStride + tex.stride] = tex.pixelData[(tex.height-1-i)*tex.stride:(tex.height-i)*tex.stride]

        tex.stride = newStride
        tex.pixelData = zeroMip
        return

    # Flip vertically at least, if we have a strange format
    else:
        logging.debug('\t\tFlipping vertically...')

        newStride = tex.stride
        zeroMip = [0]*newStride*tex.height
        for i in range(tex.height):
            zeroMip[i*newStride:(i+1)*newStride] = tex.pixelData[(tex.height-1-i)*tex.stride:(tex.height-i)*tex.stride]

        tex.pixelData = zeroMip
        tex.stride = newStride
        return

def extractTexture(texName, fieldAccess):
    """
    Extracts texture with name 'texName' from fieldAccess and returns its data
    as a Tex object
    """
    vredTex = vrFieldAccess.vrFieldAccess(fieldAccess.getFieldContainer(texName))

    # Ignore disabled/defaulted textures
    if not vredTex.hasField('useTexture') or \
        not vredTex.getBool('useTexture') or \
        not vredTex.hasField('image'):
        return None

    # Ignore textures without an actual image
    img = vrFieldAccess.vrFieldAccess(vredTex.getFieldContainer('image'))
    if not img.isValid():
        return None

    # Disable all textures so the regular export process doesn't emit them
    #vredTex.setBool('useTexture', False)

    tex = Tex()
    tex.texType = texName

    # Get regular properties
    for texPropName in texPropList:
        if vredTex.hasField(texPropName):
            propType = texPropList[texPropName]
            tex.properties[texPropName] = getPropValue(vredTex, texPropName, propType)

    # Get image properties
    if img.hasField('name'):
        tex.srcPath = img.getString('name')
        tex.width = img.getInt32('width')
        tex.height = img.getInt32('height')
        tex.bytesPerPixel = img.getInt32('bpp')
        tex.dataType = img.getInt32('dataType')
        tex.pixelFormat = img.getUInt32('pixelFormat')
        tex.stride = tex.bytesPerPixel * tex.width
        tex.pixelData = img.getMUInt8('pixel')
        if tex.pixelFormat == 6406 or tex.pixelFormat == 6409:
            tex.channels = 1
        elif tex.pixelFormat == 6407:
            tex.channels = 3
        elif tex.pixelFormat == 6408:
            tex.channels = 4
        elif tex.pixelFormat == 6410:
            tex.channels = 2

        unProcessTexture(tex)
        prepareTexturePixelData(tex)

    # Get projection transform for planar projection
    proj = vrFieldAccess.vrFieldAccess(vredTex.getFieldContainer('planarProjection'))
    tex.properties['planarTranslation'] = proj.getVec('translation', 3)
    tex.properties['planarEulerRotation'] = proj.getVec('eulerRotation', 3)
    tex.properties['planarScale'] = proj.getVec('scale', 3)

    return tex

def overwriteMappingForTriplanarMaterial(mat, fields):
    """
    Triplanar materials are an old, deprecated type of material that is still very
    common in some scenes. This function will overwrite the texture mapping information
    for the diffuse, specular, roughness and bump textures present in 'mat' with the
    data describing a triplanar mapping for a triplanar material
    """
    triplanarRepeatU = [1, 1, 1]
    if fields.hasField('xRepeatU'):
        triplanarRepeatU[0] = getPropValue(fields, 'xRepeatU', 'SFReal32')
    if fields.hasField('yRepeatU'):
        triplanarRepeatU[1] = getPropValue(fields, 'yRepeatU', 'SFReal32')
    if fields.hasField('zRepeatU'):
        triplanarRepeatU[2] = getPropValue(fields, 'zRepeatU', 'SFReal32')

    triplanarRepeatV = [1, 1, 1]
    if fields.hasField('xRepeatV'):
        triplanarRepeatV[0] = getPropValue(fields, 'xRepeatV', 'SFReal32')
    if fields.hasField('yRepeatV'):
        triplanarRepeatV[1] = getPropValue(fields, 'yRepeatV', 'SFReal32')
    if fields.hasField('zRepeatV'):
        triplanarRepeatV[2] = getPropValue(fields, 'zRepeatV', 'SFReal32')

    triplanarOffsetU = [0, 0, 0]
    triplanarOffsetV = [0, 0, 0]
    if fields.hasField('xOffset'):
        triplanarOffsetU[0], triplanarOffsetV[0] = getPropValue(fields, 'xOffset', 'SFVec2f')
    if fields.hasField('yOffset'):
        triplanarOffsetU[1], triplanarOffsetV[1] = getPropValue(fields, 'yOffset', 'SFVec2f')
    if fields.hasField('zOffset'):
        triplanarOffsetU[2], triplanarOffsetV[2] = getPropValue(fields, 'zOffset', 'SFVec2f')

    triplanarRotation = [0, 0, 0]
    if fields.hasField('rotateX'):
        triplanarRotation[0] = getPropValue(fields, 'rotateX', 'SFReal32')
    if fields.hasField('rotateY'):
        triplanarRotation[1] = getPropValue(fields, 'rotateY', 'SFReal32')
    if fields.hasField('rotateZ'):
        triplanarRotation[2] = getPropValue(fields, 'rotateZ', 'SFReal32')

    textureSizeMM = [50, 50]
    if fields.hasField('textureSizeMM'):
        textureSizeMM = getPropValue(fields, 'textureSizeMM', 'SFVec2f')

    triplanarBlend = 0
    if fields.hasField('edgeBlend'):
        triplanarBlend = getPropValue(fields, 'edgeBlend', 'SFReal32')

    affectedTextures = ['diffuseComponent', 'glossyComponent', 'specularComponent', 'bumpComponent']
    for texName in affectedTextures:
        if texName in mat.textures:
            tex = mat.textures[texName]

            tex.properties['textureProjectionType'] = 2
            tex.properties['triplanarRepeatU'] = triplanarRepeatU
            tex.properties['triplanarRepeatV'] = triplanarRepeatV
            tex.properties['triplanarOffsetU'] = triplanarOffsetU
            tex.properties['triplanarOffsetV'] = triplanarOffsetV
            tex.properties['textureSizeMM'] = textureSizeMM
            tex.properties['triplanarBlend'] = triplanarBlend
            tex.properties['triplanarRotation'] = triplanarRotation

def extractMaterials(vredMats):
    """
    Builds a list of Mat from vrMaterialPtr objects

    Args:
        vredMats (list of vrMaterialPtr): Materials to process

    Returns:
        List of Mat objects, one for each material
    """
    mats = []
    processedMats = set()

    for vredMat in vredMats:
        mat = Mat()
        mats.append(mat)

        logging.debug(vredMat.getName())

        vredMatId = vredMat.getID()
        if vredMatId in processedMats:
            continue
        processedMats.add(vredMatId)

        mat.name = vredMat.getName()
        mat.matType = vredMat.getType()
        mat.vredMatId = vredMatId
        mat.users = [n.getName() for n in vredMat.getNodes()]

        # Keep track of submaterials if we're a composite material
        for vredSubmat in vredMat.getSubMaterials():
            vredSubmatName = vredSubmat.getName()
            if vredSubmatName not in mat.submaterials:
                mat.submaterials.append(vredSubmatName)

        # Extract properties outside colorComponentData
        logging.debug('\tProps outside CCD')
        fields = vredMat.fields()
        for fieldPropName in fieldsPropList:
            if fields.hasField(fieldPropName):
                propType = fieldsPropList[fieldPropName]
                mat.properties[fieldPropName] = getPropValue(fields, fieldPropName, propType)

        # Extract data about exterior transparency texture which is an
        # exception for some reason
        logging.debug('\tProp about exterior transparency texture')
        if fields.hasField('exteriorTransparencyComponent'):
            tex = extractTexture('exteriorTransparencyComponent', fields)
            if tex != None:
                mat.textures['exteriorTransparencyComponent'] = tex

        # Extract properties from colorComponentData
        if fields.hasField('colorComponentData'):
            ccd = vrFieldAccess.vrFieldAccess(fields.getFieldContainer('colorComponentData'))

            logging.debug('\tCCD props')
            for ccdPropName in ccdPropList:
                if ccd.hasField(ccdPropName):
                    propType = ccdPropList[ccdPropName]
                    mat.properties[ccdPropName] = getPropValue(ccd, ccdPropName, propType)

            # Extract textures
            logging.debug('\tTextures')
            for texName in texTypes:
                if ccd.hasField(texName):
                    tex = extractTexture(texName, ccd)
                    if tex != None:
                        mat.textures[texName] = tex

        # If this is a triplanar material, fixup the triplanar mapping info for
        # affected textures
        logging.debug('\tFixup for triplanar')
        if vredMat.getType() == 'UTriplanarMaterial':
            overwriteMappingForTriplanarMaterial(mat, fields)

        # Extract data for metallic carpaint materials
        # Append a '2' as most of these have the same names as the primary flake layer
        logging.debug('\tMetallic carpaint second flake layer')
        if fields.hasField('secondFlakeLayerSettings'):
            sfl = vrFieldAccess.vrFieldAccess(fields.getFieldContainer('secondFlakeLayerSettings'))
            for sflPropName in flakeLayerPropList:
                if sfl.hasField(sflPropName):
                    propType = flakeLayerPropList[sflPropName]
                    mat.properties[sflPropName + "2"] = getPropValue(sfl, sflPropName, propType)

    for mat in mats:
        logging.debug(mat)

    return mats