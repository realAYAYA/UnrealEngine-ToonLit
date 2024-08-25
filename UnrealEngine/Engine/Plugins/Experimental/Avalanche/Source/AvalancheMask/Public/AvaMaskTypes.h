// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryMaskTypes.h"
#include "MaterialTypes.h"

#include "AvaMaskTypes.generated.h"

class UMaterialInterface;
class UTexture;

/** Describes a component/material slot pair. */
USTRUCT()
struct FAvaMask2DComponentMaterialPath
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftComponentReference Component;

	UPROPERTY()
	int32 SlotIdx = 0;

	/** FAvaMask2DComponentMaterialPath == operator */
	bool operator==(const FAvaMask2DComponentMaterialPath& Other) const
	{
		return (Component == Other.Component) && (SlotIdx == Other.SlotIdx);
	}
};

inline uint32 GetTypeHash(const FAvaMask2DComponentMaterialPath& MaterialPath)
{
	return HashCombineFast(GetTypeHash(MaterialPath.Component), GetTypeHash(MaterialPath.SlotIdx));
}

/** Used to store, set and compare material parameter values. */
USTRUCT()
struct FAvaMask2DMaterialParameters
{
	GENERATED_BODY()

	/** Canvas/Channel name. */
	UPROPERTY()
	FName CanvasName;

	/** Mask texture. */
	UPROPERTY()
	TObjectPtr<UTexture> Texture = nullptr;

	/** Multiplies the mask texture to determine which channel to read from. */
	UPROPERTY()
	EGeometryMaskColorChannel Channel = EGeometryMaskColorChannel::Red;

	/** Multiplies the mask texture to determine which channel to read from. */
	UPROPERTY()
	FLinearColor ChannelAsVector = FLinearColor::Red;

	/** Whether the mask result should be inverted when applied. This is stored in the material as a float to allow runtime modification. */
	UPROPERTY()
	bool bInvert = false;

	/** Base opacity/alpha to use in Read mode. */
	UPROPERTY()
	float BaseOpacity = 0.0f;

	/** The render target might be larger/overscanned, so we need to compensate. */
	UPROPERTY()
	FVector2f Padding = FVector2f::Zero();

	UPROPERTY()
	bool bApplyFeathering = false;

	UPROPERTY()
	float OuterFeatherRadius = 0.0f;

	UPROPERTY()
	float InnerFeatherRadius = 0.0f;

	UPROPERTY()
	TEnumAsByte<EBlendMode> BlendMode = EBlendMode::BLEND_Opaque;

	/** Set current values from the input material and named parameters, return true if successful. */
	bool SetFromMaterial(const UMaterialInterface* InMaterial, const FMaterialParameterInfo& InTextureParameter, const FMaterialParameterInfo& InChannelParameter, const FMaterialParameterInfo& InInvertParameter, const FMaterialParameterInfo& InBaseOpacityParameter, const FMaterialParameterInfo& InPaddingParameter, const FMaterialParameterInfo& InFeatherParameter);
};

/** Encapsulates all parameters to apply to a given modifier subject. */
USTRUCT()
struct FAvaMask2DSubjectParameters
{
	GENERATED_BODY()

	UPROPERTY()
	FAvaMask2DMaterialParameters MaterialParameters;
};
