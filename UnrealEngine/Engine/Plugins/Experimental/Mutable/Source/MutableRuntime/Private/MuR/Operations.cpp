// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Operations.h"

#include "MuR/ModelPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    #define OP_DESC_COUNT		size_t(OP_TYPE::COUNT)

    // clang-format off
    static const OP_DESC s_opDescs[OP_DESC_COUNT] =
	{ 
		// type			cached	GPU-izable	supported base image formats
        { DT_NONE,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NONE

        { DT_BOOL,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_CONSTANT
        { DT_INT,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONSTANT
        { DT_SCALAR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONSTANT
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONSTANT
        { DT_IMAGE,		true,   false,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CONSTANT
        { DT_MESH,		true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONSTANT
        { DT_LAYOUT,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONSTANT
        { DT_PROJECTOR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_CONSTANT
        { DT_STRING,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_CONSTANT

        { DT_BOOL,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_PARAMETER
		{ DT_INT,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_PARAMETER
		{ DT_SCALAR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_PARAMETER
		{ DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_PARAMETER
        { DT_PROJECTOR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_PARAMETER
        { DT_IMAGE,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PARAMETER
        { DT_STRING,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_PARAMETER

        { DT_INT,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONDITIONAL
		{ DT_SCALAR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONDITIONAL
		{ DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONDITIONAL
        { DT_IMAGE,		true,	false,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }		},	// IM_CONDITIONAL
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONDITIONAL
        { DT_LAYOUT,	false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONDITIONAL
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_CONDITIONAL

        { DT_INT,		false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_SWITCH
		{ DT_SCALAR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_SWITCH
		{ DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWITCH
		{ DT_IMAGE,		true,	false,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }		},	// IM_SWITCH
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SWITCH
		{ DT_LAYOUT,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_SWITCH
		{ DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_SWITCH

        { DT_BOOL,		false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_LESS
        { DT_BOOL,		false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_EQUAL_SC_CONST
        { DT_BOOL,		false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_AND
        { DT_BOOL,		false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_OR
        { DT_BOOL,		false,  false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_NOT

        { DT_SCALAR,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_MULTIPLYADD
        { DT_SCALAR,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_ARITHMETIC
        { DT_SCALAR,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CURVE

        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SAMPLEIMAGE
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWIZZLE
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_IMAGESIZE
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_LAYOUTBLOCKTRANSFORM
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_FROMSCALARS
        { DT_COLOUR,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_ARITHMETIC

        { DT_IMAGE,		true,	true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYER
        { DT_IMAGE,		true,	true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYERCOLOUR
        { DT_IMAGE,		true,	false,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PIXELFORMAT
        { DT_IMAGE,		true,	false,		{ 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MIPMAP
        { DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZE
        { DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZELIKE
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZEREL
		{ DT_IMAGE,		true,   false,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BLANKLAYOUT
		{ DT_IMAGE,		true,	true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 }		},	// IM_COMPOSE
        { DT_IMAGE,		true,	false,		{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_DIFFERENCE
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INTERPOLATE
		{ DT_IMAGE,		true,	true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INTERPOLATE3
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SATURATE
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LUMINANCE
        { DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SWIZZLE
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SELECTCOLOUR
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_COLOURMAP
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_GRADIENT
        { DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BINARISE
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PLAINCOLOUR
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_GPU
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CROP
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PATCH
        { DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RASTERMESH
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MAKEGROWMAP
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_DISPLACE
		{ DT_IMAGE,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MULTILAYER
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INVERT
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_NORMALCOMPOSITE
		{ DT_IMAGE,		true,	false,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_TRANSFORM

        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYLAYOUT
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_DIFFERENCE
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPH2
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MERGE
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_INTERPOLATE
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKCLIPMESH
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKDIFF
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SUBTRACT
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_REMOVEMASK
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_FORMAT
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_EXTRACTLAYOUTBLOCK
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_EXTRACTFACEGROUP
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_TRANSFORM
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPMORPHPLANE
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPWITHMESH
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SETSKELETON
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_PROJECT
        { DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYPOSE
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_REMAPINDICES
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_GEOMETRYOPERATION
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_BINDSHAPE
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYSHAPE
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPDEFORM
		{ DT_MESH,		true,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPHRESHAPE

        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDMESH
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDIMAGE
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDVECTOR
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSCALAR
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSTRING
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSURFACE
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDCOMPONENT
        { DT_INSTANCE,	false,	false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDLOD

        { DT_LAYOUT,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_PACK
        { DT_LAYOUT,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_MERGE
        { DT_LAYOUT,	true,   false,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_REMOVEBLOCKS
    };

    // clang-format on

    static_assert( OP_DESC_COUNT == (int)OP_TYPE::COUNT, "OperationDescMismatch" );

	//---------------------------------------------------------------------------------------------
	const OP_DESC& GetOpDesc( OP_TYPE type )
	{
        return s_opDescs[ (int)type ];
	}


    //---------------------------------------------------------------------------------------------
    void ForEachReference( OP& op, const TFunctionRef<void(OP::ADDRESS*)> f )
    {
        switch ( op.type )
        {
        case OP_TYPE::NONE:
        case OP_TYPE::BO_CONSTANT:
        case OP_TYPE::NU_CONSTANT:
        case OP_TYPE::SC_CONSTANT:
        case OP_TYPE::CO_CONSTANT:
        case OP_TYPE::IM_CONSTANT:
        case OP_TYPE::ME_CONSTANT:
        case OP_TYPE::LA_CONSTANT:
        case OP_TYPE::PR_CONSTANT:
        case OP_TYPE::BO_PARAMETER:
        case OP_TYPE::NU_PARAMETER:
        case OP_TYPE::SC_PARAMETER:
        case OP_TYPE::CO_PARAMETER:
        case OP_TYPE::PR_PARAMETER:
        case OP_TYPE::IM_PARAMETER:
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_LESS:
            f(&op.args.BoolLess.a );
            f(&op.args.BoolLess.b );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_EQUAL_INT_CONST:
            f(&op.args.BoolEqualScalarConst.value );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_AND:
        case OP_TYPE::BO_OR:
            f(&op.args.BoolBinary.a );
            f(&op.args.BoolBinary.b );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_NOT:
            f(&op.args.BoolNot.source );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_MULTIPLYADD:
            f(&op.args.ScalarMultiplyAdd.factor0 );
            f(&op.args.ScalarMultiplyAdd.factor1 );
            f(&op.args.ScalarMultiplyAdd.add );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_ARITHMETIC:
            f(&op.args.ScalarArithmetic.a );
            f(&op.args.ScalarArithmetic.b );
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::CO_SAMPLEIMAGE:
            f(&op.args.ColourSampleImage.image );
            f(&op.args.ColourSampleImage.x );
            f(&op.args.ColourSampleImage.y );
            break;

        case OP_TYPE::CO_SWIZZLE:
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(&op.args.ColourSwizzle.sources[t] );
            }
            break;

        case OP_TYPE::CO_IMAGESIZE:
            f(&op.args.ColourSampleImage.image );
            break;

        case OP_TYPE::CO_LAYOUTBLOCKTRANSFORM:
            f(&op.args.ColourLayoutBlockTransform.layout );
            break;

        case OP_TYPE::CO_FROMSCALARS:
            f(&op.args.ColourFromScalars.x);
            f(&op.args.ColourFromScalars.y);
            f(&op.args.ColourFromScalars.z);
            f(&op.args.ColourFromScalars.w);
            break;

        case OP_TYPE::CO_ARITHMETIC:
            f(&op.args.ColourArithmetic.a);
            f(&op.args.ColourArithmetic.b);
            break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_LAYER:
            f(&op.args.ImageLayer.base );
            if ( op.args.ImageLayer.mask )
            {
                f(&op.args.ImageLayer.mask );
            }
            f(&op.args.ImageLayer.blended );
            break;

        case OP_TYPE::IM_LAYERCOLOUR:
            f(&op.args.ImageLayerColour.base );
            if ( op.args.ImageLayerColour.mask )
            {
                f(&op.args.ImageLayerColour.mask );
            }
            f(&op.args.ImageLayerColour.colour );
            break;

        case OP_TYPE::IM_RESIZE:
            f(&op.args.ImageResize.source );
            break;

        case OP_TYPE::IM_RESIZELIKE:
            f(&op.args.ImageResizeLike.source );
            f(&op.args.ImageResizeLike.sizeSource );
            break;

        case OP_TYPE::IM_RESIZEREL:
            f(&op.args.ImageResizeRel.source );
            break;

        case OP_TYPE::IM_BLANKLAYOUT:
            f(&op.args.ImageBlankLayout.layout );
            break;

        case OP_TYPE::IM_DIFFERENCE:
            f(&op.args.ImageDifference.a );
            f(&op.args.ImageDifference.b );
            break;

        case OP_TYPE::IM_INTERPOLATE:
            f(&op.args.ImageInterpolate.factor );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT;++t)
            {
                f(&op.args.ImageInterpolate.targets[t] );
            }
            break;

        case OP_TYPE::IM_INTERPOLATE3:
            f(&op.args.ImageInterpolate3.factor1 );
            f(&op.args.ImageInterpolate3.factor2 );
            f(&op.args.ImageInterpolate3.target0 );
            f(&op.args.ImageInterpolate3.target1 );
            f(&op.args.ImageInterpolate3.target2 );
            break;

        case OP_TYPE::IM_SWIZZLE:
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(&op.args.ImageSwizzle.sources[t] );
            }
            break;

        case OP_TYPE::IM_SATURATE:
            f(&op.args.ImageSaturate.base );
            f(&op.args.ImageSaturate.factor );
            break;

        case OP_TYPE::IM_LUMINANCE:
            f(&op.args.ImageLuminance.base );
            break;

        case OP_TYPE::IM_SELECTCOLOUR:
            f(&op.args.ImageSelectColour.base );
            f(&op.args.ImageSelectColour.colour );
            break;

        case OP_TYPE::IM_COLOURMAP:
            f(&op.args.ImageColourMap.base );
            f(&op.args.ImageColourMap.mask );
            f(&op.args.ImageColourMap.map );
            break;

        case OP_TYPE::IM_GRADIENT:
            f(&op.args.ImageGradient.colour0 );
            f(&op.args.ImageGradient.colour1 );
            break;

        case OP_TYPE::IM_BINARISE:
            f(&op.args.ImageBinarise.base );
            f(&op.args.ImageBinarise.threshold );
            break;

        case OP_TYPE::IM_PLAINCOLOUR:
            f(&op.args.ImagePlainColour.colour );
            break;

        case OP_TYPE::IM_CROP:
            f(&op.args.ImageCrop.source );
            break;

        case OP_TYPE::IM_RASTERMESH:
            f(&op.args.ImageRasterMesh.mesh );
            f(&op.args.ImageRasterMesh.image );
            f(&op.args.ImageRasterMesh.mask );
            f(&op.args.ImageRasterMesh.angleFadeProperties );
            f(&op.args.ImageRasterMesh.projector );
            break;

        case OP_TYPE::IM_MAKEGROWMAP:
            f(&op.args.ImageMakeGrowMap.mask );
            break;

        case OP_TYPE::IM_DISPLACE:
            f(&op.args.ImageDisplace.source );
            f(&op.args.ImageDisplace.displacementMap );
            break;

		case OP_TYPE::IM_INVERT:
			f(&op.args.ImageInvert.base);
			break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::ME_APPLYLAYOUT:
            f(&op.args.MeshApplyLayout.layout );
            f(&op.args.MeshApplyLayout.mesh );
            break;

        case OP_TYPE::ME_MERGE:
            f(&op.args.MeshMerge.base );
            f(&op.args.MeshMerge.added );
            break;

        case OP_TYPE::ME_INTERPOLATE:
            f(&op.args.MeshInterpolate.factor );
            f(&op.args.MeshInterpolate.base );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1;++t)
            {
                f(&op.args.MeshInterpolate.targets[t] );
            }
            break;

        case OP_TYPE::ME_MASKDIFF:
            f(&op.args.MeshMaskDiff.source );
            f(&op.args.MeshMaskDiff.fragment );
            break;

        case OP_TYPE::ME_SUBTRACT:
            f(&op.args.MeshSubtract.a );
            f(&op.args.MeshSubtract.b );
            break;

        case OP_TYPE::ME_EXTRACTFACEGROUP:
            f(&op.args.MeshExtractFaceGroup.source );
            break;

        case OP_TYPE::ME_CLIPMORPHPLANE:
            f(&op.args.MeshClipMorphPlane.source);
            break;

        case OP_TYPE::ME_CLIPWITHMESH :
            f(&op.args.MeshClipWithMesh.source);
            f(&op.args.MeshClipWithMesh.clipMesh);
            break;

        case OP_TYPE::ME_SETSKELETON :
            f(&op.args.MeshSetSkeleton.source);
            f(&op.args.MeshSetSkeleton.skeleton);
            break;

		case OP_TYPE::ME_PROJECT:
			f(&op.args.MeshProject.mesh);
			f(&op.args.MeshProject.projector);
			break;

        //-------------------------------------------------------------------------------------
        case OP_TYPE::LA_PACK:
            f(&op.args.LayoutPack.layout );
            break;

        case OP_TYPE::LA_MERGE:
            f(&op.args.LayoutMerge.base );
            f(&op.args.LayoutMerge.added );
            break;

        case OP_TYPE::LA_REMOVEBLOCKS:
            f(&op.args.LayoutRemoveBlocks.source );
            f(&op.args.LayoutRemoveBlocks.mesh );
            break;

        default:
			check( false );
            break;
        }

    }



    //---------------------------------------------------------------------------------------------
    void ForEachReference( const PROGRAM& program, OP::ADDRESS at, const TFunctionRef<void(OP::ADDRESS)> f )
    {
        OP_TYPE type = program.GetOpType(at);
        switch ( type )
        {
        case OP_TYPE::NONE:
        case OP_TYPE::BO_CONSTANT:
        case OP_TYPE::NU_CONSTANT:
        case OP_TYPE::SC_CONSTANT:
        case OP_TYPE::ST_CONSTANT:
        case OP_TYPE::CO_CONSTANT:
        case OP_TYPE::IM_CONSTANT:
        case OP_TYPE::ME_CONSTANT:
        case OP_TYPE::LA_CONSTANT:
        case OP_TYPE::PR_CONSTANT:
        case OP_TYPE::BO_PARAMETER:
        case OP_TYPE::NU_PARAMETER:
        case OP_TYPE::SC_PARAMETER:
        case OP_TYPE::CO_PARAMETER:
        case OP_TYPE::PR_PARAMETER:
        case OP_TYPE::IM_PARAMETER:
            break;

        case OP_TYPE::SC_CURVE:
        {
            auto args = program.GetOpArgs<OP::ScalarCurveArgs>(at);
            f(args.time );
            break;
        }

        case OP_TYPE::NU_CONDITIONAL:
        case OP_TYPE::SC_CONDITIONAL:
        case OP_TYPE::CO_CONDITIONAL:
        case OP_TYPE::IM_CONDITIONAL:
        case OP_TYPE::ME_CONDITIONAL:
        case OP_TYPE::LA_CONDITIONAL:
        case OP_TYPE::IN_CONDITIONAL:
        {
            auto args = program.GetOpArgs<OP::ConditionalArgs>(at);
            f(args.condition );
            f(args.yes );
            f(args.no );
            break;
        }

        case OP_TYPE::NU_SWITCH:
        case OP_TYPE::SC_SWITCH:
        case OP_TYPE::CO_SWITCH:
        case OP_TYPE::IM_SWITCH:
        case OP_TYPE::ME_SWITCH:
        case OP_TYPE::LA_SWITCH:
        case OP_TYPE::IN_SWITCH:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);
			
			OP::ADDRESS VarAddress;
			FMemory::Memcpy( &VarAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS DefAddress;
			FMemory::Memcpy( &DefAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			uint32_t CaseCount;
			FMemory::Memcpy( &CaseCount, data, sizeof(uint32_t));
			data += sizeof(uint32_t);

			f(VarAddress);
			f(DefAddress);

			for ( uint32_t C = 0; C < CaseCount; ++C )
			{	
				//int32_t Condition;
				//FMemory::Memcpy( &Condition, data, sizeof(int32_t));
				data += sizeof(int32_t);
				
				OP::ADDRESS At;
				FMemory::Memcpy( &At, data, sizeof(OP::ADDRESS) );
				data += sizeof(OP::ADDRESS);

				if (At)
				{
					f(At);
				}
			}

            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_LESS:
        {
            auto args = program.GetOpArgs<OP::BoolLessArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_EQUAL_INT_CONST:
        {
            auto args = program.GetOpArgs<OP::BoolEqualScalarConstArgs>(at);
            f(args.value );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_AND:
        case OP_TYPE::BO_OR:
        {
            auto args = program.GetOpArgs<OP::BoolBinaryArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_NOT:
        {
            auto args = program.GetOpArgs<OP::BoolNotArgs>(at);
            f(args.source );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_MULTIPLYADD:
        {
            auto args = program.GetOpArgs<OP::ScalarMultiplyAddArgs>(at);
            f(args.factor0 );
            f(args.factor1 );
            f(args.add );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_ARITHMETIC:
        {
            auto args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::CO_SAMPLEIMAGE:
        {
            auto args = program.GetOpArgs<OP::ColourSampleImageArgs>(at);
            f(args.image );
            f(args.x );
            f(args.y );
            break;
        }

        case OP_TYPE::CO_SWIZZLE:
        {
            auto args = program.GetOpArgs<OP::ColourSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case OP_TYPE::CO_IMAGESIZE:
        {
            auto args = program.GetOpArgs<OP::ColourSampleImageArgs>(at);
            f(args.image );
            break;
        }

        case OP_TYPE::CO_LAYOUTBLOCKTRANSFORM:
        {
            auto args = program.GetOpArgs<OP::ColourLayoutBlockTransformArgs>(at);
            f(args.layout );
            break;
        }

        case OP_TYPE::CO_FROMSCALARS:
        {
            auto args = program.GetOpArgs<OP::ColourFromScalarsArgs>(at);
            f(args.x);
            f(args.y);
            f(args.z);
            f(args.w);
            break;
        }

        case OP_TYPE::CO_ARITHMETIC:
        {
            auto args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.a);
            f(args.b);
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_LAYER:
        {
            auto args = program.GetOpArgs<OP::ImageLayerArgs>(at);
            f(args.base );
            if ( args.mask )
            {
                f(args.mask );
            }
            f(args.blended );
            break;
        }

        case OP_TYPE::IM_LAYERCOLOUR:
        {
            auto args = program.GetOpArgs<OP::ImageLayerColourArgs>(at);
            f(args.base );
            if ( args.mask )
            {
                f(args.mask );
            }
            f(args.colour );
            break;
        }

        case OP_TYPE::IM_MULTILAYER:
        {
            auto args = program.GetOpArgs<OP::ImageMultiLayerArgs>(at);
            f(args.rangeSize );
            f(args.base );
            if ( args.mask )
            {
                f(args.mask );
            }
            f(args.blended );
            break;
        }

		case OP_TYPE::IM_NORMALCOMPOSITE:
		{
			auto args = program.GetOpArgs<OP::ImageNormalCompositeArgs>(at);
			f(args.base);
			f(args.normal);

			break;
		}

        case OP_TYPE::IM_PIXELFORMAT:
        {
            auto args = program.GetOpArgs<OP::ImagePixelFormatArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_MIPMAP:
        {
            auto args = program.GetOpArgs<OP::ImageMipmapArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_RESIZE:
        {
            auto args = program.GetOpArgs<OP::ImageResizeArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_RESIZELIKE:
        {
            auto args = program.GetOpArgs<OP::ImageResizeLikeArgs>(at);
            f(args.source );
            f(args.sizeSource );
            break;
        }

        case OP_TYPE::IM_RESIZEREL:
        {
            auto args = program.GetOpArgs<OP::ImageResizeRelArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_BLANKLAYOUT:
        {
            auto args = program.GetOpArgs<OP::ImageBlankLayoutArgs>(at);
            f(args.layout );
            break;
        }

        case OP_TYPE::IM_COMPOSE:
        {
            auto args = program.GetOpArgs<OP::ImageComposeArgs>(at);
            f(args.layout );
            f(args.base );
            f(args.blockImage );
            f(args.mask );
            break;
        }

        case OP_TYPE::IM_DIFFERENCE:
        {
            auto args = program.GetOpArgs<OP::ImageDifferenceArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        case OP_TYPE::IM_INTERPOLATE:
        {
            auto args = program.GetOpArgs<OP::ImageInterpolateArgs>(at);
            f(args.factor );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT;++t)
            {
                f(args.targets[t] );
            }
            break;
        }

        case OP_TYPE::IM_INTERPOLATE3:
        {
            auto args = program.GetOpArgs<OP::ImageInterpolate3Args>(at);
            f(args.factor1 );
            f(args.factor2 );
            f(args.target0 );
            f(args.target1 );
            f(args.target2 );
            break;
        }

        case OP_TYPE::IM_SWIZZLE:
        {
            auto args = program.GetOpArgs<OP::ImageSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case OP_TYPE::IM_SATURATE:
        {
            auto args = program.GetOpArgs<OP::ImageSaturateArgs>(at);
            f(args.base );
            f(args.factor );
            break;
        }

        case OP_TYPE::IM_LUMINANCE:
        {
            auto args = program.GetOpArgs<OP::ImageLuminanceArgs>(at);
            f(args.base );
            break;
        }

        case OP_TYPE::IM_SELECTCOLOUR:
        {
            auto args = program.GetOpArgs<OP::ImageSelectColourArgs>(at);
            f(args.base );
            f(args.colour );
            break;
        }

        case OP_TYPE::IM_COLOURMAP:
        {
            auto args = program.GetOpArgs<OP::ImageColourMapArgs>(at);
            f(args.base );
            f(args.mask );
            f(args.map );
            break;
        }

        case OP_TYPE::IM_GRADIENT:
        {
            auto args = program.GetOpArgs<OP::ImageGradientArgs>(at);
            f(args.colour0 );
            f(args.colour1 );
            break;
        }

        case OP_TYPE::IM_BINARISE:
        {
            auto args = program.GetOpArgs<OP::ImageBinariseArgs>(at);
            f(args.base );
            f(args.threshold );
            break;
        }

        case OP_TYPE::IM_PLAINCOLOUR:
        {
            auto args = program.GetOpArgs<OP::ImagePlainColourArgs>(at);
            f(args.colour );
            break;
        }

        case OP_TYPE::IM_CROP:
        {
            auto args = program.GetOpArgs<OP::ImageCropArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_PATCH:
        {
            auto args = program.GetOpArgs<OP::ImagePatchArgs>(at);
            f(args.base );
            f(args.patch );
            break;
        }

        case OP_TYPE::IM_RASTERMESH:
        {
            auto args = program.GetOpArgs<OP::ImageRasterMeshArgs>(at);
            f(args.mesh );
            f(args.image );
            f(args.mask );
            f(args.angleFadeProperties );
            f(args.projector );
            break;
        }

        case OP_TYPE::IM_MAKEGROWMAP:
        {
            auto args = program.GetOpArgs<OP::ImageMakeGrowMapArgs>(at);
            f(args.mask );
            break;
        }

        case OP_TYPE::IM_DISPLACE:
        {
            auto args = program.GetOpArgs<OP::ImageDisplaceArgs>(at);
            f(args.source );
            f(args.displacementMap );
            break;
        }

		case OP_TYPE::IM_INVERT:
		{
			auto args = program.GetOpArgs<OP::ImageInvertArgs>(at);
			f(args.base);
			break;
		}

		case OP_TYPE::IM_TRANSFORM:
		{
			OP::ImageTransformArgs Args = program.GetOpArgs<OP::ImageTransformArgs>(at);
			f(Args.base);
			f(Args.offsetX);
			f(Args.offsetY);
			f(Args.scaleX);
			f(Args.scaleY);
			f(Args.rotation);
			break;
		}

        //-------------------------------------------------------------------------------------
        case OP_TYPE::ME_APPLYLAYOUT:
        {
			OP::MeshApplyLayoutArgs args = program.GetOpArgs<OP::MeshApplyLayoutArgs>(at);
            f(args.layout );
            f(args.mesh );
            break;
        }

        case OP_TYPE::ME_DIFFERENCE:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(BaseAt);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(TargetAt);
			break;
        }

        case OP_TYPE::ME_MORPH2:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(FactorAt);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(BaseAt);

			uint8 NumTargets = 0;
			FMemory::Memcpy(&NumTargets, data, sizeof(uint8)); data += sizeof(uint8);

			for (uint8 T = 0; T < NumTargets; ++T)
			{
				OP::ADDRESS TargetAt = 0;
				FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
				f(TargetAt);
			}
			break;
        }

        case OP_TYPE::ME_MERGE:
        {
			OP::MeshMergeArgs args = program.GetOpArgs<OP::MeshMergeArgs>(at);
            f(args.base );
            f(args.added );
            break;
        }

        case OP_TYPE::ME_INTERPOLATE:
        {
			OP::MeshInterpolateArgs args = program.GetOpArgs<OP::MeshInterpolateArgs>(at);
            f(args.factor );
            f(args.base );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1;++t)
            {
                f(args.targets[t] );
            }
            break;
        }

        case OP_TYPE::ME_MASKCLIPMESH:
        {
			OP::MeshMaskClipMeshArgs args = program.GetOpArgs<OP::MeshMaskClipMeshArgs>(at);
            f(args.source );
            f(args.clip );
            break;
        }

        case OP_TYPE::ME_MASKDIFF:
        {
			OP::MeshMaskDiffArgs args = program.GetOpArgs<OP::MeshMaskDiffArgs>(at);
            f(args.source );
            f(args.fragment );
            break;
        }

        case OP_TYPE::ME_SUBTRACT:
        {
			OP::MeshSubtractArgs args = program.GetOpArgs<OP::MeshSubtractArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        case OP_TYPE::ME_REMOVEMASK:
        {
            const uint8_t* data = program.GetOpArgsPointer(at);
            mu::OP::ADDRESS source;
            memcpy( &source, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
            f(source);

            uint16 removes = 0;
            memcpy( &removes, data, sizeof(uint16) ); data+=sizeof(uint16);
            for (uint16 r=0; r<removes; ++r)
            {
                mu::OP::ADDRESS condition;
                memcpy( &condition, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(condition);

                mu::OP::ADDRESS mask;
                memcpy( &mask, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(mask);
            }
            break;
        }

        case OP_TYPE::ME_FORMAT:
        {
			OP::MeshFormatArgs args = program.GetOpArgs<OP::MeshFormatArgs>(at);
            f(args.source );
            f(args.format );
            break;
        }

        case OP_TYPE::ME_TRANSFORM:
        {
			OP::MeshTransformArgs args = program.GetOpArgs<OP::MeshTransformArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::ME_EXTRACTLAYOUTBLOCK:
        {
            const uint8_t* data = program.GetOpArgsPointer(at);
            mu::OP::ADDRESS source;
            memcpy( &source, data, sizeof(OP::ADDRESS) );
            f(source);
            break;
        }

        case OP_TYPE::ME_EXTRACTFACEGROUP:
        {
			OP::MeshExtractFaceGroupArgs args = program.GetOpArgs<OP::MeshExtractFaceGroupArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::ME_CLIPMORPHPLANE:
        {
			OP::MeshClipMorphPlaneArgs args = program.GetOpArgs<OP::MeshClipMorphPlaneArgs>(at);
            f(args.source);
            break;
        }

        case OP_TYPE::ME_CLIPWITHMESH :
        {
			OP::MeshClipWithMeshArgs args = program.GetOpArgs<OP::MeshClipWithMeshArgs>(at);
            f(args.source);
            f(args.clipMesh);
            break;
        }

        case OP_TYPE::ME_CLIPDEFORM:
        {
			OP::MeshClipDeformArgs args = program.GetOpArgs<OP::MeshClipDeformArgs>(at);
            f(args.mesh);
            f(args.clipShape);
            break;
        }
       
		case OP_TYPE::ME_MORPHRESHAPE:
		{
			OP::MeshMorphReshapeArgs Args = program.GetOpArgs<OP::MeshMorphReshapeArgs>(at);
			f(Args.Morph);
			f(Args.Reshape);

			break;
		}
 	
        case OP_TYPE::ME_REMAPINDICES :
        {
			OP::MeshRemapIndicesArgs args = program.GetOpArgs<OP::MeshRemapIndicesArgs>(at);
            f(args.source);
            f(args.reference);
            break;
        }

        case OP_TYPE::ME_SETSKELETON :
        {
			OP::MeshSetSkeletonArgs args = program.GetOpArgs<OP::MeshSetSkeletonArgs>(at);
            f(args.source);
            f(args.skeleton);
            break;
        }

        case OP_TYPE::ME_PROJECT :
        {
			OP::MeshProjectArgs args = program.GetOpArgs<OP::MeshProjectArgs>(at);
            f(args.mesh);
            f(args.projector);
            break;
        }

		case OP_TYPE::ME_APPLYPOSE:
		{
			OP::MeshApplyPoseArgs args = program.GetOpArgs<OP::MeshApplyPoseArgs>(at);
			f(args.base);
			f(args.pose);
			break;
		}

		case OP_TYPE::ME_GEOMETRYOPERATION:
		{
			OP::MeshGeometryOperationArgs args = program.GetOpArgs<OP::MeshGeometryOperationArgs>(at);
			f(args.meshA);
			f(args.meshB);
			f(args.scalarA);
			f(args.scalarB);
			break;
		}

		case OP_TYPE::ME_BINDSHAPE:
		{
			OP::MeshBindShapeArgs args = program.GetOpArgs<OP::MeshBindShapeArgs>(at);
			f(args.mesh);
			f(args.shape);
			break;
		}

		case OP_TYPE::ME_APPLYSHAPE:
		{
			OP::MeshApplyShapeArgs args = program.GetOpArgs<OP::MeshApplyShapeArgs>(at);
			f(args.mesh);
			f(args.shape);
			break;
		}

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IN_ADDMESH:
        case OP_TYPE::IN_ADDIMAGE:
        case OP_TYPE::IN_ADDVECTOR:
        case OP_TYPE::IN_ADDSCALAR:
        case OP_TYPE::IN_ADDSTRING:
        case OP_TYPE::IN_ADDCOMPONENT:
        case OP_TYPE::IN_ADDSURFACE:
        {
			OP::InstanceAddArgs args = program.GetOpArgs<OP::InstanceAddArgs>(at);
            f(args.instance );
            f(args.value );
            break;
        }

        case OP_TYPE::IN_ADDLOD:
        {
			OP::InstanceAddLODArgs args = program.GetOpArgs<OP::InstanceAddLODArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_ADD_COUNT;++t)
            {
                f(args.lod[t] );
            }
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::LA_PACK:
        {
			OP::LayoutPackArgs args = program.GetOpArgs<OP::LayoutPackArgs>(at);
            f(args.layout );
            break;
        }

        case OP_TYPE::LA_MERGE:
        {
			OP::LayoutMergeArgs args = program.GetOpArgs<OP::LayoutMergeArgs>(at);
            f(args.base );
            f(args.added );
            break;
        }

        case OP_TYPE::LA_REMOVEBLOCKS:
        {
			OP::LayoutRemoveBlocksArgs args = program.GetOpArgs<OP::LayoutRemoveBlocksArgs>(at);
            f(args.source );
            f(args.mesh );
            break;
        }

        default:
			check( false );
            break;
        }

    }


}

