// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Platform.h"
#include "MuR/SerialisationPrivate.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Function.h"


#define MUTABLE_OP_MAX_INTERPOLATE_COUNT	6
#define MUTABLE_OP_MAX_ADD_COUNT			7
#define MUTABLE_OP_MAX_SWIZZLE_CHANNELS		4

namespace mu
{

    //!
    enum class OP_TYPE : uint16
    {
        //-----------------------------------------------------------------------------------------
        // No operation
        //-----------------------------------------------------------------------------------------
        NONE,

        //-----------------------------------------------------------------------------------------
        // Generic operations
        //-----------------------------------------------------------------------------------------

        //! Constant value
        BO_CONSTANT,
        NU_CONSTANT,
        SC_CONSTANT,
        CO_CONSTANT,
        IM_CONSTANT,
        ME_CONSTANT,
        LA_CONSTANT,
        PR_CONSTANT,
        ST_CONSTANT,

        //! User parameter
        BO_PARAMETER,
        NU_PARAMETER,
        SC_PARAMETER,
        CO_PARAMETER,
        PR_PARAMETER,
        IM_PARAMETER,
        ST_PARAMETER,

        //! Select one value or the other depending on a boolean input
        NU_CONDITIONAL,
        SC_CONDITIONAL,
        CO_CONDITIONAL,
        IM_CONDITIONAL,
        ME_CONDITIONAL,
        LA_CONDITIONAL,
        IN_CONDITIONAL,

        //! Select one of several values depending on an int input
        NU_SWITCH,
        SC_SWITCH,
        CO_SWITCH,
        IM_SWITCH,
        ME_SWITCH,
        LA_SWITCH,
        IN_SWITCH,

        //-----------------------------------------------------------------------------------------
        // Boolean operations
        //-----------------------------------------------------------------------------------------

        //! Compare two scalars, return true if the first is less than the second.
        BO_LESS,

        //! Compare an integerexpression with an integer constant
        BO_EQUAL_INT_CONST,

        //! Logical and
        BO_AND,

        //! Logical or
        BO_OR,

        //! Left as an exercise to the reader to find out what this op does.
        BO_NOT,

        //-----------------------------------------------------------------------------------------
        // Integer operations
        //-----------------------------------------------------------------------------------------

        //-----------------------------------------------------------------------------------------
        // Scalar operations
        //-----------------------------------------------------------------------------------------

        //! Multiply a scalar value by another onw and add a third one to the result
        SC_MULTIPLYADD,

        //! Apply an arithmetic operation to two scalars
        SC_ARITHMETIC,

        //! Get a scalar value from a curve
        SC_CURVE,

        //-----------------------------------------------------------------------------------------
        // Colour operations. Colours are sometimes used as generic vectors.
        //-----------------------------------------------------------------------------------------

        //! Sample an image to get its colour.
        CO_SAMPLEIMAGE,

        //! Make a color by shuffling channels from other colours.
        CO_SWIZZLE,

        //! Make a "color" from the size of an image. Colours are used as a generic vector here.
        CO_IMAGESIZE,

        //! Encode a layout block transformation in a vector: position = (x,y) size = ( z, w )
        CO_LAYOUTBLOCKTRANSFORM,

        //! Compose a vector from 4 scalars
        CO_FROMSCALARS,

        //! Apply component-wise arithmetic operations to two colours
        CO_ARITHMETIC,

        //-----------------------------------------------------------------------------------------
        // Image operations
        //-----------------------------------------------------------------------------------------

        //! Combine an image on top of another one using a specific effect (Blend, SoftLight, 
		//! Hardlight, Burn...). And optionally a mask.
        IM_LAYER,

        //! Apply a colour on top of an image using a specific effect (Blend, SoftLight, 
		//! Hardlight, Burn...), optionally using a mask.
        IM_LAYERCOLOUR,        

        //! Convert between pixel formats
        IM_PIXELFORMAT,

        //! Generate mipmaps up to a provided level
        IM_MIPMAP,

        //! Resize the image to a constant size
        IM_RESIZE,

        //! Resize the image to the size of another image
        IM_RESIZELIKE,

        //! Resize the image by a relative factor
        IM_RESIZEREL,

        //! Create an empty image to hold a particular layout.
        IM_BLANKLAYOUT,

        //! Copy an image into a rect of another one.
        IM_COMPOSE,

        //! Compare two images and generate a one channel image with 1 in the different pixels.
        IM_DIFFERENCE,

        //! Interpolate between 2 images taken from a row of targets (2 consecutive targets).
        IM_INTERPOLATE,

        //! Interpolate between 3 images.
        IM_INTERPOLATE3,

        //! Change the saturation of the image.
        IM_SATURATE,

        //! Generate a one-channel image with the luminance of the source image.
        IM_LUMINANCE,

        //! Recombine the channels of several images into one.
        IM_SWIZZLE,

        //! Create a black and white image with the pixels of another image that match a colour.
        IM_SELECTCOLOUR,

        //! Convert the source image colours using a "palette" image sampled with the source
        //! grey-level.
        IM_COLOURMAP,

        //! Build a horizontal gradient image from two colours
        IM_GRADIENT,

        //! Generate a black and white image from an image and a threshold.
        IM_BINARISE,

        //! Generate a plain colour image
        IM_PLAINCOLOUR,

        //! Image resulting from a GPU program
        IM_GPU,

        //! Cut a rect from an image
        IM_CROP,

        //! Replace a subrect of an image with another one
        IM_PATCH,

        //! Render a mesh texture layout into a mask
        IM_RASTERMESH,

        //! Create an image displacement encoding the grow operation for a mask
        IM_MAKEGROWMAP,

        //! Apply an image displacement on another image.
        IM_DISPLACE,

        //! Repeately apply
        IM_MULTILAYER,

        //! Inverts the colors of an image
        IM_INVERT,

        //! Modifiy roughness channel of an image based on normal variance.
        IM_NORMALCOMPOSITE,

		//! Apply linear transform to Image content. Resulting samples outside the original image are tiled.
		IM_TRANSFORM,

        //-----------------------------------------------------------------------------------------
        // Mesh operations
        //-----------------------------------------------------------------------------------------

        //! Apply a layout to a mesh texture coordinates channel
        ME_APPLYLAYOUT,

        //! Compare two meshes and extract a morph from the first to the second
        //! The meshes must have the same topology, etc.
        ME_DIFFERENCE,

        //! Apply a linear combination of two morphs on a base. The 2 morphs are taken from a
        //! sequence.
        ME_MORPH2,

        //! Merge a mesh to a mesh
        ME_MERGE,

        //! Interpolate between several meshes.
        ME_INTERPOLATE,

        //! Create a new mask mesh selecting all the faces of a source that are inside a given
        //! clip mesh.
        ME_MASKCLIPMESH,

        //! Create a new mask mesh selecting all the faces of a source that match another mesh.
        ME_MASKDIFF,

        //! Remove from a source mesh all the faces in common with another mesh
        ME_SUBTRACT,

        //! Remove all the geometry selected by a mask.
        ME_REMOVEMASK,

        //! Change the mesh format to match the format of another one.
        ME_FORMAT,

        //! Extract a fragment of a mesh containing a specific layout block.
        ME_EXTRACTLAYOUTBLOCK,

        //! Extract a fragment of a mesh containing a specific face group.
        ME_EXTRACTFACEGROUP,

        //! Apply a transform in a 4x4 matrix to the geometry channels of the mesh
        ME_TRANSFORM,

        //! Clip the mesh with a plane and morph it when it is near until it becomes an ellipse on
        //! the plane.
        ME_CLIPMORPHPLANE,

        //! Clip the mesh with another mesh.
        ME_CLIPWITHMESH,

        //! Replace the skeleton data from a mesh with another one.
        ME_SETSKELETON,

        //! Project a mesh using a projector and clipping the irrelevant faces
        ME_PROJECT,

        //! Deform a skinned mesh applying a skeletal pose
        ME_APPLYPOSE,

		//! Remap the vertex index buffer with the ones in the reference mesh, using the current
		//! ones as relative to that mesh.
		//! \TODO: Deprecated?
		ME_REMAPINDICES,

		//! Apply a geometry core operation to a mesh.
		ME_GEOMETRYOPERATION,

		//! Calculate the binding of a mesh on a shape
		ME_BINDSHAPE,

		//! Apply a shape on a (previously bound) mesh
		ME_APPLYSHAPE,

		//! Clip Deform using bind data.
		ME_CLIPDEFORM,
	
        //! Mesh morph with Skeleton Reshape based on the morphed mesh.
        ME_MORPHRESHAPE,

        //-----------------------------------------------------------------------------------------
        // Instance operations
        //-----------------------------------------------------------------------------------------

        //! Add a mesh to an instance
        IN_ADDMESH,

        //! Add an image to an instance
        IN_ADDIMAGE,

        //! Add a vector to an instance
        IN_ADDVECTOR,

        //! Add a scalar to an instance
        IN_ADDSCALAR,

        //! Add a string to an instance
        IN_ADDSTRING,

        //! Add a surface to an instance component
        IN_ADDSURFACE,

        //! Add a component to an instance LOD
        IN_ADDCOMPONENT,

        //! Add all LODs to an instance. This operation can only appear once in a model.
        IN_ADDLOD,

        //-----------------------------------------------------------------------------------------
        // Layout operations
        //-----------------------------------------------------------------------------------------

        //! Pack all the layout blocks from the source in the grid without overlapping
        LA_PACK,

        //! Merge two layouts
        LA_MERGE,

        //! Remove all layout blocks not used by any vertex of the mesh.
        //! This operation is for the new way of managing layout blocks.
        LA_REMOVEBLOCKS,

        //-----------------------------------------------------------------------------------------
        // Utility values
        //-----------------------------------------------------------------------------------------

        //!
        COUNT

    };


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template<typename ADDRESS_TYPE>
    struct t_OP
    {
        typedef ADDRESS_TYPE ADDRESS;

        //-----------------------------------------------------------------------------------------
        // Arguments for every operation type
        //-----------------------------------------------------------------------------------------
        struct BoolConstantArgs
        {
            bool value;
        };

        struct IntConstantArgs
        {
            int value;
        };

        struct ScalarConstantArgs
        {
            float value;
        };

        struct ColourConstantArgs
        {
            float value[4];
        };

        struct ResourceConstantArgs
        {
            ADDRESS value;
        };

        struct MeshConstantArgs
        {
            // Index of the mesh in the mesh constant array
            ADDRESS value;

            // If not negative, index of the skeleton to set to the mesh from the skeleton
            // constant array.
            int32 skeleton;

            // If not negative, index of the physics body to set to the mesh from the physics body
            // constant array.
			int32 physicsBody;
        };

        struct ParameterArgs
        {
            ADDRESS variable;
        };

        struct ConditionalArgs
        {
            ADDRESS condition, yes, no;
        };

        //-------------------------------------------------------------------------------------
        struct BoolLessArgs
        {
            ADDRESS a,b;
        };

        struct BoolEqualScalarConstArgs
        {
            ADDRESS value;
            int16 constant;
        };

        struct BoolBinaryArgs
        {
            ADDRESS a,b;
        };

        struct BoolNotArgs
        {
            ADDRESS source;
        };

        //-------------------------------------------------------------------------------------
        struct ScalarMultiplyAddArgs
        {
            ADDRESS factor0,factor1,add;
        };

        struct ScalarCurveArgs
        {
            ADDRESS time;   // Operation generating the time value to sample the curve
            ADDRESS curve;  // Constant curve
        };

        //-------------------------------------------------------------------------------------
        struct ColourSampleImageArgs
        {
            ADDRESS image;
            ADDRESS x,y;
            uint8 filter;
        };

        struct ColourSwizzleArgs
        {
            uint8 sourceChannels[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
            ADDRESS sources[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
        };

        struct ColourImageSizeArgs
        {
            ADDRESS image;
        };

        struct ColourLayoutBlockTransformArgs
        {
            ADDRESS layout;
            uint16 block;
        };

        struct ColourMultiplyArgs
        {
            ADDRESS a,b;
        };

		struct ColourFromScalarsArgs
		{
			ADDRESS x, y, z, w;
		};

        struct ArithmeticArgs
		{
			typedef enum
			{
				NONE,
				ADD,
				SUBTRACT,
				MULTIPLY,
				DIVIDE
			} OPERATION;
            uint8 operation;

			ADDRESS a, b;
		};


        //-------------------------------------------------------------------------------------
        struct ImageLayerArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS blended;
			uint8 blendType;	// One of BLEND_TYPE
			uint8 flags;
			typedef enum
            {
                F_NONE              =	0,
                //! The mask is considered binary: 0 means 0% and any other value means 100%
                F_BINARY_MASK       =	1 << 0,
                //! If the image has 4 channels, apply to the fourth channel as well.
                F_APPLY_TO_ALPHA    =	1 << 1
            } FLAGS;
        };

        struct ImageMultiLayerArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS blended;
            ADDRESS rangeSize;
            uint16 rangeId;
            uint16 blendType; // One of BLEND_TYPE
        };

        struct ImageLayerColourArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS colour;
            EImageFormat reformat;
			uint8 blendType;	// One of BLEND_TYPE
        };

        struct ImagePixelFormatArgs
        {
            ADDRESS source;
			EImageFormat format;
			EImageFormat formatIfAlpha;
        };

        struct ImageMipmapArgs
        {
            ADDRESS source;

            //! Number of mipmaps to build. If zero, it means all.
            uint8 levels;

            //! Number of mipmaps that can be generated for a single layout block.
            uint8 blockLevels;

            //! This is true if this operation is supposed to build only the tail mipmaps.
            //! It is used during the code optimisation phase, and to validate the code.
            bool onlyTail;

            //! Mipmap generation settings. 
            float sharpenFactor;
            EAddressMode addressMode;
            EMipmapFilterType filterType;
			bool ditherMipmapAlpha;
        };

        struct ImageResizeArgs
        {
            ADDRESS source;
            uint16 size[2];

            inline uint16 GetSize(int i) const { return size[i]; }
        };

        struct ImageResizeLikeArgs
        {
            //! Image that will be resized
            ADDRESS source;

            //! Image whose size will be used to resize the source.
            ADDRESS sizeSource;
        };

        struct ImageResizeVarArgs
        {
            //! Image that will be resized
            ADDRESS source;

            //! Size expression.
            ADDRESS size;
        };

        struct ImageResizeRelArgs
        {
            //! Image that will be resized
            ADDRESS source;

            //! Factor for each axis.
            float factor[2];
        };

        struct ImageBlankLayoutArgs
        {
            ADDRESS layout;

            //! Size of a layout block in pixels.
            uint16 blockSize[2];
			EImageFormat format;

            //! If true, generate all the mipmaps except the ones in skipMipmaps
            uint8 generateMipmaps;

            //! Mipmaps to generate if mipmaps are to be generated. 0 means all.
            uint8 mipmapCount;
        };

        struct ImageComposeArgs
        {
            ADDRESS layout, base, blockImage;
            ADDRESS mask;
            uint32 blockIndex;
        };

        struct ImageDifferenceArgs
        {
            ADDRESS a;
            ADDRESS b;
        };

        struct ImageInterpolateArgs
        {
            ADDRESS factor;
            ADDRESS targets[ MUTABLE_OP_MAX_INTERPOLATE_COUNT ];
        };

        struct ImageInterpolate3Args
        {
            ADDRESS factor1, factor2;
            ADDRESS target0, target1, target2;
        };

        struct ImageSaturateArgs
        {
            //! Image to modify.
            ADDRESS base;

            //! Saturation factor: 0 desaturates, 1 leaves the same, >1 saturates
            ADDRESS factor;

        };

        struct ImageLuminanceArgs
        {
            //! Image to modify.
            ADDRESS base;
        };

        struct ImageSwizzleArgs
        {
			EImageFormat format;
            uint8 sourceChannels[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
            ADDRESS sources[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
        };

        struct ImageSelectColourArgs
        {
            ADDRESS base;
            ADDRESS colour;
        };

        struct ImageColourMapArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS map;
        };

        struct ImageGradientArgs
        {
            ADDRESS colour0;
            ADDRESS colour1;
            uint16 size[2];
        };

        struct ImageBinariseArgs
        {
            ADDRESS base;
            ADDRESS threshold;
        };

        struct ImagePlainColourArgs
        {
            ADDRESS colour;
			EImageFormat format;
            uint16 size[2];
        };

        struct ImageGPUArgs
        {
            ADDRESS program;
        };

        struct ImageCropArgs
        {
            ADDRESS source;
            uint16 minX, minY, sizeX, sizeY;
        };

        struct ImagePatchArgs
        {
            ADDRESS base;
            ADDRESS patch;
            uint16 minX, minY;
        };

        struct ImageRasterMeshArgs
        {
            ADDRESS mesh;
            uint16 sizeX, sizeY;
            int32 blockIndex;

            // These are used in case of projected mesh raster.
            ADDRESS image;
            ADDRESS angleFadeProperties;

            //! Mask selecting the pixels in the destination image that may receive projection.
            ADDRESS mask;

            //! A projector may be needed for some kind of per-pixel raster operations
            //! like cylindrical projections.
            ADDRESS projector;
        };

        struct ImageMakeGrowMapArgs
        {
            ADDRESS mask;
            int32 border;
        };

        struct ImageDisplaceArgs
        {
            ADDRESS source;
            ADDRESS displacementMap;
        };

		struct ImageInvertArgs
		{
			ADDRESS base;
		};

        struct ImageNormalCompositeArgs
        {
            ADDRESS base;
            ADDRESS normal;

            float power;
            ECompositeImageMode mode;
        };

		struct ImageTransformArgs
		{
			ADDRESS base = 0;
			ADDRESS offsetX = 0;
			ADDRESS offsetY = 0;
			ADDRESS scaleX = 0;
			ADDRESS scaleY = 0;
			ADDRESS rotation = 0;
		};

        //-------------------------------------------------------------------------------------
        struct MeshApplyLayoutArgs
        {
            ADDRESS mesh;
            ADDRESS layout;
            uint16 channel;
        };

        struct MeshMergeArgs
        {
            ADDRESS base;
            ADDRESS added;

            // If 0, it merges the surfaces, otherwise, add a new surface
            // for the added mesh.
            uint32 newSurfaceID;
        };

        struct MeshInterpolateArgs
        {
            ADDRESS factor;
            ADDRESS base;
            ADDRESS targets[ MUTABLE_OP_MAX_INTERPOLATE_COUNT-1 ];
        };

        struct MeshMaskClipMeshArgs
        {
            ADDRESS source;
            ADDRESS clip;
        };

        struct MeshRemapIndicesArgs
        {
            ADDRESS source;
            ADDRESS reference;
        };

        struct MeshMaskDiffArgs
        {
            ADDRESS source;
            ADDRESS fragment;
        };

        struct MeshSubtractArgs
        {
            ADDRESS a;
            ADDRESS b;
        };

        struct MeshFormatArgs
        {
            ADDRESS source;
            ADDRESS format;

            typedef enum
            {
                BT_VERTEX			= 1,
                BT_INDEX			= 2,
                BT_FACE				= 4,
                //! This flag will not add blank channels for the channels in the format mesh but not
                //! in the source mesh.
                BT_IGNORE_MISSING	= 16,
                //! This flag will force the reset of buffer indices to 0
                BT_RESETBUFFERINDICES = 32
            } BUFFER_TYPE;

            //! Flag combination, selecting the buffers to reformat. The rest are left untouched.
            uint8 buffers;

        };


        struct MeshExtractFaceGroupArgs
        {
            ADDRESS source;
            uint32 group;
        };

		struct MeshTransformArgs
		{
			ADDRESS source;
			ADDRESS matrix;
		};

		struct MeshClipMorphPlaneArgs
		{
			ADDRESS source;

			ADDRESS morphShape;
			ADDRESS vertexSelectionShapeOrBone;
			
			typedef enum 
			{
				VS_NONE,
				VS_SHAPE,
				VS_BONE_HIERARCHY
			} VERTEX_SELECTION_TYPE;
            uint8 vertexSelectionType;

			float dist, factor, maxBoneRadius;
		};

        struct MeshClipWithMeshArgs
        {
            ADDRESS source;
            ADDRESS clipMesh;
        };

        struct MeshSetSkeletonArgs
        {
            ADDRESS source;
            ADDRESS skeleton;
        };

        struct MeshProjectArgs
        {
            ADDRESS mesh;
            ADDRESS projector;
        };

        struct MeshApplyPoseArgs
        {
            ADDRESS base;
            ADDRESS pose;
        };

		struct MeshGeometryOperationArgs
		{
			ADDRESS meshA;
			ADDRESS meshB;
			ADDRESS scalarA;
			ADDRESS scalarB;
		};

		enum class EMeshBindShapeFlags : uint32
		{
			None				   = 0,
			ReshapeSkeleton		   = 1 << 0,
			DiscardInvalidBindings = 1 << 1,
			EnableRigidParts       = 1 << 2,
			DeformAllBones		   = 1 << 3,
			ReshapePhysicsVolumes  = 1 << 4,
			ReshapeVertices		   = 1 << 5,
			DeformAllPhysics	   = 1 << 6,
		};

		struct MeshBindShapeArgs
		{
			ADDRESS mesh;
			ADDRESS shape;
			uint32 flags;
			uint32 bindingMethod;
		};

		struct MeshApplyShapeArgs
		{
			ADDRESS mesh;
			ADDRESS shape;
			uint32 flags;
		};

		struct MeshMorphReshapeArgs
		{
			ADDRESS Morph;
			ADDRESS Reshape;
		};


		struct MeshClipDeformArgs
		{
			ADDRESS mesh;
			ADDRESS clipShape;

			float clipWeightThreshold = 0.9f;
		};


        //-------------------------------------------------------------------------------------
        struct InstanceAddArgs
        {
            ADDRESS instance;
            ADDRESS value;
            uint32 id;
            uint32 externalId;
            ADDRESS name;

            // Index in the PROGRAM::m_parameterLists with the parameters that are relevant
            // for whatever is added by this operation. This is used only for resources like
            // images or meshes.
            ADDRESS relevantParametersListIndex;
        };

        struct InstanceAddLODArgs
        {
            ADDRESS lod[ MUTABLE_OP_MAX_ADD_COUNT ];
        };

        //-------------------------------------------------------------------------------------
        struct LayoutPackArgs
        {
            ADDRESS layout;
        };

        struct LayoutMergeArgs
        {
            ADDRESS base;
            ADDRESS added;
        };

        struct LayoutRemoveBlocksArgs
        {
            // Layout to be processed
            ADDRESS source;

            // Source mesh to scan for active blocks
            ADDRESS mesh;            
            uint8 meshLayoutIndex;
        };


        //-------------------------------------------------------------------------------------
        //-------------------------------------------------------------------------------------
        //-------------------------------------------------------------------------------------
        t_OP()
        {
            type = OP_TYPE::NONE;
            unused = 0;
			FMemory::Memzero( &args, sizeof(args) );
        }

        bool operator==( const t_OP<ADDRESS>& o ) const
        {
            if (o.type!=type) return false;
            //if (o.flags!=flags) return false;
            if (FMemory::Memcmp(&o.args, &args, sizeof(ARGS))) return false;
            return true;
        }

        bool operator<( const t_OP<ADDRESS>& o ) const
        {
            if (o.type<type) return true;
            if (o.type>type) return false;
//            if (o.flags<flags) return true;
//            if (o.flags>flags) return false;
            if (FMemory::Memcmp(&o.args, &args, sizeof(ARGS))<0) return true;
            return false;
        }

        OP_TYPE type;

        typedef enum
        {
            //! In a particular state, this op is used only once. Knowing this help optimising the
            //! build process since it allows reusing the resources in the operation result.
            //F_SINGLE_USE = 1 << 0

        } FLAGS;

        //! Bitmask of FLAGS values
        uint16 unused;

        //! Constant arguments per operation type
		//! Everything in this union needs to be converted to their own AST class eventually.
        typedef union
        {
            IntConstantArgs IntConstant;
            ScalarConstantArgs ScalarConstant;
            ColourConstantArgs ColourConstant;
            //ParameterArgs Parameter;

            //-------------------------------------------------------------------------------------
            BoolLessArgs BoolLess;
            BoolEqualScalarConstArgs BoolEqualScalarConst;
            BoolBinaryArgs BoolBinary;
            BoolNotArgs BoolNot;

            //-------------------------------------------------------------------------------------
            ScalarMultiplyAddArgs ScalarMultiplyAdd;
            ArithmeticArgs ScalarArithmetic;

            //-------------------------------------------------------------------------------------
            ColourSampleImageArgs ColourSampleImage;
            ColourSwizzleArgs ColourSwizzle;
            ColourImageSizeArgs ColourImageSize;
            ColourMultiplyArgs ColourMultiply;
            ColourLayoutBlockTransformArgs ColourLayoutBlockTransform;
            ColourFromScalarsArgs ColourFromScalars;
            ArithmeticArgs ColourArithmetic;

            //-------------------------------------------------------------------------------------
            ImageLayerArgs ImageLayer;
            ImageLayerColourArgs ImageLayerColour;
            ImageResizeArgs ImageResize;
            ImageResizeLikeArgs ImageResizeLike;
            ImageResizeVarArgs ImageResizeVar;
            ImageResizeRelArgs ImageResizeRel;
            ImageBlankLayoutArgs ImageBlankLayout;
            ImageDifferenceArgs ImageDifference;
            ImageInterpolateArgs ImageInterpolate;
            ImageInterpolate3Args ImageInterpolate3;
            ImageSaturateArgs ImageSaturate;
            ImageLuminanceArgs ImageLuminance;
            ImageSwizzleArgs ImageSwizzle;
            ImageSelectColourArgs ImageSelectColour;
            ImageColourMapArgs ImageColourMap;
            ImageGradientArgs ImageGradient;
            ImageBinariseArgs ImageBinarise;
            ImagePlainColourArgs ImagePlainColour;
            ImageGPUArgs ImageGPU;
            ImageCropArgs ImageCrop;
            ImageRasterMeshArgs ImageRasterMesh;
            ImageMakeGrowMapArgs ImageMakeGrowMap;
            ImageDisplaceArgs ImageDisplace;
			ImageInvertArgs ImageInvert;

            //-------------------------------------------------------------------------------------
            MeshApplyLayoutArgs MeshApplyLayout;
            MeshMergeArgs MeshMerge;
            MeshInterpolateArgs MeshInterpolate;
            MeshMaskDiffArgs MeshMaskDiff;
            MeshSubtractArgs MeshSubtract;
            MeshExtractFaceGroupArgs MeshExtractFaceGroup;
			MeshClipMorphPlaneArgs MeshClipMorphPlane;
            MeshClipWithMeshArgs MeshClipWithMesh;
            MeshSetSkeletonArgs MeshSetSkeleton;
			MeshProjectArgs MeshProject;

            //-------------------------------------------------------------------------------------
            LayoutPackArgs LayoutPack;
            LayoutMergeArgs LayoutMerge;
            LayoutRemoveBlocksArgs LayoutRemoveBlocks;

        } ARGS;

        ARGS args;
    };

    typedef t_OP<uint32> OP;

    // OP is not serialisable anymore, we link code now.
    MUTABLE_DEFINE_POD_SERIALISABLE( OP );
    MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE( OP );
    
    // Check that we didn't go out of control with the operation size
    static_assert( sizeof(OP::ARGS)==28, "Argument union has an unexpected size." );
    static_assert( sizeof(OP)==32, "Operation struct has an unexpected size." );


	//! Types of data handled by the Mutable runtime.
	typedef enum
	{
		DT_NONE,
		DT_BOOL,
		DT_INT,
		DT_SCALAR,
		DT_COLOUR,
		DT_IMAGE,
		DT_VOLUME_DEPRECATED,
		DT_LAYOUT,
		DT_MESH,
		DT_INSTANCE,
		DT_PROJECTOR,
		DT_STRING,

		// Supporting data types : Never returned as an actual data type for any operation.
		DT_MATRIX,
		DT_SHAPE,
		DT_CURVE,
		DT_SKELETON,
		DT_PHYSICS_ASSET,
		
		DT_COUNT
	} DATATYPE;


	// Generic data about a Mutable runtime operation.
    struct OP_DESC
    {
		//! Type of data generated by the instruction
		DATATYPE type;

        //! True if the instruction is worth caching when generating models
        bool cached;

        //! True if the instruction is worth executing in the GPU
        bool gpuizable;

        //! For image instructions, for every image format, true if it is supported as the base
        //! format of the operation.
        //! TODO: Move to tools library?
		bool supportedBasePixelFormats[uint8(EImageFormat::IF_COUNT)];
    };

	MUTABLERUNTIME_API extern const OP_DESC& GetOpDesc( OP_TYPE type );


    //!
    inline static DATATYPE GetOpDataType( OP_TYPE type )
    {
		return GetOpDesc(type).type;
    }

    //! Utility function to apply a function to all operation references to other operations.
	MUTABLERUNTIME_API extern void ForEachReference( OP& op, const TFunctionRef<void(OP::ADDRESS*)> );

    //! Utility function to apply a function to all operation references to other operations.
	MUTABLERUNTIME_API extern void ForEachReference( const struct PROGRAM& program, OP::ADDRESS at, const TFunctionRef<void(OP::ADDRESS)> );

	//!
	MUTABLERUNTIME_API inline OP_TYPE GetSwitchForType( DATATYPE d )
    {
        switch (d)
        {
        case DT_INSTANCE: return OP_TYPE::IN_SWITCH;
        case DT_MESH: return OP_TYPE::ME_SWITCH;
		case DT_IMAGE: return OP_TYPE::IM_SWITCH;
		case DT_LAYOUT: return OP_TYPE::LA_SWITCH;
        case DT_COLOUR: return OP_TYPE::CO_SWITCH;
        case DT_SCALAR: return OP_TYPE::SC_SWITCH;
        case DT_INT: return OP_TYPE::NU_SWITCH;
        default:
			check(false);
			break;
        }
        return OP_TYPE::NONE;
    }
}

