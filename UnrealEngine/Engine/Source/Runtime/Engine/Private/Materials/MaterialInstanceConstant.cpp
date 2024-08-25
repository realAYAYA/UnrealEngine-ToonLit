// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialInstanceConstant.cpp: MaterialInstanceConstant implementation.
=============================================================================*/

#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceSupport.h"
#include "MaterialCachedData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialInstanceConstant)

#if WITH_EDITOR
#include "MaterialCachedHLSLTree.h"
#include "ObjectCacheEventSink.h"
#endif

UMaterialInstanceConstant::UMaterialInstanceConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PhysMaterialMask = nullptr;
}

void UMaterialInstanceConstant::FinishDestroy()
{
	Super::FinishDestroy();
}

void UMaterialInstanceConstant::PostLoad()
{
	LLM_SCOPE(ELLMTag::Materials);
	Super::PostLoad();
}

FLinearColor UMaterialInstanceConstant::K2_GetVectorParameterValue(FName ParameterName)
{
	FLinearColor Result(0,0,0);
	Super::GetVectorParameterValue(ParameterName, Result);
	return Result;
}

float UMaterialInstanceConstant::K2_GetScalarParameterValue(FName ParameterName)
{
	float Result = 0.f;
	Super::GetScalarParameterValue(ParameterName, Result);
	return Result;
}

UTexture* UMaterialInstanceConstant::K2_GetTextureParameterValue(FName ParameterName)
{
	UTexture* Result = NULL;
	Super::GetTextureParameterValue(ParameterName, Result);
	return Result;
}

UPhysicalMaterialMask* UMaterialInstanceConstant::GetPhysicalMaterialMask() const
{
	return PhysMaterialMask;
}

#if WITH_EDITOR
void UMaterialInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ParameterStateId = FGuid::NewGuid();
}

void UMaterialInstanceConstant::SetParentEditorOnly(UMaterialInterface* NewParent, bool RecacheShader)
{
	checkf(!Parent || GIsEditor || IsRunningCommandlet(), TEXT("SetParentEditorOnly() may only be used to initialize (not change) the parent outside of the editor, GIsEditor=%d, IsRunningCommandlet()=%d"),
		GIsEditor ? 1 : 0,
		IsRunningCommandlet() ? 1 : 0);

	if (SetParentInternal(NewParent, RecacheShader))
	{
		ValidateStaticPermutationAllowed();
		UpdateCachedData();
	}
}

void UMaterialInstanceConstant::CopyMaterialUniformParametersEditorOnly(UMaterialInterface* Source, bool bIncludeStaticParams)
{
	CopyMaterialUniformParametersInternal(Source);

	if (bIncludeStaticParams && (Source != nullptr) && (Source != this))
	{
		if (UMaterialInstance* SourceMatInst = Cast<UMaterialInstance>(Source))
		{
			FStaticParameterSet SourceParamSet;
			SourceMatInst->GetStaticParameterValues(SourceParamSet);

			FStaticParameterSet MyParamSet;
			GetStaticParameterValues(MyParamSet);

			MyParamSet.StaticSwitchParameters = SourceParamSet.StaticSwitchParameters;

			UpdateStaticPermutation(MyParamSet);

			InitResources();
		}
	}
}

void UMaterialInstanceConstant::SetVectorParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, FLinearColor Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetVectorParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetScalarParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, float Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetScalarParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetScalarParameterAtlasEditorOnly(const FMaterialParameterInfo& ParameterInfo, FScalarParameterAtlasInstanceData AtlasData)
{
	check(GIsEditor || IsRunningCommandlet());
	SetScalarParameterAtlasInternal(ParameterInfo, AtlasData);
}

void UMaterialInstanceConstant::SetTextureParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, UTexture* Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetTextureParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetRuntimeVirtualTextureParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, URuntimeVirtualTexture* Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetRuntimeVirtualTextureParameterValueInternal(ParameterInfo, Value);
}

void UMaterialInstanceConstant::SetSparseVolumeTextureParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, USparseVolumeTexture* Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetSparseVolumeTextureParameterValueInternal(ParameterInfo, Value);
}

void UMaterialInstanceConstant::SetFontParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo,class UFont* FontValue,int32 FontPage)
{
	check(GIsEditor || IsRunningCommandlet());
	SetFontParameterValueInternal(ParameterInfo,FontValue,FontPage);
}

void UMaterialInstanceConstant::ClearParameterValuesEditorOnly()
{
	check(GIsEditor || IsRunningCommandlet());
	ClearParameterValuesInternal();
}

#if WITH_EDITOR
void FMaterialInstanceCachedData::InitializeForConstant(const FMaterialLayersFunctions* Layers, const FMaterialLayersFunctions* ParentLayers)
{
	const int32 NumLayers = Layers ? Layers->Layers.Num() : 0;
	ParentLayerIndexRemap.Empty(NumLayers);
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		int32 ParentLayerIndex = INDEX_NONE;
		if (ParentLayers && Layers->EditorOnly.LayerLinkStates[LayerIndex] == EMaterialLayerLinkState::LinkedToParent)
		{
			const FGuid& LayerGuid = Layers->EditorOnly.LayerGuids[LayerIndex];
			ParentLayerIndex = ParentLayers->EditorOnly.LayerGuids.Find(LayerGuid);
		}
		ParentLayerIndexRemap.Add(ParentLayerIndex);
	}
}
#endif // WITH_EDITOR

void UMaterialInstanceConstant::UpdateCachedData()
{
	// Don't need to rebuild cached data if it was serialized
	if (!bLoadedCachedData)
	{
		if (!CachedData)
		{
			CachedData.Reset(new FMaterialInstanceCachedData());
		}

		FMaterialLayersFunctions Layers;
		const bool bHasLayers = GetMaterialLayers(Layers);

		FMaterialLayersFunctions ParentLayers;
		const bool bParentHasLayers = Parent && Parent->GetMaterialLayers(ParentLayers);
		CachedData->InitializeForConstant(bHasLayers ? &Layers : nullptr, bParentHasLayers ? &ParentLayers : nullptr);
		if (Resource)
		{
			Resource->GameThread_UpdateCachedData(*CachedData);
		}
	}

	if (!bLoadedCachedExpressionData)
	{
		const bool bUsingNewHLSLGenerator = IsUsingNewHLSLGenerator();
		TUniquePtr<FMaterialCachedExpressionData> LocalCachedExpressionData;
		TUniquePtr<FMaterialCachedHLSLTree> LocalCachedTree;

		// If we have overridden material layers, need to create a local cached expression data
		// Otherwise we can leave it as null, and use cached data from our parent
		const FStaticParameterSet& LocalStaticParameters = GetStaticParameters();
		if (LocalStaticParameters.bHasMaterialLayers)
		{
			UMaterial* BaseMaterial = GetMaterial();

			FMaterialLayersFunctions MaterialLayers;
			LocalStaticParameters.GetMaterialLayers(MaterialLayers);
			if (bUsingNewHLSLGenerator)
			{
				LocalCachedTree.Reset(new FMaterialCachedHLSLTree());
				LocalCachedTree->GenerateTree(BaseMaterial, &MaterialLayers, nullptr);
			}
			else
			{
				FMaterialCachedExpressionContext Context;
				Context.LayerOverrides = &MaterialLayers;
				LocalCachedExpressionData.Reset(new FMaterialCachedExpressionData());
				LocalCachedExpressionData->UpdateForExpressions(Context, BaseMaterial->GetExpressions(), GlobalParameter, INDEX_NONE);
			}
		}

		CachedHLSLTree = MoveTemp(LocalCachedTree);

		if (bUsingNewHLSLGenerator && bHasStaticPermutationResource)
		{
			// Find the values of all overridden parameters including those overridden by parent instances. This is needed
			// for correct HLSL tree preparation where static parameter values can affect the final result. We cannot call
			// GetStaticParameterValues() here because it uses CachedExpressionData which is not ready yet
			FStaticParameterSet OverriddenStaticParameters;
#if WITH_EDITORONLY_DATA
			{
				OverriddenStaticParameters.EditorOnly.TerrainLayerWeightParameters = LocalStaticParameters.EditorOnly.TerrainLayerWeightParameters;

				FMaterialInheritanceChain InstanceChain;
				GetMaterialInheritanceChain(InstanceChain);

				// Maps a layer in parent to a layer in this instance
				TArray<int32, TInlineAllocator<16>> LayerIndexRemap;
				LayerIndexRemap.Empty(GetCachedInstanceData().ParentLayerIndexRemap.Num());
				for (int32 LayerIndex = 0; LayerIndex < GetCachedInstanceData().ParentLayerIndexRemap.Num(); ++LayerIndex)
				{
					LayerIndexRemap.Add(LayerIndex);
				}

				TMap<FMaterialParameterInfo, FMaterialParameterMetadata> OverriddenStaticParametersMap[2];
				TSet<FMaterialParameterInfo> SeenParameters[2];
				for (int32 InstanceIndex = 0; InstanceIndex < InstanceChain.MaterialInstances.Num(); ++InstanceIndex)
				{
					const UMaterialInstance* Instance = InstanceChain.MaterialInstances[InstanceIndex];
					const bool bSetOverride = InstanceIndex == 0;

					if (InstanceIndex > 0)
					{
						const UMaterialInstance* ChildInstance = InstanceChain.MaterialInstances[InstanceIndex - 1];
						const int32 NumLayers = Instance->GetCachedInstanceData().ParentLayerIndexRemap.Num();
						RemapLayersForParent(LayerIndexRemap, NumLayers, ChildInstance->GetCachedInstanceData().ParentLayerIndexRemap);
					}

					const FStaticParameterSet& InstanceStaticParameters = Instance->GetStaticParameters();
					GameThread_ApplyParameterOverrides(
						InstanceStaticParameters.StaticSwitchParameters,
						LayerIndexRemap,
						bSetOverride,
						SeenParameters[0],
						OverriddenStaticParametersMap[0],
						true);
					GameThread_ApplyParameterOverrides(
						InstanceStaticParameters.EditorOnly.StaticComponentMaskParameters,
						LayerIndexRemap,
						bSetOverride,
						SeenParameters[1],
						OverriddenStaticParametersMap[1],
						true);
				}

				OverriddenStaticParameters.AddParametersOfType(EMaterialParameterType::StaticSwitch, OverriddenStaticParametersMap[0]);
				OverriddenStaticParameters.AddParametersOfType(EMaterialParameterType::StaticComponentMask, OverriddenStaticParametersMap[1]);
			}
#endif // WITH_EDITORONLY_DATA

			check(!LocalCachedExpressionData);
			LocalCachedExpressionData.Reset(new FMaterialCachedExpressionData());
			LocalCachedExpressionData->UpdateForCachedHLSLTree(GetCachedHLSLTree(), &OverriddenStaticParameters, this);
		}

		CachedExpressionData = MoveTemp(LocalCachedExpressionData);
		if (CachedExpressionData)
		{
			EditorOnlyData->CachedExpressionData = CachedExpressionData->EditorOnlyData;
		}

		FObjectCacheEventSink::NotifyReferencedTextureChanged_Concurrent(this);
	}
}

void UMaterialInstanceConstant::SetNaniteOverrideMaterial(bool bInEnableOverride, UMaterialInterface* InOverrideMaterial)
{
	NaniteOverrideMaterial.bEnableOverride = bInEnableOverride;
	NaniteOverrideMaterial.OverrideMaterialEditor = InOverrideMaterial;
}

uint32 UMaterialInstanceConstant::ComputeAllStateCRC() const
{
	uint32 CRC = Super::ComputeAllStateCRC();
	CRC = FCrc::TypeCrc32(ParameterStateId, CRC);
	return CRC;
}

#endif // #if WITH_EDITOR
