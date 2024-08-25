// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Mesh/InterchangeMeshHelper.h"

#include "MeshDescription.h"
#include "StaticMeshOperations.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "StaticMeshAttributes.h"

namespace UE::Interchange::Private::MeshHelper
{
	void RemapPolygonGroups(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, PolygonGroupMap& RemapPolygonGroup)
	{
		FStaticMeshConstAttributes SourceAttributes(SourceMesh);
		TPolygonGroupAttributesConstRef<FName> SourceImportedMaterialSlotNames = SourceAttributes.GetPolygonGroupMaterialSlotNames();

		FStaticMeshAttributes TargetAttributes(TargetMesh);
		TPolygonGroupAttributesRef<FName> TargetImportedMaterialSlotNames = TargetAttributes.GetPolygonGroupMaterialSlotNames();

		for (FPolygonGroupID SourcePolygonGroupID : SourceMesh.PolygonGroups().GetElementIDs())
		{
			FPolygonGroupID TargetMatchingID = INDEX_NONE;
			for (FPolygonGroupID TargetPolygonGroupID : TargetMesh.PolygonGroups().GetElementIDs())
			{
				if (SourceImportedMaterialSlotNames[SourcePolygonGroupID] == TargetImportedMaterialSlotNames[TargetPolygonGroupID])
				{
					TargetMatchingID = TargetPolygonGroupID;
					break;
				}
			}
			if (TargetMatchingID == INDEX_NONE)
			{
				TargetMatchingID = TargetMesh.CreatePolygonGroup();
				TargetImportedMaterialSlotNames[TargetMatchingID] = SourceImportedMaterialSlotNames[SourcePolygonGroupID];
			}
			else
			{
				//Since we want to keep the sections separate we need to create a new polygongroup
				TargetMatchingID = TargetMesh.CreatePolygonGroup();
				FString NewSlotName = SourceImportedMaterialSlotNames[SourcePolygonGroupID].ToString() + TEXT("_Section") + FString::FromInt(TargetMatchingID.GetValue());
				TargetImportedMaterialSlotNames[TargetMatchingID] = FName(NewSlotName);
			}
			RemapPolygonGroup.Add(SourcePolygonGroupID, TargetMatchingID);
		}
	}
} //ns UE::Interchange::Private::MeshHelper