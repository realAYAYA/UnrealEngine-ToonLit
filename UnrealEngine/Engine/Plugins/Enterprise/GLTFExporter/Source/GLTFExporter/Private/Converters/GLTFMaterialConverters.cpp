// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFMaterialConverters.h"
#include "Converters/GLTFMaterialUtility.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Tasks/GLTFDelayedMaterialTasks.h"

void FGLTFMaterialConverter::Sanitize(const UMaterialInterface*& Material, const FGLTFMeshData*& MeshData, FGLTFIndexArray& SectionIndices)
{
	Material = Builder.ResolveProxy(Material);

	if (MeshData == nullptr ||
		Builder.ExportOptions->BakeMaterialInputs != EGLTFMaterialBakeMode::UseMeshData ||
		!FGLTFMaterialUtility::NeedsMeshData(Material))
	{
		MeshData = nullptr;
		SectionIndices = {};
	}

#if WITH_EDITOR
	if (MeshData != nullptr)
	{
		const FMeshDescription& MeshDescription = MeshData->GetParent()->Description;

		const float DegenerateUVPercentage = UVDegenerateChecker.GetOrAdd(&MeshDescription, SectionIndices, MeshData->BakeUsingTexCoord);
		if (FMath::IsNearlyEqual(DegenerateUVPercentage, 1))
		{
			FString SectionString = TEXT("mesh section");
			SectionString += SectionIndices.Num() > 1 ? TEXT("s ") : TEXT(" ");
			SectionString += FString::JoinBy(SectionIndices, TEXT(", "), FString::FromInt);

			Builder.LogWarning(FString::Printf(
				TEXT("Material %s uses mesh data from %s but the lightmap UVs (channel %d) are nearly 100%% degenerate (in %s). Simple baking will be used as fallback"),
				*Material->GetName(),
				*MeshData->GetParent()->Name,
				MeshData->BakeUsingTexCoord,
				*SectionString));

			MeshData = nullptr;
			SectionIndices = {};
		}
	}
#endif
}

FGLTFJsonMaterial* FGLTFMaterialConverter::Convert(const UMaterialInterface* Material, const FGLTFMeshData* MeshData, FGLTFIndexArray SectionIndices)
{
	if (Material != FGLTFMaterialUtility::GetDefaultMaterial())
	{
		FGLTFJsonMaterial* JsonMaterial = Builder.AddMaterial();
		Builder.ScheduleSlowTask<FGLTFDelayedMaterialTask>(Builder, UVOverlapChecker, Material, MeshData, SectionIndices, JsonMaterial);
		return JsonMaterial;
	}

	return nullptr; // use default gltf definition
}
