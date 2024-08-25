// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "RenderMesh_Editor.h"
#include "CoreMesh.h"
#include "MeshInfo.h"
#include "KismetProceduralMeshLibrary.h"
#include "../Helper/MathUtils.h"
#include <Engine/StaticMesh.h>
#include <StaticMeshResources.h>

RenderMesh_Editor::RenderMesh_Editor()
{
	_isPlane = false;
}

RenderMesh_Editor::RenderMesh_Editor(RenderMesh* parent, TArray<MeshInfoPtr> meshes, MaterialInfoPtr matInfo) : RenderMesh(parent, meshes, matInfo)
{

}

RenderMesh_Editor::RenderMesh_Editor(UStaticMeshComponent* staticMeshComponent,UWorld* world)
{
	_world = world;
	_meshComponents.Add(staticMeshComponent);
}

void RenderMesh_Editor::PrepareForRendering(UWorld* world, FVector scale)
{
	if (_parentMesh)
		_parentMesh->SetViewScale(scale);

	_viewScale = scale;
}

AsyncActionResultPtr RenderMesh_Editor::Load()
{
	// Need to figure out how we are going to support MTS and UDIM meshes.
	_meshSplitType = MeshSplitType::Single;

	//_meshActor->GetComponents<UStaticMeshComponent>(_meshComponents);
	if (_meshComponents.Num() <= 0)
		return cti::make_exceptional_continuable<ActionResultPtr>(std::make_exception_ptr(std::runtime_error("No mesh components found on actor")));

	return cti::make_continuable<ActionResultPtr>([this](auto&& promise)
		{
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, promise = std::forward<decltype(promise)>(promise)]() mutable
			{
				try
				{
					LoadInternal();
					promise.set_value(std::make_shared<ActionResult>());
				}
				catch (cti::exception_t ex)
				{
					promise.set_exception(ex);
				}
			});
		});
}

void RenderMesh_Editor::SetMaterial(UMaterialInterface* material)
{
	for (int meshIndex = 0; meshIndex < _meshComponents.Num(); meshIndex++)
	{
		UStaticMeshComponent* meshComponent = _meshComponents[meshIndex];
		if (meshComponent->IsValidLowLevel())
		{
			MeshInfoPtr meshInfo = _meshes[meshIndex];
			meshComponent->SetMaterial(meshInfo->GetMaterialIndex(), material);
		}
	}
}

void RenderMesh_Editor::LoadInternal()
{
	for (UStaticMeshComponent* meshComponent : _meshComponents)
	{
		LoadSingleMeshComponent(*meshComponent);
	}
}
void RenderMesh_Editor::LoadSingleMeshComponent(const UStaticMeshComponent& meshComponent)
{
	UStaticMesh* staticMesh = meshComponent.GetStaticMesh();

	//Set bAllowCPUAccess to true as it is required to use  GetSectionFromStaticMesh
	staticMesh->bAllowCPUAccess = true;

	const int32 LODindex = 0;
	int32 numSections = staticMesh->GetNumSections(LODindex);

	// Return if RenderData is invalid
	if (!staticMesh->GetRenderData())
		return;

	// No valid mesh data on lod 0 (shouldn't happen)
	if (!staticMesh->GetRenderData()->LODResources.IsValidIndex(LODindex))
		return;

	// load materials
	//const TArray<UMaterialInterface*> materialInterfaces = meshComponent.GetMaterials();
	for (int32 materialIndex = 0; materialIndex < meshComponent.GetNumMaterials(); ++materialIndex)
	{
		UMaterialInterface* materialInterface = meshComponent.GetMaterial(materialIndex);
		if (materialInterface)
		{
			FString matName;
			materialInterface->GetName(matName);
			AddMaterialInfo(materialIndex, matName);
		}
	}
	_currentMaterials.Empty();
	_currentMaterials.Append(_originalMaterials);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	_originalBounds.Init();
	
	for (int32 sectionIndex = 0; sectionIndex < numSections; sectionIndex++)
	{
		CoreMesh* cmesh = new CoreMesh();
		CoreMeshPtr cmeshPtr = CoreMeshPtr(cmesh);
		MeshInfoPtr meshInfo = std::make_shared<MeshInfo>(cmeshPtr);

		cmesh->bounds = staticMesh->GetBoundingBox();
		meshComponent.GetName(cmesh->name);

		FMeshSectionInfo sectionInfo = staticMesh->GetSectionInfoMap().Get(LODindex, sectionIndex);
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, LODindex, sectionIndex, cmesh->vertices, cmesh->triangles, cmesh->normals, cmesh->uvs, cmesh->tangents);
		cmesh->materialIndex = sectionInfo.MaterialIndex;
		
		MathUtils::EncapsulateBound(_originalBounds, cmeshPtr->bounds);	

		_meshes.Add(meshInfo);
	}
}
#endif // WITH_EDITOR