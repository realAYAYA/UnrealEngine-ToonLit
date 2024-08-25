// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Operations.h"

#include "MuR/ModelPrivate.h"


namespace mu
{
    MUTABLE_IMPLEMENT_POD_SERIALISABLE(OP);
    MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(OP);
	
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    #define OP_DESC_COUNT		int32(OP_TYPE::COUNT)

    // clang-format off
    static const OP_DESC s_opDescs[OP_DESC_COUNT] =
	{ 
		// type				cached	supported base image formats
        { DT_NONE,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NONE

		{ DT_BOOL,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_CONSTANT
		{ DT_INT,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONSTANT
		{ DT_SCALAR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONSTANT
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONSTANT
		{ DT_IMAGE,			true,   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CONSTANT
		{ DT_MESH,			true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONSTANT
		{ DT_LAYOUT,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONSTANT
		{ DT_PROJECTOR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_CONSTANT
		{ DT_STRING,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_CONSTANT
		{ DT_EXTENSION_DATA,true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_CONSTANT

		{ DT_BOOL,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_PARAMETER
		{ DT_INT,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_PARAMETER
		{ DT_SCALAR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_PARAMETER
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_PARAMETER
		{ DT_PROJECTOR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_PARAMETER
		{ DT_IMAGE,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PARAMETER
		{ DT_STRING,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_PARAMETER

		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_REFERENCE

		{ DT_INT,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONDITIONAL
		{ DT_SCALAR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONDITIONAL
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONDITIONAL
		{ DT_IMAGE,			true,	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_CONDITIONAL
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONDITIONAL
		{ DT_LAYOUT,		false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONDITIONAL
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_CONDITIONAL
		{ DT_EXTENSION_DATA,false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_CONDITIONAL

		{ DT_INT,			false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_SWITCH
		{ DT_SCALAR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_SWITCH
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWITCH
		{ DT_IMAGE,			true,	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_SWITCH
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SWITCH
		{ DT_LAYOUT,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_SWITCH
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_SWITCH
		{ DT_EXTENSION_DATA,false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_SWITCH

		{ DT_BOOL,			false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_LESS
		{ DT_BOOL,			false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_EQUAL_SC_CONST
		{ DT_BOOL,			false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_AND
		{ DT_BOOL,			false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_OR
		{ DT_BOOL,			false,  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_NOT

		{ DT_SCALAR,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_MULTIPLYADD
		{ DT_SCALAR,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_ARITHMETIC
		{ DT_SCALAR,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CURVE

		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SAMPLEIMAGE
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWIZZLE
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_FROMSCALARS
		{ DT_COLOUR,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_ARITHMETIC

		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYER
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYERCOLOUR
		{ DT_IMAGE,			true,	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PIXELFORMAT
		{ DT_IMAGE,			true,	{ 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MIPMAP
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZELIKE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZEREL
		{ DT_IMAGE,			true,   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BLANKLAYOUT
		{ DT_IMAGE,			true,	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_COMPOSE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INTERPOLATE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SATURATE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LUMINANCE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SWIZZLE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_COLOURMAP
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_GRADIENT
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BINARISE
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PLAINCOLOUR
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CROP
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PATCH
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RASTERMESH
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MAKEGROWMAP
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_DISPLACE
		{ DT_IMAGE,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MULTILAYER
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INVERT
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_NORMALCOMPOSITE
		{ DT_IMAGE,			true,	{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_TRANSFORM

		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYLAYOUT
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_DIFFERENCE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPH
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MERGE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_INTERPOLATE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKCLIPMESH
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKCLIPUVMASK
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKDIFF
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_REMOVEMASK
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_FORMAT
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_EXTRACTLAYOUTBLOCK
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_TRANSFORM
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPMORPHPLANE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPWITHMESH
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SETSKELETON
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_PROJECT
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYPOSE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_GEOMETRYOPERATION
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_BINDSHAPE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYSHAPE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPDEFORM
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPHRESHAPE
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_OPTIMIZESKINNING
		{ DT_MESH,			true,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_ADDTAGS

		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDMESH
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDIMAGE
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDVECTOR
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSCALAR
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSTRING
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSURFACE
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDCOMPONENT
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDLOD
		{ DT_INSTANCE,		false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDEXTENSIONDATA

		{ DT_LAYOUT,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_PACK
		{ DT_LAYOUT,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_MERGE
		{ DT_LAYOUT,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_REMOVEBLOCKS
		{ DT_LAYOUT,		true,   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_FROMMESH
	};

    // clang-format on

    static_assert( OP_DESC_COUNT == (int)OP_TYPE::COUNT, "OperationDescMismatch" );

	//---------------------------------------------------------------------------------------------
	const OP_DESC& GetOpDesc( OP_TYPE type )
	{
        return s_opDescs[ (int32)type ];
	}


    //---------------------------------------------------------------------------------------------
    void ForEachReference( OP& op, const TFunctionRef<void(OP::ADDRESS*)> f )
    {
		// Only operations that still use ASTOpFixed should be handled here.
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
            for (int32 t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(&op.args.ColourSwizzle.sources[t] );
            }
            break;

        case OP_TYPE::CO_FROMSCALARS:
			for (int32 t = 0; t < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
			{
				f(&op.args.ColourFromScalars.v[t]);
			}
            break;

        case OP_TYPE::CO_ARITHMETIC:
            f(&op.args.ColourArithmetic.a);
            f(&op.args.ColourArithmetic.b);
            break;

        //-------------------------------------------------------------------------------------
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

        case OP_TYPE::IM_INTERPOLATE:
            f(&op.args.ImageInterpolate.factor );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT;++t)
            {
                f(&op.args.ImageInterpolate.targets[t] );
            }
            break;

        case OP_TYPE::IM_SATURATE:
            f(&op.args.ImageSaturate.base );
            f(&op.args.ImageSaturate.factor );
            break;

        case OP_TYPE::IM_LUMINANCE:
            f(&op.args.ImageLuminance.base );
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

        default:
			check( false );
            break;
        }

    }



    //---------------------------------------------------------------------------------------------
    void ForEachReference( const FProgram& program, OP::ADDRESS at, const TFunctionRef<void(OP::ADDRESS)> f )
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
		case OP_TYPE::ED_CONSTANT:
        case OP_TYPE::BO_PARAMETER:
        case OP_TYPE::NU_PARAMETER:
        case OP_TYPE::SC_PARAMETER:
        case OP_TYPE::CO_PARAMETER:
        case OP_TYPE::PR_PARAMETER:
        case OP_TYPE::IM_PARAMETER:
		case OP_TYPE::IM_REFERENCE:
			break;

        case OP_TYPE::SC_CURVE:
        {
			OP::ScalarCurveArgs args = program.GetOpArgs<OP::ScalarCurveArgs>(at);
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
		case OP_TYPE::ED_CONDITIONAL:
        {
			OP::ConditionalArgs args = program.GetOpArgs<OP::ConditionalArgs>(at);
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
		case OP_TYPE::ED_SWITCH:
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
			OP::BoolLessArgs args = program.GetOpArgs<OP::BoolLessArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_EQUAL_INT_CONST:
        {
			OP::BoolEqualScalarConstArgs args = program.GetOpArgs<OP::BoolEqualScalarConstArgs>(at);
            f(args.value );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_AND:
        case OP_TYPE::BO_OR:
        {
			OP::BoolBinaryArgs args = program.GetOpArgs<OP::BoolBinaryArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::BO_NOT:
        {
			OP::BoolNotArgs args = program.GetOpArgs<OP::BoolNotArgs>(at);
            f(args.source );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_MULTIPLYADD:
        {
			OP::ScalarMultiplyAddArgs args = program.GetOpArgs<OP::ScalarMultiplyAddArgs>(at);
            f(args.factor0 );
            f(args.factor1 );
            f(args.add );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::SC_ARITHMETIC:
        {
			OP::ArithmeticArgs args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.a );
            f(args.b );
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::CO_SAMPLEIMAGE:
        {
			OP::ColourSampleImageArgs args = program.GetOpArgs<OP::ColourSampleImageArgs>(at);
            f(args.image );
            f(args.x );
            f(args.y );
            break;
        }

        case OP_TYPE::CO_SWIZZLE:
        {
			OP::ColourSwizzleArgs args = program.GetOpArgs<OP::ColourSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case OP_TYPE::CO_FROMSCALARS:
        {
			OP::ColourFromScalarsArgs args = program.GetOpArgs<OP::ColourFromScalarsArgs>(at);
			for (int32 t = 0; t < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
			{
				f(args.v[t]);
			}
            break;
        }

        case OP_TYPE::CO_ARITHMETIC:
        {
			OP::ArithmeticArgs args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.a);
            f(args.b);
            break;
        }

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_LAYER:
        {
			OP::ImageLayerArgs args = program.GetOpArgs<OP::ImageLayerArgs>(at);
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
			OP::ImageLayerColourArgs args = program.GetOpArgs<OP::ImageLayerColourArgs>(at);
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
			OP::ImageMultiLayerArgs args = program.GetOpArgs<OP::ImageMultiLayerArgs>(at);
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
			OP::ImageNormalCompositeArgs args = program.GetOpArgs<OP::ImageNormalCompositeArgs>(at);
			f(args.base);
			f(args.normal);

			break;
		}

        case OP_TYPE::IM_PIXELFORMAT:
        {
			OP::ImagePixelFormatArgs args = program.GetOpArgs<OP::ImagePixelFormatArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_MIPMAP:
        {
			OP::ImageMipmapArgs args = program.GetOpArgs<OP::ImageMipmapArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_RESIZE:
        {
			OP::ImageResizeArgs args = program.GetOpArgs<OP::ImageResizeArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_RESIZELIKE:
        {
			OP::ImageResizeLikeArgs args = program.GetOpArgs<OP::ImageResizeLikeArgs>(at);
            f(args.source );
            f(args.sizeSource );
            break;
        }

        case OP_TYPE::IM_RESIZEREL:
        {
			OP::ImageResizeRelArgs args = program.GetOpArgs<OP::ImageResizeRelArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_BLANKLAYOUT:
        {
			OP::ImageBlankLayoutArgs args = program.GetOpArgs<OP::ImageBlankLayoutArgs>(at);
            f(args.layout );
            break;
        }

        case OP_TYPE::IM_COMPOSE:
        {
			OP::ImageComposeArgs args = program.GetOpArgs<OP::ImageComposeArgs>(at);
            f(args.layout );
            f(args.base );
            f(args.blockImage );
            f(args.mask );
            break;
        }

        case OP_TYPE::IM_INTERPOLATE:
        {
			OP::ImageInterpolateArgs args = program.GetOpArgs<OP::ImageInterpolateArgs>(at);
            f(args.factor );

            for (int t=0;t<MUTABLE_OP_MAX_INTERPOLATE_COUNT;++t)
            {
                f(args.targets[t] );
            }
            break;
        }

        case OP_TYPE::IM_SWIZZLE:
        {
			OP::ImageSwizzleArgs args = program.GetOpArgs<OP::ImageSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case OP_TYPE::IM_SATURATE:
        {
			OP::ImageSaturateArgs args = program.GetOpArgs<OP::ImageSaturateArgs>(at);
            f(args.base );
            f(args.factor );
            break;
        }

        case OP_TYPE::IM_LUMINANCE:
        {
			OP::ImageLuminanceArgs args = program.GetOpArgs<OP::ImageLuminanceArgs>(at);
            f(args.base );
            break;
        }

        case OP_TYPE::IM_COLOURMAP:
        {
			OP::ImageColourMapArgs args = program.GetOpArgs<OP::ImageColourMapArgs>(at);
            f(args.base );
            f(args.mask );
            f(args.map );
            break;
        }

        case OP_TYPE::IM_GRADIENT:
        {
			OP::ImageGradientArgs args = program.GetOpArgs<OP::ImageGradientArgs>(at);
            f(args.colour0 );
            f(args.colour1 );
            break;
        }

        case OP_TYPE::IM_BINARISE:
        {
			OP::ImageBinariseArgs args = program.GetOpArgs<OP::ImageBinariseArgs>(at);
            f(args.base );
            f(args.threshold );
            break;
        }

        case OP_TYPE::IM_PLAINCOLOUR:
        {
			OP::ImagePlainColourArgs args = program.GetOpArgs<OP::ImagePlainColourArgs>(at);
            f(args.colour );
            break;
        }

        case OP_TYPE::IM_CROP:
        {
			OP::ImageCropArgs args = program.GetOpArgs<OP::ImageCropArgs>(at);
            f(args.source );
            break;
        }

        case OP_TYPE::IM_PATCH:
        {
			OP::ImagePatchArgs args = program.GetOpArgs<OP::ImagePatchArgs>(at);
            f(args.base );
            f(args.patch );
            break;
        }

        case OP_TYPE::IM_RASTERMESH:
        {
			OP::ImageRasterMeshArgs args = program.GetOpArgs<OP::ImageRasterMeshArgs>(at);
            f(args.mesh );
            f(args.image );
            f(args.mask );
            f(args.angleFadeProperties );
            f(args.projector );
            break;
        }

        case OP_TYPE::IM_MAKEGROWMAP:
        {
			OP::ImageMakeGrowMapArgs args = program.GetOpArgs<OP::ImageMakeGrowMapArgs>(at);
            f(args.mask );
            break;
        }

        case OP_TYPE::IM_DISPLACE:
        {
			OP::ImageDisplaceArgs args = program.GetOpArgs<OP::ImageDisplaceArgs>(at);
            f(args.source );
            f(args.displacementMap );
            break;
        }

		case OP_TYPE::IM_INVERT:
		{
			OP::ImageInvertArgs args = program.GetOpArgs<OP::ImageInvertArgs>(at);
			f(args.base);
			break;
		}

		case OP_TYPE::IM_TRANSFORM:
		{
			OP::ImageTransformArgs Args = program.GetOpArgs<OP::ImageTransformArgs>(at);
			f(Args.Base);
			f(Args.OffsetX);
			f(Args.OffsetY);
			f(Args.ScaleX);
			f(Args.ScaleY);
			f(Args.Rotation);
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

        case OP_TYPE::ME_MORPH:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(FactorAt);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(BaseAt);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(TargetAt);
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
			f(args.source);
			f(args.clip);
			break;
		}

		case OP_TYPE::ME_MASKCLIPUVMASK:
		{
			OP::MeshMaskClipUVMaskArgs args = program.GetOpArgs<OP::MeshMaskClipUVMaskArgs>(at);
			f(args.Source);
			f(args.Mask);
			break;
		}

        case OP_TYPE::ME_MASKDIFF:
        {
			OP::MeshMaskDiffArgs args = program.GetOpArgs<OP::MeshMaskDiffArgs>(at);
            f(args.source );
            f(args.fragment );
            break;
        }

        case OP_TYPE::ME_REMOVEMASK:
        {
            const uint8_t* data = program.GetOpArgsPointer(at);
            mu::OP::ADDRESS source;
            FMemory::Memcpy( &source, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
            f(source);

            uint16 removes = 0;
			FMemory::Memcpy( &removes, data, sizeof(uint16) ); data+=sizeof(uint16);
            for (uint16 r=0; r<removes; ++r)
            {
                mu::OP::ADDRESS condition;
				FMemory::Memcpy( &condition, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(condition);

                mu::OP::ADDRESS mask;
				FMemory::Memcpy( &mask, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(mask);
            }
            break;
        }

		case OP_TYPE::ME_ADDTAGS:
		{
			const uint8_t* data = program.GetOpArgsPointer(at);
			mu::OP::ADDRESS source;
			FMemory::Memcpy(&source, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(source);
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

		case OP_TYPE::ME_OPTIMIZESKINNING:
		{
			OP::MeshOptimizeSkinningArgs args = program.GetOpArgs<OP::MeshOptimizeSkinningArgs>(at);
			f(args.source);
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

		case OP_TYPE::IN_ADDEXTENSIONDATA:
		{
			const OP::InstanceAddExtensionDataArgs Args = program.GetOpArgs<OP::InstanceAddExtensionDataArgs>(at);

			f(Args.Instance);
			f(Args.ExtensionData);

			break;
		}

        //-------------------------------------------------------------------------------------
        case OP_TYPE::LA_PACK:
        {
			OP::LayoutPackArgs args = program.GetOpArgs<OP::LayoutPackArgs>(at);
            f(args.Source );
            break;
        }

        case OP_TYPE::LA_MERGE:
        {
			OP::LayoutMergeArgs args = program.GetOpArgs<OP::LayoutMergeArgs>(at);
            f(args.Base );
            f(args.Added );
            break;
        }

		case OP_TYPE::LA_REMOVEBLOCKS:
		{
			OP::LayoutRemoveBlocksArgs args = program.GetOpArgs<OP::LayoutRemoveBlocksArgs>(at);
			f(args.Source);
			f(args.ReferenceLayout);
			break;
		}

		case OP_TYPE::LA_FROMMESH:
		{
			OP::LayoutFromMeshArgs args = program.GetOpArgs<OP::LayoutFromMeshArgs>(at);
			f(args.Mesh);
			break;
		}

        default:
			check( false );
            break;
        }

    }


}

