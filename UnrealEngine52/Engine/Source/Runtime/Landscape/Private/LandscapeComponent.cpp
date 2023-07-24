// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInstance.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

#if WITH_EDITOR

#include "LandscapeHLODBuilder.h"

uint32 ULandscapeComponent::UndoRedoModifiedComponentCount;
TArray<ULandscapeComponent*> ULandscapeComponent::UndoRedoModifiedComponents;

#endif

FName FWeightmapLayerAllocationInfo::GetLayerName() const
{
	if (LayerInfo)
	{
		return LayerInfo->LayerName;
	}
	return NAME_None;
}

uint32 FWeightmapLayerAllocationInfo::GetHash() const
{
	uint32 Hash = PointerHash(LayerInfo);
	Hash = HashCombine(GetTypeHash(WeightmapTextureIndex), Hash);
	Hash = HashCombine(GetTypeHash(WeightmapTextureChannel), Hash);
	return Hash;
}

#if WITH_EDITOR

void FLandscapeEditToolRenderData::UpdateDebugColorMaterial(const ULandscapeComponent* const Component)
{
	Component->GetLayerDebugColorKey(DebugChannelR, DebugChannelG, DebugChannelB);
}

void FLandscapeEditToolRenderData::UpdateSelectionMaterial(int32 InSelectedType, const ULandscapeComponent* const Component)
{
	// Check selection
	if (SelectedType != InSelectedType && (SelectedType & ST_REGION) && !(InSelectedType & ST_REGION))
	{
		// Clear Select textures...
		if (DataTexture)
		{
			FLandscapeEditDataInterface LandscapeEdit(Component->GetLandscapeInfo());
			LandscapeEdit.ZeroTexture(DataTexture);
		}
	}

	SelectedType = InSelectedType;
}

void ULandscapeComponent::UpdateEditToolRenderData()
{
	FLandscapeComponentSceneProxy* LandscapeSceneProxy = (FLandscapeComponentSceneProxy*)SceneProxy;

	if (LandscapeSceneProxy != nullptr)
	{
		TArray<UMaterialInterface*> UsedMaterialsForVerification;
		const bool bGetDebugMaterials = true;
		GetUsedMaterials(UsedMaterialsForVerification, bGetDebugMaterials);

		FLandscapeEditToolRenderData LandscapeEditToolRenderData = EditToolRenderData;
		ENQUEUE_RENDER_COMMAND(UpdateEditToolRenderData)(
			[LandscapeEditToolRenderData, LandscapeSceneProxy, UsedMaterialsForVerification](FRHICommandListImmediate& RHICmdList)
			{
				LandscapeSceneProxy->EditToolRenderData = LandscapeEditToolRenderData;				
				LandscapeSceneProxy->SetUsedMaterialForVerification(UsedMaterialsForVerification);
			});
	}
}

TSubclassOf<UHLODBuilder> ULandscapeComponent::GetCustomHLODBuilderClass() const
{
	return ULandscapeHLODBuilder::StaticClass();
}

#endif
