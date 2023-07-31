// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureBuilder.h"

#include "RenderUtils.h"
#include "VT/VirtualTexture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VirtualTextureBuilder)

UVirtualTextureBuilder::UVirtualTextureBuilder(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
}

UVirtualTextureBuilder::~UVirtualTextureBuilder()
{
}

void UVirtualTextureBuilder::Serialize(FArchive& Ar)
{
	if (Ar.IsCooking() && Ar.IsSaving() && !UseVirtualTexturing(GMaxRHIFeatureLevel, Ar.CookingTarget()))
	{
		// Clear Texture during cook for platforms that don't support virtual texturing
		UVirtualTexture2D* TextureBackup = Texture;
		Texture = nullptr;
		Super::Serialize(Ar);
		Texture = TextureBackup;
	}
	else
	{
		Super::Serialize(Ar);
	}
}

#if WITH_EDITOR

void UVirtualTextureBuilder::BuildTexture(FVirtualTextureBuildDesc const& BuildDesc)
{
	BuildHash = BuildDesc.BuildHash;

	Texture = NewObject<UVirtualTexture2D>(this, TEXT("Texture"));
	Texture->VirtualTextureStreaming = true;
	Texture->LODGroup = BuildDesc.LODGroup;

	Texture->Settings.Init();
	Texture->Settings.TileSize = BuildDesc.TileSize;
	Texture->Settings.TileBorderSize = BuildDesc.TileBorderSize;
	Texture->LossyCompressionAmount = BuildDesc.LossyCompressionAmount;

	Texture->bContinuousUpdate = BuildDesc.bContinuousUpdate;
	Texture->bSinglePhysicalSpace = BuildDesc.bSinglePhysicalSpace;

	for (int32 Layer = 0; Layer < BuildDesc.LayerCount; Layer++)
	{
		Texture->SetLayerFormatSettings(Layer, BuildDesc.LayerFormatSettings[Layer]);
	}

	Texture->Source.InitLayered(BuildDesc.InSizeX, BuildDesc.InSizeY, 1, BuildDesc.LayerCount, 1, BuildDesc.LayerFormats.GetData(), BuildDesc.InData);
	Texture->PostEditChange();
}

#endif

