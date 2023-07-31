// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"

namespace Lightmass
{

#if !PLATFORM_MAC && !PLATFORM_LINUX
#pragma pack(push, 1)
#endif

//----------------------------------------------------------------------------
//	Helper definitions
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//	Mesh export file header
//----------------------------------------------------------------------------
struct FMaterialFileHeader
{
	/** FourCC cookie: 'MTRL' */
	uint32		Cookie;
	FGuid		FormatVersion;
};

//----------------------------------------------------------------------------
//	Base material
//----------------------------------------------------------------------------
struct FBaseMaterialData
{
	FGuid		Guid;
};

//----------------------------------------------------------------------------
//	Material data, builds upon FBaseMaterialData
//----------------------------------------------------------------------------
/**
 *	Material blend mode.
 *	MUST MATCH UNREAL EXACTLY!!!
 */
enum EBlendMode
{
    BLEND_Opaque            =0,
    BLEND_Masked            =1,
    BLEND_Translucent       =2,
    BLEND_Additive          =3,
    BLEND_Modulate          =4,
	BLEND_AlphaComposite    =5,
	BLEND_AlphaHoldout      =6,
	BLEND_MAX               =7,
};
enum EStrataBlendMode
{
	SBM_Opaque								= 0,
	SBM_Masked								= 1,
	SBM_TranslucentGreyTransmittance		= 2,
	SBM_TranslucentColoredTransmittance		= 3,
	SBM_ColoredTransmittanceOnly			= 4,
	SBM_AlphaHoldout						= 5,
	SBM_MAX									= 6,
};

struct FMaterialData
{
	/** The BLEND mode of the material */
	EBlendMode BlendMode;
	/** The Strata BLEND mode of the material */
	EStrataBlendMode StrataBlendMode;
	/** Whether the material is two-sided or not */
	uint32 bTwoSided:1;
	/** Whether the material should cast shadows as masked even though it has a translucent blend mode. */
	uint32 bCastShadowAsMasked:1;
	uint32 bSurfaceDomain:1;
	/** Scales the emissive contribution for this material. */
	float EmissiveBoost;
	/** Scales the diffuse contribution for this material. */
	float DiffuseBoost;
	/** The clip value for masked rendering */
	float OpacityMaskClipValue;

	/** 
	 *	The sizes of the material property samples
	 */
	int32 EmissiveSize;
	int32 DiffuseSize;
	int32 TransmissionSize;
	int32 NormalSize;

	FMaterialData() :
		  StrataBlendMode(SBM_MAX)
		, EmissiveBoost(1.0f)
		, DiffuseBoost(1.0f)
		, EmissiveSize(0)
		, DiffuseSize(0)
		, TransmissionSize(0)
		, NormalSize(0)
	{
	}
};

#if !PLATFORM_MAC && !PLATFORM_LINUX
#pragma pack(pop)
#endif

}	// namespace Lightmass
