// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTextureSampler.h"

#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

FPCGElementPtr UPCGTextureSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGTextureSamplerElement>();
}

TArray<FPCGPinProperties> UPCGTextureSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultParamsLabel, EPCGDataType::Param);

	return PinProperties;
}

bool FPCGTextureSamplerElement::IsCacheable(const UPCGSettings* InSettings) const
{
	// It is currently possible to cache the texture sampler only if using an absolute transform
	// since otherwise it depends on the source component.
	// However, more tricky here is that the bUseAbsoluteTransform can be overriden, in which case we can't take a decision
	return false;
}

bool FPCGTextureSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTextureSamplerElement::Execute);

	const UPCGTextureSamplerSettings* Settings = Context->GetInputSettings<UPCGTextureSamplerSettings>();
	check(Settings);

	UPCGParamData* Params = Context->InputData.GetParams();

	FTransform Transform = PCG_GET_OVERRIDEN_VALUE(Settings, Transform, Params);
	bool bUseAbsoluteTransform = PCG_GET_OVERRIDEN_VALUE(Settings, bUseAbsoluteTransform, Params);
	//TObjectPtr<UTexture2D> PCG_OVERRIDEABLE_VALUE(Texture);
	TObjectPtr<UTexture2D> Texture = Settings->Texture;
	EPCGTextureDensityFunction DensityFunction = PCG_GET_OVERRIDEN_VALUE(Settings, DensityFunction, Params);
	EPCGTextureColorChannel ColorChannel = PCG_GET_OVERRIDEN_VALUE(Settings, ColorChannel, Params);
	float TexelSize = PCG_GET_OVERRIDEN_VALUE(Settings, TexelSize, Params);
	bool bUseAdvancedTiling = PCG_GET_OVERRIDEN_VALUE(Settings, bUseAdvancedTiling, Params);
	FVector2D Tiling = PCG_GET_OVERRIDEN_VALUE(Settings, Tiling, Params);
	FVector2D CenterOffset = PCG_GET_OVERRIDEN_VALUE(Settings, CenterOffset, Params);
	float Rotation = PCG_GET_OVERRIDEN_VALUE(Settings, Rotation, Params);
	bool bUseTileBounds = PCG_GET_OVERRIDEN_VALUE(Settings, bUseTileBounds, Params);
	FVector2D TileBoundsMin = PCG_GET_OVERRIDEN_VALUE(Settings, TileBoundsMin, Params);
	FVector2D TileBoundsMax = PCG_GET_OVERRIDEN_VALUE(Settings, TileBoundsMax, Params);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGTextureData* TextureData = NewObject<UPCGTextureData>();
	Output.Data = TextureData;

	TextureData->TargetActor = Context->SourceComponent->GetOwner();

	AActor* OriginalActor = UPCGBlueprintHelpers::GetOriginalComponent(*Context)->GetOwner();
	
	FTransform FinalTransform = Transform;

	if (!bUseAbsoluteTransform)
	{
		FTransform OriginalActorTransform = OriginalActor->GetTransform();
		FinalTransform = Transform * OriginalActorTransform;
		
		FBox OriginalActorLocalBounds = PCGHelpers::GetActorLocalBounds(OriginalActor);
		FinalTransform.SetScale3D(FinalTransform.GetScale3D() * 0.5 * (OriginalActorLocalBounds.Max - OriginalActorLocalBounds.Min));
	}

	// Initialize & set properties
	TextureData->Initialize(Texture, FinalTransform);
	TextureData->DensityFunction = DensityFunction;
	TextureData->ColorChannel = ColorChannel;
	TextureData->TexelSize = TexelSize;
	TextureData->bUseAdvancedTiling = bUseAdvancedTiling;
	TextureData->Tiling = Tiling;
	TextureData->CenterOffset = CenterOffset;
	TextureData->Rotation = Rotation;
	TextureData->bUseTileBounds = bUseTileBounds;
	TextureData->TileBounds = FBox2D(TileBoundsMin, TileBoundsMax);

	return true;
}