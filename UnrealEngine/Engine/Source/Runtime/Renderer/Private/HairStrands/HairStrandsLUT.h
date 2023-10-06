// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairRendering.h: Hair strands LUT generation.
=============================================================================*/

#pragma once

#include "RenderGraphFwd.h"

class FViewInfo;

enum FHairLUTType
{
	HairLUTType_DualScattering,
	HairLUTType_MeanEnergy,
	HairLUTType_Coverage,
	HairLUTTypeCount
};

/// Returns Hair LUTs. LUTs are generated on demand.
FRDGTextureRef GetHairLUT(FRDGBuilder& GraphBuilder, const FViewInfo& View, FHairLUTType Type);
