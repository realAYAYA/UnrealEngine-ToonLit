// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Mesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

struct FText3DPolygonGroup
{
	int32 FirstVertex;
	int32 FirstTriangle;
};

using TText3DGroupList = TArray<FText3DPolygonGroup, TFixedAllocator<static_cast<int32>(EText3DGroupType::TypeCount)>>;

class FText3DGlyph
{
public:
	FText3DGlyph();

	void Build(UStaticMesh* StaticMesh, class UMaterial* DefaultMaterial);

	FMeshDescription& GetMeshDescription();
	FStaticMeshAttributes& GetStaticMeshAttributes();
	TText3DGroupList& GetGroups();

private:
	FMeshDescription MeshDescription;
	FStaticMeshAttributes StaticMeshAttributes;
	TText3DGroupList Groups;
};
