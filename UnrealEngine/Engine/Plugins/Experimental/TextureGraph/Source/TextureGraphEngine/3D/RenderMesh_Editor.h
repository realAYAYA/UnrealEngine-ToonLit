// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "RenderMesh.h"
#include "Components/StaticMeshComponent.h"

struct CoreMesh;

class TEXTUREGRAPHENGINE_API RenderMesh_Editor: public RenderMesh
{

protected:
	TArray<UStaticMeshComponent*>			_meshComponents;
	UWorld*									_world = nullptr;
	virtual void							LoadInternal() override;
	void									LoadSingleMeshComponent(const UStaticMeshComponent& mesh);

public:
											RenderMesh_Editor();
											RenderMesh_Editor(RenderMesh* parent, TArray<MeshInfoPtr> meshes, MaterialInfoPtr matInfo);
											RenderMesh_Editor(UStaticMeshComponent* staticMeshComponent, UWorld* world);

	FORCEINLINE								TArray<UStaticMeshComponent*> GetMeshComponents() { return _meshComponents; }
	virtual AsyncActionResultPtr			Load() override;
	virtual FMatrix							LocalToWorldMatrix() const override { return _meshComponents[0]->GetComponentTransform().ToMatrixWithScale(); }
	virtual void							PrepareForRendering(UWorld* world, FVector scale) override;
	virtual void							SetMaterial(UMaterialInterface* material) override;
};

typedef std::shared_ptr<RenderMesh_Editor> RenderMesh_EditorPtr;
#endif