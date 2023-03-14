// Copyright Epic Games, Inc. All Rights Reserved.

#include "Glyph.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"

FText3DGlyph::FText3DGlyph() :
	StaticMeshAttributes(MeshDescription)
{
	StaticMeshAttributes.Register();
	Groups.SetNum(static_cast<int32>(EText3DGroupType::TypeCount));
	MeshDescription.ReserveNewPolygonGroups(Groups.Num());

	for (int32 Index = 0; Index < Groups.Num(); Index++)
	{
		MeshDescription.CreatePolygonGroup();
	}
}

void FText3DGlyph::Build(UStaticMesh* StaticMesh, UMaterial* DefaultMaterial)
{
	check(StaticMesh);

	for (int32 Index = 0; Index < Groups.Num(); Index++)
	{
		StaticMeshAttributes.GetPolygonGroupMaterialSlotNames()[FPolygonGroupID(Index)] = StaticMesh->AddMaterial(DefaultMaterial);
	}

	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDescription);

	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bCommitMeshDescription = true;
	Params.bFastBuild = true;
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, Params);
}

FMeshDescription& FText3DGlyph::GetMeshDescription()
{
	return MeshDescription;
}

FStaticMeshAttributes& FText3DGlyph::GetStaticMeshAttributes()
{
	return StaticMeshAttributes;
}

TText3DGroupList& FText3DGlyph::GetGroups()
{
	return Groups;
}
