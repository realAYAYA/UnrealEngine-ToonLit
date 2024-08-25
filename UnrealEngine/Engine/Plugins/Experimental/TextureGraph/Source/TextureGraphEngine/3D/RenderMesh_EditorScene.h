// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "RenderMesh_Editor.h"

struct CoreMesh;

class TEXTUREGRAPHENGINE_API RenderMesh_EditorScene: public RenderMesh_Editor
{
private:
	AActor*									_meshActor;				/// Actor that this rendermesh is associated to

public:
											RenderMesh_EditorScene();
											RenderMesh_EditorScene(AActor* actor);
											RenderMesh_EditorScene(RenderMesh* parent, TArray<MeshInfoPtr> meshes, MaterialInfoPtr matInfo);

	AsyncActionResultPtr					Load();
	void									PrepareForRendering(UWorld* world, FVector scale) override;
	void									RemoveActors() override;

};

typedef std::shared_ptr<RenderMesh_EditorScene> RenderMesh_EditorScenePtr;
#endif