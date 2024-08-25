// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialValueType.h
=============================================================================*/

#pragma once

/**
 * The types which can be used by materials.
 */
enum EMaterialValueType
{
	/** 
	 * A scalar float type.  
	 * Note that MCT_Float1 will not auto promote to any other float types, 
	 * So use MCT_Float instead for scalar expression return types.
	 */
	MCT_Float1		= 1,
	MCT_Float2		= 2,
	MCT_Float3		= 4,
	MCT_Float4		= 8,

	/** 
	 * Any size float type by definition, but this is treated as a scalar which can auto convert (by replication) to any other size float vector.
	 * Use this as the type for any scalar expressions.
	 */
	MCT_Float                 = 8|4|2|1,
	MCT_Texture2D	          = 1 << 4,
	MCT_TextureCube	          = 1 << 5,
	MCT_Texture2DArray		  = 1 << 6,
	MCT_TextureCubeArray      = 1 << 7,
	MCT_VolumeTexture         = 1 << 8,
	MCT_StaticBool            = 1 << 9,
	MCT_Unknown               = 1 << 10,
	MCT_MaterialAttributes	  = 1 << 11,
	MCT_TextureExternal       = 1 << 12,
	MCT_TextureVirtual        = 1 << 13,
	MCT_SparseVolumeTexture   = 1 << 14,
	/** MCT_SparseVolumeTexture is intentionally not (yet) included here because it differs a lot from the other texture types and may not be supported/appropriate for all MCT_Texture use cases. */
	MCT_Texture               = MCT_Texture2D | MCT_TextureCube | MCT_Texture2DArray | MCT_TextureCubeArray | MCT_VolumeTexture | MCT_TextureExternal | MCT_TextureVirtual,

	/** Used internally when sampling from virtual textures */
	MCT_VTPageTableResult     = 1 << 15,
	
	MCT_ShadingModel		  = 1 << 16,

	MCT_Substrate			  = 1 << 17,

	MCT_LWCScalar			  = 1 << 18,
	MCT_LWCVector2			  = 1 << 19,
	MCT_LWCVector3			  = 1 << 20,
	MCT_LWCVector4			  = 1 << 21,
	MCT_LWCType				  = MCT_LWCScalar | MCT_LWCVector2 | MCT_LWCVector3 | MCT_LWCVector4,

	MCT_Execution             = 1 << 22,

	/** Used for code chunks that are statements with no value, rather than expressions */
	MCT_VoidStatement         = 1 << 23,

	/** Non-static bool, only used in new HLSL translator */
	MCT_Bool				 = 1 << 24,

	/** Unsigned int types */
	MCT_UInt1				 = 1 << 25,
	MCT_UInt2				 = 1 << 26,
	MCT_UInt3				 = 1 << 27,
	MCT_UInt4				 = 1 << 28,
	MCT_UInt				 = MCT_UInt1 | MCT_UInt2 | MCT_UInt3 | MCT_UInt4,

	MCT_Numeric = MCT_Float | MCT_LWCType | MCT_Bool,
};
