// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskTypes.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

bool FAvaMask2DMaterialParameters::SetFromMaterial(
	const UMaterialInterface* InMaterial
	, const FMaterialParameterInfo& InTextureParameter
	, const FMaterialParameterInfo& InChannelParameter
	, const FMaterialParameterInfo& InInvertParameter
	, const FMaterialParameterInfo& InBaseOpacityParameter
	, const FMaterialParameterInfo& InPaddingParameter
	, const FMaterialParameterInfo& InFeatherParameter)
{
	if (!ensureAlways(InMaterial))
	{
		return false;
	}

	// Texture
	{
		UTexture* FoundTexture = nullptr;
		if (!InMaterial->GetTextureParameterValue(InTextureParameter, FoundTexture))
		{
			return false;
		}
		Texture = FoundTexture;
	}

	// Base Opacity
	{
		float FoundValue = 0.0f;
		if (!InMaterial->GetScalarParameterValue(InBaseOpacityParameter, FoundValue))
		{
			return false;
		}
		BaseOpacity = FoundValue;
	}

	// Channel
	{
		FLinearColor FoundValue;
		if (!InMaterial->GetVectorParameterValue(InChannelParameter, FoundValue))
		{
			return false;
		}
		ChannelAsVector = FoundValue;
		Channel = UE::GeometryMask::VectorToMaskChannel(ChannelAsVector);
	}

	// Invert
	{
		float FoundValue = 0.0f;
		if (!InMaterial->GetScalarParameterValue(InInvertParameter, FoundValue))
		{
			return false;
		}
		bInvert = FoundValue > 0.1;
	}

	// Padding
	{
		FLinearColor FoundValue;
		if (!InMaterial->GetVectorParameterValue(InPaddingParameter, FoundValue))
		{
			return false;			
		}
		Padding = FVector2f(FoundValue.R, FoundValue.G);
	}

	// Feathering
	{
		FLinearColor FoundValue;
		if (!InMaterial->GetVectorParameterValue(InFeatherParameter, FoundValue))
		{
			return false;
		}
		bApplyFeathering = !FMath::IsNearlyZero(FoundValue.R);
		OuterFeatherRadius = FoundValue.G;
		InnerFeatherRadius = FoundValue.B;
	}

	return true;
}
