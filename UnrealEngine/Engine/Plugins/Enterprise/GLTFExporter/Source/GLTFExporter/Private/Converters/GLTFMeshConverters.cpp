// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFMeshConverters.h"
#include "Converters/GLTFMeshUtility.h"
#include "Converters/GLTFMaterialUtility.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Tasks/GLTFDelayedMeshTasks.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

void FGLTFStaticMeshConverter::Sanitize(const UStaticMesh*& StaticMesh, const UStaticMeshComponent*& StaticMeshComponent, FGLTFMaterialArray& Materials, int32& LODIndex)
{
	FGLTFMeshUtility::ResolveMaterials(Materials, StaticMeshComponent, StaticMesh);
	Builder.ResolveProxies(Materials);

	LODIndex = Builder.SanitizeLOD(StaticMesh, StaticMeshComponent, LODIndex);

	if (StaticMeshComponent != nullptr)
	{
		const bool bUsesMeshData = Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::UseMeshData &&
			FGLTFMaterialUtility::NeedsMeshData(Materials); // TODO: if this expensive, cache the results for each material

		const bool bIsReferencedByVariant = Builder.GetObjectVariants(StaticMeshComponent) != nullptr;

		// Only use the component if it's needed for baking or variants, since we would
		// otherwise export a copy of this mesh for each mesh-component.
		if (!bUsesMeshData && !bIsReferencedByVariant)
		{
			StaticMeshComponent = nullptr;
		}
	}
}

FGLTFJsonMesh* FGLTFStaticMeshConverter::Convert(const UStaticMesh* StaticMesh,  const UStaticMeshComponent* StaticMeshComponent, FGLTFMaterialArray Materials, int32 LODIndex)
{
#if !WITH_EDITOR
	if (!StaticMesh->bAllowCPUAccess)
	{
		Builder.LogSuggestion(FString::Printf(
			TEXT("Export of mesh %s can in runtime be speed-up by checking 'Allow CPU Access' in asset settings"),
			*StaticMesh->GetName()));
	}
#endif

	const int32 MaterialCount = FGLTFMeshUtility::GetMaterials(StaticMesh).Num();
	FGLTFJsonMesh* JsonMesh = Builder.AddMesh();
	JsonMesh->Primitives.AddDefaulted(MaterialCount);

	Builder.ScheduleSlowTask<FGLTFDelayedStaticMeshTask>(Builder, MeshSectionConverter, StaticMesh, StaticMeshComponent, Materials, LODIndex, JsonMesh);
	return JsonMesh;
}

void FGLTFSkeletalMeshConverter::Sanitize(const USkeletalMesh*& SkeletalMesh, const USkeletalMeshComponent*& SkeletalMeshComponent, FGLTFMaterialArray& Materials, int32& LODIndex)
{
	FGLTFMeshUtility::ResolveMaterials(Materials, SkeletalMeshComponent, SkeletalMesh);
	Builder.ResolveProxies(Materials);

	LODIndex = Builder.SanitizeLOD(SkeletalMesh, SkeletalMeshComponent, LODIndex);

	if (SkeletalMeshComponent != nullptr)
	{
		const bool bUsesMeshData = Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::UseMeshData &&
			FGLTFMaterialUtility::NeedsMeshData(Materials); // TODO: if this expensive, cache the results for each material

		const bool bIsReferencedByVariant = Builder.GetObjectVariants(SkeletalMeshComponent) != nullptr;

		// Only use the component if it's needed for baking or variants, since we would
		// otherwise export a copy of this mesh for each mesh-component.
		if (!bUsesMeshData && !bIsReferencedByVariant)
		{
			SkeletalMeshComponent = nullptr;
		}
	}
}

FGLTFJsonMesh* FGLTFSkeletalMeshConverter::Convert(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent, FGLTFMaterialArray Materials, int32 LODIndex)
{
#if !WITH_EDITOR
	if (!SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess)
	{
		Builder.LogSuggestion(FString::Printf(
			TEXT("Export of mesh %s (LOD %d) can in runtime be speed-up by checking 'Allow CPU Access' in asset settings"),
			*SkeletalMesh->GetName(),
			LODIndex));
	}
#endif

	const int32 MaterialCount = FGLTFMeshUtility::GetMaterials(SkeletalMesh).Num();
	FGLTFJsonMesh* JsonMesh = Builder.AddMesh();
	JsonMesh->Primitives.AddDefaulted(MaterialCount);

	Builder.ScheduleSlowTask<FGLTFDelayedSkeletalMeshTask>(Builder, MeshSectionConverter, SkeletalMesh, SkeletalMeshComponent, Materials, LODIndex, JsonMesh);
	return JsonMesh;
}
