// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "3D/RenderMesh.h"
#include "Helper/Promise.h"

struct CoreMesh;
typedef std::shared_ptr<CoreMesh>	CoreMeshPtr;


class RenderMesh;

class TEXTUREGRAPHENGINE_API RenderMesh_Procedural: public RenderMesh
{
private:
	int										_tesselation = 32;
	FVector2D								_dimension= FVector2D::UnitVector;
	FVector									_offSet;
	
public:
	explicit								RenderMesh_Procedural(const MeshLoadInfo loadInfo);
	virtual AsyncActionResultPtr			Load() override;
	virtual void							LoadInternal() override;
	void									GenerateProcedural(int tesselation, FVector2D dimension, CoreMeshPtr cmesh);

	//////////////////////////////////////////////////////////////////////////
	////Inline Functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE	int&						Tesselation() { return _tesselation; }
};

typedef std::shared_ptr<RenderMesh_Procedural> RenderMesh_ProceduralPtr;