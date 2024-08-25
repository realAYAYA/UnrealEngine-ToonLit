// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Mesh/InterchangeStaticMeshFactory.h"

#if WITH_EDITOR
#include "BSPOps.h"
#endif
#include "Components.h"
#include "Engine/Polys.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Mesh/InterchangeMeshHelper.h"
#include "Mesh/InterchangeMeshPayloadInterface.h"
#include "MeshBudgetProjectSettings.h"
#include "Model.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeStaticMeshFactory)

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif //WITH_EDITORONLY_DATA

UClass* UInterchangeStaticMeshFactory::GetFactoryClass() const
{
	return UStaticMesh::StaticClass();
}

namespace UE::Interchange::Private::StaticMesh
{
	void ReorderMaterialSlotToBaseLod(UStaticMesh* StaticMesh)
	{
#if WITH_EDITOR
		if (!StaticMesh || !StaticMesh->IsMeshDescriptionValid(0))
		{
			return;
		}

		TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();
		if (Materials.Num() < 2)
		{
			return;
		}

		TArray<int32> RemapMaterialIndexes;
		RemapMaterialIndexes.Reserve(Materials.Num());
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			RemapMaterialIndexes.Add(INDEX_NONE);
		}
		TArray<FStaticMaterial> ReorderMaterialArray;
		ReorderMaterialArray.Reserve(Materials.Num());

		for (int32 LodIndex = 0; LodIndex < StaticMesh->GetNumSourceModels(); ++LodIndex)
		{
			const FMeshDescription* LodMeshDescription = StaticMesh->GetMeshDescription(LodIndex);
			if (!LodMeshDescription || !ensure(!LodMeshDescription->NeedsCompact()))
			{
				if (LodIndex == 0)
				{
					return; //Lod 0 must always participate in the re-order, return if we can't use it
				}
				continue;
			}

			FStaticMeshConstAttributes StaticMeshAttributes(*LodMeshDescription);
			TPolygonGroupAttributesConstRef<FName> SlotNames = StaticMeshAttributes.GetPolygonGroupMaterialSlotNames();
			for (FPolygonGroupID PolygonGroupID : LodMeshDescription->PolygonGroups().GetElementIDs())
			{
				FName ImportMaterialName = SlotNames[PolygonGroupID];
				bool bFoundMatch = false;
				for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
				{
					if (RemapMaterialIndexes[MaterialIndex] != INDEX_NONE)
					{
						//This material was already matched
						continue;
					}
					int32& RemapIndex = RemapMaterialIndexes[MaterialIndex];
					FName MaterialName = Materials[MaterialIndex].ImportedMaterialSlotName;
					if (MaterialName == ImportMaterialName)
					{
						RemapIndex = ReorderMaterialArray.Add(Materials[MaterialIndex]);
						bFoundMatch = true;
						break;
					}
				}
				//All mesh description polygon group should have a match
				ensure(bFoundMatch);
			}
		}
		//Custom LOD can add materials, so we add them at the end of the material slots
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			if (RemapMaterialIndexes[MaterialIndex] == INDEX_NONE)
			{
				RemapMaterialIndexes[MaterialIndex] = ReorderMaterialArray.Add(Materials[MaterialIndex]);
			}
		}

		check(ReorderMaterialArray.Num() == Materials.Num());

		//Reorder the static mesh material slot array
		Materials = ReorderMaterialArray;

		//Fix all the section info map MaterialIndex with the RemapMaterialIndexes
		FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
		const int32 LodCount = StaticMesh->GetNumSourceModels();
		for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
		{
			const int32 SectionCount = SectionInfoMap.GetSectionNumber(LodIndex);
			for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
			{
				FMeshSectionInfo SectionInfo = SectionInfoMap.Get(LodIndex, SectionIndex);
				SectionInfo.MaterialIndex = RemapMaterialIndexes[SectionInfo.MaterialIndex];
				SectionInfoMap.Set(LodIndex, SectionIndex, SectionInfo);
			}
		}
#endif // WITH_EDITOR
	}
} //ns UE::Interchange::Private::StaticMesh

UInterchangeFactoryBase::FImportAssetResult UInterchangeStaticMeshFactory::BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeStaticMeshFactory::BeginImportAsset_GameThread);

	FImportAssetResult ImportAssetResult;
	UStaticMesh* StaticMesh = nullptr;
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return ImportAssetResult;
	}

	const UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = Cast<UInterchangeStaticMeshFactoryNode>(Arguments.AssetNode);
	if (StaticMeshFactoryNode == nullptr)
	{
		return ImportAssetResult;
	}

	UObject* ExistingAsset = Arguments.ReimportObject;
	if (!ExistingAsset)
	{
		FSoftObjectPath ReferenceObject;
		if (StaticMeshFactoryNode->GetCustomReferenceObject(ReferenceObject))
		{
			ExistingAsset = ReferenceObject.TryLoad();
		}
	}

	// create a new static mesh or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		StaticMesh = NewObject<UStaticMesh>(Arguments.Parent, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else
	{
		//This is a reimport, we are just re-updating the source data
		StaticMesh = Cast<UStaticMesh>(ExistingAsset);

		// Clear the render data on the existing static mesh from the game thread so that we're ready to update it
		if (StaticMesh && StaticMesh->AreRenderingResourcesInitialized())
		{
			const bool bInvalidateLighting = true;
			const bool bRefreshBounds = true;
			FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(StaticMesh, bInvalidateLighting, bRefreshBounds);
			StaticMesh->ReleaseResources();
			StaticMesh->ReleaseResourcesFence.Wait();
			
			StaticMesh->SetRenderData(nullptr);
			StaticMesh->SetBodySetup(nullptr);
		}
	}
	
	if (!StaticMesh)
	{
		if (!Arguments.ReimportObject)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create StaticMesh asset %s."), *Arguments.AssetName);
		}
		return ImportAssetResult;
	}

	// create the BodySetup on the game thread
	StaticMesh->CreateBodySetup();
	
#if WITH_EDITOR
	if (!FApp::IsGame())
	{
		StaticMesh->PreEditChange(nullptr);
	}
#endif // WITH_EDITOR

	ImportAssetResult.ImportedObject = StaticMesh;
	return ImportAssetResult;
}

UInterchangeFactoryBase::FImportAssetResult UInterchangeStaticMeshFactory::ImportAsset_Async(const FImportAssetObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeStaticMeshFactory::ImportAsset_Async);

	FImportAssetResult ImportAssetResult;
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return ImportAssetResult;
	}

	UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = Cast<UInterchangeStaticMeshFactoryNode>(Arguments.AssetNode);
	if (StaticMeshFactoryNode == nullptr)
	{
		return ImportAssetResult;
	}

	UObject* StaticMeshObject = UE::Interchange::FFactoryCommon::AsyncFindObject(StaticMeshFactoryNode, GetFactoryClass(), Arguments.Parent, Arguments.AssetName);
	const bool bReimport = Arguments.ReimportObject && StaticMeshObject;

	if (!StaticMeshObject)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not import the StaticMesh asset %s because the asset does not exist."), *Arguments.AssetName);
		return ImportAssetResult;
	}

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(StaticMeshObject);
	if (!ensure(StaticMesh))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not cast to StaticMesh asset %s."), *Arguments.AssetName);
		return ImportAssetResult;
	}

	ensure(!StaticMesh->AreRenderingResourcesInitialized());

	const int32 LodCount = FMath::Min(StaticMeshFactoryNode->GetLodDataCount(), MAX_STATIC_MESH_LODS);
	if (LodCount != StaticMeshFactoryNode->GetLodDataCount())
	{
		const int32 LodCountDiff = StaticMeshFactoryNode->GetLodDataCount() - MAX_STATIC_MESH_LODS;
		UE_LOG(LogInterchangeImport, Warning, TEXT("Reached the maximum number of LODs for a Static Mesh (%d) - discarding %d LOD meshes."), MAX_STATIC_MESH_LODS, LodCountDiff);
	}
#if WITH_EDITOR
	const int32 PrevLodCount = StaticMesh->GetNumSourceModels();
	const int32 FinalLodCount = FMath::Max(PrevLodCount, LodCount);
	StaticMesh->SetNumSourceModels(FinalLodCount);
#endif // WITH_EDITOR

	// If we are reimporting, cache the existing vertex colors so they can be optionally reapplied after reimport
	TMap<FVector3f, FColor> ExisitingVertexColorData;
	if (bReimport)
	{
		StaticMesh->GetVertexColorData(ExisitingVertexColorData);
	}

	bool bKeepSectionsSeparate = false;
	StaticMeshFactoryNode->GetCustomKeepSectionsSeparate(bKeepSectionsSeparate);

	// Set material slots from imported materials
	auto UpdateOrAddStaticMaterial = [&StaticMesh, bReimport](const FName& MaterialSlotName, UMaterialInterface* MaterialInterface)
	{
		UMaterialInterface* NewMaterial = MaterialInterface ? MaterialInterface : UMaterial::GetDefaultMaterial(MD_Surface);

		FStaticMaterial* StaticMaterial = StaticMesh->GetStaticMaterials().FindByPredicate([&MaterialSlotName](const FStaticMaterial& Material) { return Material.MaterialSlotName == MaterialSlotName; });
		if (StaticMaterial)
		{
			//When we are not re-importing, we always force update the material, we should see this case when importing LODs is on since its an import.
			//When we do a re-import we update the material interface only if the current asset matching material is null and is not the default material. (this avoid touching a slot that was change by the user)
			if (!bReimport || (MaterialInterface && (!StaticMaterial->MaterialInterface || StaticMaterial->MaterialInterface == UMaterial::GetDefaultMaterial(MD_Surface))))
			{
				StaticMaterial->MaterialInterface = NewMaterial;
			}
		}
		else
		{
			int32 MaterialSlotIndex = StaticMesh->GetStaticMaterials().Emplace(NewMaterial, MaterialSlotName);
#if !WITH_EDITOR
			// UV density is not supported to be generated at runtime for now. We fake that it has been initialized so that we don't trigger ensures.
			StaticMesh->GetStaticMaterials()[MaterialSlotIndex].UVChannelData = FMeshUVChannelInfo(1.f);
#endif
		}
	};

	TMap<FString, FString> SlotMaterialDependencies;
	StaticMeshFactoryNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
	for (TPair<FString, FString>& SlotMaterialDependency : SlotMaterialDependencies)
	{
		FName MaterialSlotName = *SlotMaterialDependency.Key;

		const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(Arguments.NodeContainer->GetNode(SlotMaterialDependency.Value));
		if (!MaterialFactoryNode)
		{
			UpdateOrAddStaticMaterial(MaterialSlotName, nullptr);
			continue;
		}
		FSoftObjectPath MaterialFactoryNodeReferenceObject;
		MaterialFactoryNode->GetCustomReferenceObject(MaterialFactoryNodeReferenceObject);
		if (!MaterialFactoryNodeReferenceObject.IsValid())
		{
			UpdateOrAddStaticMaterial(MaterialSlotName, nullptr);
			continue;
		}

		UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(MaterialFactoryNodeReferenceObject.TryLoad());
		UpdateOrAddStaticMaterial(MaterialSlotName, MaterialInterface ? MaterialInterface : nullptr);
	}

	// Now import geometry for each LOD
	TArray<FString> LodDataUniqueIds;
	StaticMeshFactoryNode->GetLodDataUniqueIds(LodDataUniqueIds);
	ensure(LodDataUniqueIds.Num() >= LodCount);

	TArray<FMeshDescription>& LodMeshDescriptions = ImportAssetObjectData.LodMeshDescriptions;
	LodMeshDescriptions.SetNum(LodCount);

	bool bImportCollision = false;
	bool bImportedCustomCollision = false;
	int32 CurrentLodIndex = 0;
	for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
	{
		FString LodUniqueId = LodDataUniqueIds[LodIndex];
		const UInterchangeStaticMeshLodDataNode* LodDataNode = Cast<UInterchangeStaticMeshLodDataNode>(Arguments.NodeContainer->GetNode(LodUniqueId));
		if (!LodDataNode)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD when importing StaticMesh asset %s."), *Arguments.AssetName);
			continue;
		}

		// Add the lod mesh data to the static mesh
		FMeshDescription& LodMeshDescription = LodMeshDescriptions[CurrentLodIndex];

		FStaticMeshOperations::FAppendSettings AppendSettings;
		for (int32 ChannelIdx = 0; ChannelIdx < FStaticMeshOperations::FAppendSettings::MAX_NUM_UV_CHANNELS; ++ChannelIdx)
		{
			AppendSettings.bMergeUVChannels[ChannelIdx] = true;
		}
				

		TArray<FString> MeshUids;
		LodDataNode->GetMeshUids(MeshUids);

		// Fill the lod mesh description using all combined mesh parts
		TArray<FMeshPayload> MeshPayloads = GetMeshPayloads(Arguments, MeshUids);

		// Just move the mesh description from the first valid payload then append the rest
		bool bFirstValidMoved = false;
		for (FMeshPayload& MeshPayload : MeshPayloads)
		{
			const TOptional<UE::Interchange::FMeshPayloadData>& LodMeshPayload = MeshPayload.PayloadData.Get();
			if (!LodMeshPayload.IsSet())
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid static mesh payload key for StaticMesh asset %s."), *Arguments.AssetName);
				continue;
			}

			if (!bFirstValidMoved)
			{
				FMeshDescription& MeshDescription = const_cast<FMeshDescription&>(LodMeshPayload->MeshDescription);
				if (MeshDescription.IsEmpty())
				{
					continue;
				}
				LodMeshDescription = MoveTemp(MeshDescription);
				bFirstValidMoved = true;
			}
			else
			{
				if (LodMeshPayload->MeshDescription.IsEmpty())
				{
					continue;
				}
				if (bKeepSectionsSeparate)
				{
					AppendSettings.PolygonGroupsDelegate = FAppendPolygonGroupsDelegate::CreateLambda([](const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, PolygonGroupMap& RemapPolygonGroup)
						{
							UE::Interchange::Private::MeshHelper::RemapPolygonGroups(SourceMesh, TargetMesh, RemapPolygonGroup);
						});
				}
				FStaticMeshOperations::AppendMeshDescription(LodMeshPayload->MeshDescription, LodMeshDescription, AppendSettings);
			}
		}

		// Manage vertex color
		// Replace -> do nothing, we want to use the translated source data
		// Ignore -> remove vertex color from import data (when we re-import, ignore have to put back the current mesh vertex color)
		// Override -> replace the vertex color by the override color
		// @todo: new mesh description attribute for painted vertex colors?
		{
			FStaticMeshAttributes Attributes(LodMeshDescription);
			TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
			bool bReplaceVertexColor = false;
			StaticMeshFactoryNode->GetCustomVertexColorReplace(bReplaceVertexColor);
			if (!bReplaceVertexColor)
			{
				bool bIgnoreVertexColor = false;
				StaticMeshFactoryNode->GetCustomVertexColorIgnore(bIgnoreVertexColor);
				if (bIgnoreVertexColor)
				{
					for (const FVertexInstanceID& VertexInstanceID : LodMeshDescription.VertexInstances().GetElementIDs())
					{
						//If we have old vertex color (reimport), we want to keep it if the option is ignore
						if (ExisitingVertexColorData.Num() > 0)
						{
							const FVector3f& VertexPosition = LodMeshDescription.GetVertexPosition(LodMeshDescription.GetVertexInstanceVertex(VertexInstanceID));
							const FColor* PaintedColor = ExisitingVertexColorData.Find(VertexPosition);
							if (PaintedColor)
							{
								// A matching color for this vertex was found
								VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(*PaintedColor));
							}
							else
							{
								//Flush the vertex color
								VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(FColor::White));
							}
						}
						else
						{
							//Flush the vertex color
							VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(FColor::White));
						}
					}
				}
				else
				{
					FColor OverrideVertexColor;
					if (StaticMeshFactoryNode->GetCustomVertexColorOverride(OverrideVertexColor))
					{
						for (const FVertexInstanceID& VertexInstanceID : LodMeshDescription.VertexInstances().GetElementIDs())
						{
							VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(OverrideVertexColor));
						}
					}
				}
			}
		}

		// Import collision geometry
		if (CurrentLodIndex == 0)
		{
			LodDataNode->GetImportCollision(bImportCollision);
			if(bImportCollision)
			{
				bImportedCustomCollision |= ImportBoxCollision(Arguments, StaticMesh, LodDataNode);
				bImportedCustomCollision |= ImportCapsuleCollision(Arguments, StaticMesh, LodDataNode);
				bImportedCustomCollision |= ImportSphereCollision(Arguments, StaticMesh, LodDataNode);
				bImportedCustomCollision |= ImportConvexCollision(Arguments, StaticMesh, LodDataNode);
			}
		}

		CurrentLodIndex++;
	}

#if WITH_EDITOR
	{
		const bool bIsAReimport = Arguments.ReimportObject != nullptr;
		SetupSourceModelsSettings(*StaticMesh, LodMeshDescriptions, PrevLodCount, FinalLodCount, bIsAReimport);

		// SetupSourceModelsSettings can change the destination lightmap UV index
		// Make sure the destination lightmap UV index on the factory node takes
		// in account the potential change
		int32 FactoryDstLightmapIndex;
		if (StaticMeshFactoryNode->GetCustomDstLightmapIndex(FactoryDstLightmapIndex) && StaticMesh->GetLightMapCoordinateIndex() > FactoryDstLightmapIndex)
		{
			StaticMeshFactoryNode->SetCustomDstLightmapIndex(StaticMesh->GetLightMapCoordinateIndex());
		}
	}
#endif // WITH_EDITOR

	ImportAssetObjectData.bImportCollision = bImportCollision;
	ImportAssetObjectData.bImportedCustomCollision = bImportedCustomCollision;

	// Getting the file Hash will cache it into the source data
	Arguments.SourceData->GetFileContentHash();

	BuildFromMeshDescriptions(*StaticMesh);

	ImportAssetResult.ImportedObject = StaticMeshObject;
	return ImportAssetResult;
}

UInterchangeFactoryBase::FImportAssetResult UInterchangeStaticMeshFactory::EndImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeStaticMeshFactory::EndImportAsset_GameThread);

	FImportAssetResult ImportAssetResult;
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return ImportAssetResult;
	}

	UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = Cast<UInterchangeStaticMeshFactoryNode>(Arguments.AssetNode);
	if (StaticMeshFactoryNode == nullptr)
	{
		return ImportAssetResult;
	}

	const UClass* StaticMeshClass = StaticMeshFactoryNode->GetObjectClass();
	check(StaticMeshClass && StaticMeshClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	const bool bReimport = Arguments.ReimportObject && ExistingAsset;

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(ExistingAsset);
	if (!ensure(StaticMesh))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not create StaticMesh asset %s."), *Arguments.AssetName);
		return ImportAssetResult;
	}

	if (FApp::IsGame())
	{
		if (!Arguments.ReimportObject)
		{
			// Apply all StaticMeshFactoryNode custom attributes to the static mesh asset
			StaticMeshFactoryNode->ApplyAllCustomAttributeToObject(StaticMesh);
		}

		ImportAssetResult.ImportedObject = StaticMesh;
		return ImportAssetResult;
	}

	for (int32 LodIndex = 0; LodIndex < ImportAssetObjectData.LodMeshDescriptions.Num(); ++LodIndex)
	{
		// Add the lod mesh data to the static mesh
		FMeshDescription& LodMeshDescription = ImportAssetObjectData.LodMeshDescriptions[LodIndex];
		if (LodMeshDescription.IsEmpty())
		{
			//All the valid Mesh description are at the beginning of the array
			break;
		}

		// Build section info map from materials
		FStaticMeshConstAttributes StaticMeshAttributes(LodMeshDescription);
		TPolygonGroupAttributesRef<const FName> SlotNames = StaticMeshAttributes.GetPolygonGroupMaterialSlotNames();
#if WITH_EDITOR
		if (bReimport)
		{
			//Match the existing section info map data

			//First find the old mesh description polygon groups name that match with the imported mesh description polygon groups name.
			//Copy the data
			const int32 PreviousSectionCount = StaticMesh->GetSectionInfoMap().GetSectionNumber(LodIndex);
			TMap<FPolygonGroupID, FPolygonGroupID> ImportedToOldPolygonGroupMatch;
			ImportedToOldPolygonGroupMatch.Reserve(LodMeshDescription.PolygonGroups().Num());
			if (StaticMesh->IsMeshDescriptionValid(LodIndex))
			{
				//Match incoming mesh description with the old mesh description
				FMeshDescription& OldMeshDescription = *StaticMesh->GetMeshDescription(LodIndex);
				FStaticMeshConstAttributes OldStaticMeshAttributes(OldMeshDescription);
				TPolygonGroupAttributesRef<const FName> OldSlotNames = OldStaticMeshAttributes.GetPolygonGroupMaterialSlotNames();
				for (FPolygonGroupID PolygonGroupID : LodMeshDescription.PolygonGroups().GetElementIDs())
				{
					for (FPolygonGroupID OldPolygonGroupID : OldMeshDescription.PolygonGroups().GetElementIDs())
					{
						if (SlotNames[PolygonGroupID] == OldSlotNames[OldPolygonGroupID])
						{
							ImportedToOldPolygonGroupMatch.FindOrAdd(PolygonGroupID) = OldPolygonGroupID;
							break;
						}
					}
				}
			}
			//Create a new set of mesh section info for this lod
			TArray<FMeshSectionInfo> NewSectionInfoMapData;
			NewSectionInfoMapData.Reserve(LodMeshDescription.PolygonGroups().Num());
			for (FPolygonGroupID PolygonGroupID : LodMeshDescription.PolygonGroups().GetElementIDs())
			{
				if (FPolygonGroupID* OldPolygonGroupID = ImportedToOldPolygonGroupMatch.Find(PolygonGroupID))
				{
					if (StaticMesh->GetSectionInfoMap().IsValidSection(LodIndex, OldPolygonGroupID->GetValue()))
					{
						NewSectionInfoMapData.Add(StaticMesh->GetSectionInfoMap().Get(LodIndex, OldPolygonGroupID->GetValue()));
					}
				}
				else
				{
					//This is an unmatched section, its either added or we did not recover the name
					int32 MaterialSlotIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(SlotNames[PolygonGroupID]);
					//Missing material slot should have been added before
					ensure(MaterialSlotIndex != INDEX_NONE);
					NewSectionInfoMapData.Add(FMeshSectionInfo(MaterialSlotIndex));
				}
			}

			//Clear all section for this LOD
			for (int32 PreviousSectionIndex = 0; PreviousSectionIndex < PreviousSectionCount; ++PreviousSectionIndex)
			{
				StaticMesh->GetSectionInfoMap().Remove(LodIndex, PreviousSectionIndex);
			}
			//Recreate the new section info map
			for (int32 NewSectionIndex = 0; NewSectionIndex < NewSectionInfoMapData.Num(); ++NewSectionIndex)
			{
				StaticMesh->GetSectionInfoMap().Set(LodIndex, NewSectionIndex, NewSectionInfoMapData[NewSectionIndex]);
			}
		}
		else
#endif // WITH_EDITOR
		{
			int32 SectionIndex = 0;
			for (FPolygonGroupID PolygonGroupID : LodMeshDescription.PolygonGroups().GetElementIDs())
			{
				int32 MaterialSlotIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(SlotNames[PolygonGroupID]);

				// If no material was found with this slot name, fill out a blank slot instead.
				if (MaterialSlotIndex == INDEX_NONE)
				{
					MaterialSlotIndex = StaticMesh->GetStaticMaterials().Emplace(UMaterial::GetDefaultMaterial(MD_Surface), SlotNames[PolygonGroupID]);
#if !WITH_EDITOR
					StaticMesh->GetStaticMaterials()[MaterialSlotIndex].UVChannelData = FMeshUVChannelInfo(1.f);
#endif
				}

#if WITH_EDITOR
				FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(LodIndex, SectionIndex);
				Info.MaterialIndex = MaterialSlotIndex;
				StaticMesh->GetSectionInfoMap().Remove(LodIndex, SectionIndex);
				StaticMesh->GetSectionInfoMap().Set(LodIndex, SectionIndex, Info);
#endif

				SectionIndex++;
			}
		}
	}

	CommitMeshDescriptions(*StaticMesh);

	ImportSockets(Arguments, StaticMesh, StaticMeshFactoryNode);

	if (!Arguments.ReimportObject)
	{
		// Apply all StaticMeshFactoryNode custom attributes to the static mesh asset
		StaticMeshFactoryNode->ApplyAllCustomAttributeToObject(StaticMesh);
	}
#if WITH_EDITOR
	else
	{
		//Apply the re import strategy 
		UInterchangeAssetImportData* InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(StaticMesh->GetAssetImportData());
		UInterchangeFactoryBaseNode* PreviousNode = nullptr;
		if (InterchangeAssetImportData)
		{
			PreviousNode = InterchangeAssetImportData->GetStoredFactoryNode(InterchangeAssetImportData->NodeUniqueID);
		}
		UInterchangeFactoryBaseNode* CurrentNode = NewObject<UInterchangeStaticMeshFactoryNode>(GetTransientPackage());
		UInterchangeBaseNode::CopyStorage(StaticMeshFactoryNode, CurrentNode);
		CurrentNode->FillAllCustomAttributeFromObject(StaticMesh);
		UE::Interchange::FFactoryCommon::ApplyReimportStrategyToAsset(StaticMesh, PreviousNode, CurrentNode, StaticMeshFactoryNode);

		//Reorder the Hires mesh description in the same order has the lod 0 mesh description
		if (StaticMesh->IsHiResMeshDescriptionValid())
		{
			FMeshDescription* HiresMeshDescription = StaticMesh->GetHiResMeshDescription();
			FMeshDescription* Lod0MeshDescription = StaticMesh->GetMeshDescription(0);
			if (HiresMeshDescription && Lod0MeshDescription)
			{
				StaticMesh->ModifyHiResMeshDescription();
				FString MaterialNameConflictMsg = TEXT("[Asset ") + StaticMesh->GetPathName() + TEXT("] Nanite high-resolution import has material names that differ from the LOD 0 material name. Your Nanite high-resolution mesh should use the same material names the LOD 0 uses to ensure the sections can be remapped in the same order.");
				FString MaterialCountConflictMsg = TEXT("[Asset ") + StaticMesh->GetPathName() + TEXT("] Nanite high-resolution import doesn't have the same material count as LOD 0. Your Nanite high-resolution mesh should have the same number of materials as LOD 0.");
				FStaticMeshOperations::ReorderMeshDescriptionPolygonGroups(*Lod0MeshDescription, *HiresMeshDescription, MaterialNameConflictMsg, MaterialCountConflictMsg);
				StaticMesh->CommitHiResMeshDescription();
			}
		}
	}
#endif // WITH_EDITOR

	if(ImportAssetObjectData.bImportCollision)
	{
		if (!ImportAssetObjectData.bImportedCustomCollision)
		{
			GenerateKDopCollision(StaticMesh);
		}
#if WITH_EDITORONLY_DATA
		else
		{
			StaticMesh->bCustomizedCollision = true;
		}
#endif // WITH_EDITORONLY_DATA
	}
#if WITH_EDITOR
	//Lod group need to use the static mesh API and cannot use the apply delegate
	if (!Arguments.ReimportObject)
	{
		FName LodGroup = NAME_None;
		if (StaticMeshFactoryNode->GetCustomLODGroup(LodGroup) && LodGroup != NAME_None)
		{
			constexpr bool bRebuildImmediately = false;
			constexpr bool bAllowModify = false;
			StaticMesh->SetLODGroup(LodGroup, bRebuildImmediately, bAllowModify);
		}
	}
	FMeshBudgetProjectSettingsUtils::SetLodGroupForStaticMesh(StaticMesh);
#endif

	if (bReimport)
	{
		UE::Interchange::Private::StaticMesh::ReorderMaterialSlotToBaseLod(StaticMesh);
#if WITH_EDITOR
		UStaticMesh::RemoveUnusedMaterialSlots(StaticMesh);
#endif
	}

	ImportAssetResult.ImportedObject = StaticMesh;
	return ImportAssetResult;
}

void UInterchangeStaticMeshFactory::CommitMeshDescriptions(UStaticMesh& StaticMesh)
{
#if WITH_EDITOR
	if (FApp::IsGame())
	{
		return;
	}

	TArray<FMeshDescription> LodMeshDescriptions = MoveTemp(ImportAssetObjectData.LodMeshDescriptions);

	UStaticMesh::FCommitMeshDescriptionParams CommitMeshDescriptionParams;
	CommitMeshDescriptionParams.bMarkPackageDirty = false; // Marking packages dirty isn't thread-safe

	for (int32 LodIndex = 0; LodIndex < LodMeshDescriptions.Num(); ++LodIndex)
	{
		FMeshDescription* StaticMeshDescription = StaticMesh.CreateMeshDescription(LodIndex);
		check(StaticMeshDescription);
		*StaticMeshDescription = MoveTemp(LodMeshDescriptions[LodIndex]);

		StaticMesh.CommitMeshDescription(LodIndex, CommitMeshDescriptionParams);
	}
#endif
}

void UInterchangeStaticMeshFactory::BuildFromMeshDescriptions(UStaticMesh& StaticMesh)
{
	if (!FApp::IsGame())
	{
		return;
	}

	TArray<FMeshDescription> LodMeshDescriptions = MoveTemp(ImportAssetObjectData.LodMeshDescriptions);
	TArray<const FMeshDescription*> MeshDescriptionPointers;
	MeshDescriptionPointers.Reserve(LodMeshDescriptions.Num());

	for (const FMeshDescription& MeshDescription : LodMeshDescriptions)
	{
		MeshDescriptionPointers.Add(&MeshDescription);
	}

	UStaticMesh::FBuildMeshDescriptionsParams BuildMeshDescriptionsParams;
	BuildMeshDescriptionsParams.bUseHashAsGuid = true;
	BuildMeshDescriptionsParams.bMarkPackageDirty = false;
	BuildMeshDescriptionsParams.bBuildSimpleCollision = false;
	// Do not commit since we only need the render data and commit is slow
	BuildMeshDescriptionsParams.bCommitMeshDescription = false;
	BuildMeshDescriptionsParams.bFastBuild = true;
	// For the time being at runtime collision is set to complex one
	// TODO: Revisit pipeline options for collision. bImportCollision is not enough.
	BuildMeshDescriptionsParams.bAllowCpuAccess = ImportAssetObjectData.bImportCollision;
	StaticMesh.bAllowCPUAccess = BuildMeshDescriptionsParams.bAllowCpuAccess;

	StaticMesh.BuildFromMeshDescriptions(MeshDescriptionPointers, BuildMeshDescriptionsParams);
	
	// TODO: Expand support for different collision types
	if (ensure(StaticMesh.GetRenderData()))
	{
		if (ImportAssetObjectData.bImportCollision && !ImportAssetObjectData.bImportedCustomCollision)
		{
			if (StaticMesh.GetBodySetup() == nullptr)
			{
				StaticMesh.CreateBodySetup();
			}

			StaticMesh.GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		}
	}	
}

#if WITH_EDITORONLY_DATA
void UInterchangeStaticMeshFactory::SetupSourceModelsSettings(UStaticMesh& StaticMesh, const TArray<FMeshDescription>& LodMeshDescriptions, int32 PreviousLodCount, int32 FinalLodCount, bool bIsAReimport)
{
	for (int32 LodIndex = 0; LodIndex < FinalLodCount; ++LodIndex)
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh.GetSourceModel(LodIndex);

		// Make sure that mesh descriptions for added LODs are kept as is when the mesh is built
		if (LodIndex >= PreviousLodCount)
		{
			SrcModel.ResetReductionSetting();
		}

		if (!bIsAReimport && LodMeshDescriptions.IsValidIndex(LodIndex))
		{
			FStaticMeshConstAttributes StaticMeshAttributes(LodMeshDescriptions[LodIndex]);
			const int32 NumUVChannels = StaticMeshAttributes.GetVertexInstanceUVs().IsValid() ? StaticMeshAttributes.GetVertexInstanceUVs().GetNumChannels() : 1;
			const int32 FirstOpenUVChannel = NumUVChannels >= MAX_MESH_TEXTURE_COORDS_MD ? 1 : NumUVChannels;

			SrcModel.BuildSettings.DstLightmapIndex = FirstOpenUVChannel;

			if (LodIndex == 0)
			{
				StaticMesh.SetLightMapCoordinateIndex(FirstOpenUVChannel);
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA

/* This function is call in the completion task on the main thread, use it to call main thread post creation step for your assets */
void UInterchangeStaticMeshFactory::SetupObject_GameThread(const FSetupObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeStaticMeshFactory::SetupObject_GameThread);

	check(IsInGameThread());
	Super::SetupObject_GameThread(Arguments);

	// TODO: make sure this works at runtime
#if WITH_EDITORONLY_DATA
	if (ensure(Arguments.ImportedObject && Arguments.SourceData))
	{
		// We must call the Update of the asset source file in the main thread because UAssetImportData::Update execute some delegate we do not control
		UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Arguments.ImportedObject);

		UAssetImportData* ImportDataPtr = StaticMesh->GetAssetImportData();
		UE::Interchange::FFactoryCommon::FUpdateImportAssetDataParameters UpdateImportAssetDataParameters(StaticMesh, ImportDataPtr, Arguments.SourceData, Arguments.NodeUniqueID, Arguments.NodeContainer, Arguments.OriginalPipelines, Arguments.Translator);
		ImportDataPtr = UE::Interchange::FFactoryCommon::UpdateImportAssetData(UpdateImportAssetDataParameters);
		StaticMesh->SetAssetImportData(ImportDataPtr);
	}
#endif
}


TArray<UInterchangeStaticMeshFactory::FMeshPayload> UInterchangeStaticMeshFactory::GetMeshPayloads(const FImportAssetObjectParams& Arguments, const TArray<FString>& MeshUids) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeStaticMeshFactory::GetMeshPayloads);

	TArray<FMeshPayload> Payloads;
	Payloads.Reserve(MeshUids.Num());

	const IInterchangeMeshPayloadInterface* MeshTranslatorPayloadInterface = Cast<IInterchangeMeshPayloadInterface>(Arguments.Translator);
	if (!MeshTranslatorPayloadInterface)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import static mesh. The translator does not implement IInterchangeMeshPayloadInterface."));
		return Payloads;
	}

	FTransform GlobalOffsetTransform = FTransform::Identity;
	bool bBakeMeshes = false;
	if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(Arguments.NodeContainer))
	{
		CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
		CommonPipelineDataFactoryNode->GetBakeMeshes(bBakeMeshes);
	}

	for (const FString& MeshUid : MeshUids)
	{
		FMeshPayload Payload;

		const UInterchangeBaseNode* Node = Arguments.NodeContainer->GetNode(MeshUid);
		const UInterchangeMeshNode* MeshNode = Cast<const UInterchangeMeshNode>(Node);
		if (MeshNode == nullptr)
		{
			// MeshUid must refer to a scene node
			const UInterchangeSceneNode* SceneNode = Cast<const UInterchangeSceneNode>(Node);
			if (!ensure(SceneNode))
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD mesh reference when importing StaticMesh asset %s."), *Arguments.AssetName);
				continue;
			}

			if (bBakeMeshes)
			{
				// Get the transform from the scene node
				FTransform SceneNodeGlobalTransform;
				if (SceneNode->GetCustomGlobalTransform(Arguments.NodeContainer, GlobalOffsetTransform, SceneNodeGlobalTransform))
				{
					Payload.Transform = SceneNodeGlobalTransform;
				}
			}

			// And get the mesh node which it references
			FString MeshDependencyUid;
			SceneNode->GetCustomAssetInstanceUid(MeshDependencyUid);
			MeshNode = Cast<UInterchangeMeshNode>(Arguments.NodeContainer->GetNode(MeshDependencyUid));
		}
		else
		{
			if (bBakeMeshes)
			{
				//If we have a mesh that is not reference by a scene node, we must apply the global offset.
				Payload.Transform = GlobalOffsetTransform;
			}
		}

		if (!ensure(MeshNode))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD mesh reference when importing StaticMesh asset %s."), *Arguments.AssetName);
			continue;
		}

		TOptional<FInterchangeMeshPayLoadKey> OptionalPayLoadKey = MeshNode->GetPayLoadKey();
		if (!ensure(OptionalPayLoadKey.IsSet()))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Empty LOD mesh reference payload when importing StaticMesh asset %s."), *Arguments.AssetName);
			continue;
		}

		FInterchangeMeshPayLoadKey& PayLoadKey = OptionalPayLoadKey.GetValue();

		Payload.MeshName = PayLoadKey.UniqueId;
		Payload.PayloadData = MeshTranslatorPayloadInterface->GetMeshPayloadData(PayLoadKey, Payload.Transform);

		Payloads.Emplace(MoveTemp(Payload));
	}

	return Payloads;
}


bool UInterchangeStaticMeshFactory::AddConvexGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom)
{
	FStaticMeshConstAttributes Attributes(MeshDescription);
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();
	
	if (VertexPositions.GetNumElements() == 0)
	{
		return false;
	}

	FKConvexElem& ConvexElem = AggGeom.ConvexElems.Emplace_GetRef();
	ConvexElem.VertexData.AddUninitialized(VertexPositions.GetNumElements());

	for (int32 Index = 0; Index < VertexPositions.GetNumElements(); Index++)
	{
		ConvexElem.VertexData[Index] = Transform.TransformPosition(FVector(VertexPositions[Index]));
	}

	ConvexElem.UpdateElemBox();

	return true;
}


bool UInterchangeStaticMeshFactory::DecomposeConvexMesh(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, UBodySetup* BodySetup)
{
#if WITH_EDITOR

	// Construct a bit array containing a bit for each triangle ID in the mesh description.
	// We are assuming the mesh description is compact, i.e. it has no holes, and so the number of triangles is equal to the array size.
	// The aim is to identify 'islands' of adjacent triangles which will form separate convex hulls

	check(MeshDescription.Triangles().Num() == MeshDescription.Triangles().GetArraySize());
	TBitArray<> BitArray(0, MeshDescription.Triangles().Num());

	// Here we build the groups of triangle IDs

	TArray<TArray<FTriangleID>> TriangleGroups;

	int32 FirstIndex = BitArray.FindAndSetFirstZeroBit();
	while (FirstIndex != INDEX_NONE)
	{
		// Find the first index we haven't used yet, and use it as the beginning of a new triangle group

		TArray<FTriangleID>& TriangleGroup = TriangleGroups.Emplace_GetRef();
		TriangleGroup.Emplace(FirstIndex);

		// Now iterate through the TriangleGroup array, finding unused adjacent triangles to each index, and appending them
		// to the end of the array.  Note we deliberately check the array size each time round the loop, as each iteration
		// can cause it to grow.

		for (int32 CheckIndex = 0; CheckIndex < TriangleGroup.Num(); ++CheckIndex)
		{
			for (FTriangleID AdjacentTriangleID : MeshDescription.GetTriangleAdjacentTriangles(TriangleGroup[CheckIndex]))
			{
				if (BitArray[AdjacentTriangleID] == 0)
				{
					// Append unused adjacent triangles to the TriangleGroup, to be considered for adjacency later
					TriangleGroup.Emplace(AdjacentTriangleID);
					BitArray[AdjacentTriangleID] = 1;
				}
			}
		}

		// When we exhaust the triangle group array, there are no more triangles in this island.
		// Now find the start of the next group.

		FirstIndex = BitArray.FindAndSetFirstZeroBit();
	}

	// Now iterate through the triangle groups, adding each as a convex hull to the AggGeom

	UModel* TempModel = NewObject<UModel>();
	TempModel->RootOutside = true;
	TempModel->EmptyModel(true, true);
	TempModel->Polys->ClearFlags(RF_Transactional);

	FStaticMeshConstAttributes Attributes(MeshDescription);
	TTriangleAttributesRef<TArrayView<const FVertexID>> TriangleVertices = Attributes.GetTriangleVertexIndices();
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();

	bool bSuccess = true;

	for (const TArray<FTriangleID>& TriangleGroup : TriangleGroups)
	{
		// Initialize a new brush
		TempModel->Polys->Element.Empty();

		// Add each triangle to the brush
		int32 Index = 0;
		for (FTriangleID TriangleID : TriangleGroup)
		{
			FPoly& Poly = TempModel->Polys->Element.Emplace_GetRef();
			Poly.Init();
			Poly.iLink = Index++;

			// For reasons lost in time, BSP poly vertices have the opposite winding order to regular mesh vertices.
			// So add them backwards (sigh)
			Poly.Vertices.Emplace(Transform.TransformPosition(FVector(VertexPositions[TriangleVertices[TriangleID][2]])));
			Poly.Vertices.Emplace(Transform.TransformPosition(FVector(VertexPositions[TriangleVertices[TriangleID][1]])));
			Poly.Vertices.Emplace(Transform.TransformPosition(FVector(VertexPositions[TriangleVertices[TriangleID][0]])));

			Poly.CalcNormal(true);
		}

		// Build bounding box
		TempModel->BuildBound();

		// Build BSP for the brush
		FBSPOps::bspBuild(TempModel, FBSPOps::BSP_Good, 15, 70, 1, 0);
		FBSPOps::bspRefresh(TempModel, true);
		FBSPOps::bspBuildBounds(TempModel);

		bSuccess &= BodySetup->CreateFromModel(TempModel, false);
	}

	TempModel->ClearInternalFlags(EInternalObjectFlags::Async);
	TempModel->Polys->ClearInternalFlags(EInternalObjectFlags::Async);

	return bSuccess;

#else // #if WITH_EDITOR

	return false;

#endif
}


static bool AreEqual(float A, float B)
{
	constexpr float MeshToPrimTolerance = 0.001f;
	return FMath::Abs(A - B) < MeshToPrimTolerance;
}


static bool AreParallel(const FVector3f& A, const FVector3f& B)
{
	float Dot = FVector3f::DotProduct(A, B);

	return (AreEqual(FMath::Abs(Dot), 1.0f));
}


static FVector3f GetTriangleNormal(const FTransform& Transform, TVertexAttributesRef<const FVector3f> VertexPositions, TArrayView<const FVertexID> VertexIndices)
{
	const FVector3f& V0 = VertexPositions[VertexIndices[0]];
	const FVector3f& V1 = VertexPositions[VertexIndices[1]];
	const FVector3f& V2 = VertexPositions[VertexIndices[2]];
	// @todo: LWC conversions everywhere here; surely this can be more elegant?
	return FVector3f(Transform.TransformVector(FVector(FVector3f::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal())));
}


bool UInterchangeStaticMeshFactory::AddBoxGeomFromTris(const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom)
{
	// Maintain an array of the planes we have encountered so far.
	// We are expecting two instances of three unique plane orientations, one for each side of the box.

	struct FPlaneInfo
	{
		FPlaneInfo(const FVector3f InNormal, float InFirstDistance)
			: Normal(InNormal),
			  DistCount(1),
			  PlaneDist{InFirstDistance, 0.0f}
		{}

		FVector3f Normal;
		int32 DistCount;
		float PlaneDist[2];
	};

	TArray<FPlaneInfo> Planes;
	FBox Box(ForceInit);

	FStaticMeshConstAttributes Attributes(MeshDescription);
	TTriangleAttributesRef<TArrayView<const FVertexID>> TriangleVertices = Attributes.GetTriangleVertexIndices();
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();

	for (FTriangleID TriangleID : MeshDescription.Triangles().GetElementIDs())
	{
		TArrayView<const FVertexID> VertexIndices = TriangleVertices[TriangleID];

		FVector3f TriangleNormal = GetTriangleNormal(Transform, VertexPositions, VertexIndices);
		if (TriangleNormal.IsNearlyZero())
		{
			continue;
		}

		bool bFoundPlane = false;
		for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num() && !bFoundPlane; PlaneIndex++)
		{
			// if this triangle plane is already known...
			if (AreParallel(TriangleNormal, Planes[PlaneIndex].Normal))
			{
				// Always use the same normal when comparing distances, to ensure consistent sign.
				float Dist = FVector3f::DotProduct(VertexPositions[VertexIndices[0]], Planes[PlaneIndex].Normal);

				// we only have one distance, and its not that one, add it.
				if (Planes[PlaneIndex].DistCount == 1 && !AreEqual(Dist, Planes[PlaneIndex].PlaneDist[0]))
				{
					Planes[PlaneIndex].PlaneDist[1] = Dist;
					Planes[PlaneIndex].DistCount = 2;
				}
				// if we have a second distance, and its not that either, something is wrong.
				else if (Planes[PlaneIndex].DistCount == 2 && !AreEqual(Dist, Planes[PlaneIndex].PlaneDist[0]) && !AreEqual(Dist, Planes[PlaneIndex].PlaneDist[1]))
				{
					// Error
//					UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddBoxGeomFromTris (%s): Found more than 2 planes with different distances."), ObjName);
					return false;
				}

				bFoundPlane = true;
			}
		}

		// If this triangle does not match an existing plane, add to list.
		if (!bFoundPlane)
		{
			Planes.Emplace(TriangleNormal, FVector3f::DotProduct(VertexPositions[VertexIndices[0]], TriangleNormal));
		}

		// Maintain an AABB, adding points from each triangle.
		// We will use this to determine the origin of the box transform.

		Box += Transform.TransformPosition(FVector(VertexPositions[VertexIndices[0]]));
		Box += Transform.TransformPosition(FVector(VertexPositions[VertexIndices[1]]));
		Box += Transform.TransformPosition(FVector(VertexPositions[VertexIndices[2]]));
	}

	// Now we have our candidate planes, see if there are any problems

	// Wrong number of planes.
	if (Planes.Num() != 3)
	{
		// Error
//		UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddBoxGeomFromTris (%s): Not very box-like (need 3 sets of planes)."), ObjName);
		return false;
	}

	// If we don't have 3 pairs, we can't carry on.
	if ((Planes[0].DistCount != 2) || (Planes[1].DistCount != 2) || (Planes[2].DistCount != 2))
	{
		// Error
//		UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddBoxGeomFromTris (%s): Incomplete set of planes (need 2 per axis)."), ObjName);
		return false;
	}

	// ensure valid TM by cross-product
	if (!AreParallel(FVector3f::CrossProduct(Planes[0].Normal, Planes[1].Normal), Planes[2].Normal))
	{
		// Error
//		UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddBoxGeomFromTris (%s): Box axes are not perpendicular."), ObjName);
		return false;
	}

	// Allocate box in array
	FKBoxElem BoxElem;

	//In case we have a box oriented with the world axis system we want to reorder the plane to not introduce axis swap.
	//If the box was turned, the order of the planes will be arbitrary and the box rotation will make the collision
	//not playing well if the asset is built or place in a level with a non uniform scale.
	FVector3f Axis[3] = { FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f::ZAxisVector };
	int32 Reorder[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
	{
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			if (AreParallel(Planes[PlaneIndex].Normal, Axis[AxisIndex]))
			{
				Reorder[PlaneIndex] = AxisIndex;
				break;
			}
		}
	}

	if (Reorder[0] == INDEX_NONE || Reorder[1] == INDEX_NONE || Reorder[2] == INDEX_NONE)
	{
		Reorder[0] = 0;
		Reorder[1] = 1;
		Reorder[2] = 2;
	}

	BoxElem.SetTransform(FTransform(FVector(Planes[Reorder[0]].Normal), FVector(Planes[Reorder[1]].Normal), FVector(Planes[Reorder[2]].Normal), Box.GetCenter()));

	// distance between parallel planes is box edge lengths.
	BoxElem.X = FMath::Abs(Planes[Reorder[0]].PlaneDist[0] - Planes[Reorder[0]].PlaneDist[1]);
	BoxElem.Y = FMath::Abs(Planes[Reorder[1]].PlaneDist[0] - Planes[Reorder[1]].PlaneDist[1]);
	BoxElem.Z = FMath::Abs(Planes[Reorder[2]].PlaneDist[0] - Planes[Reorder[2]].PlaneDist[1]);

	AggGeom.BoxElems.Add(BoxElem);

	return true;
}


bool UInterchangeStaticMeshFactory::AddSphereGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom)
{
	FStaticMeshConstAttributes Attributes(MeshDescription);
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();

	if (VertexPositions.GetNumElements() == 0)
	{
		return false;
	}

	FBox Box(ForceInit);

	for (const FVector3f& VertexPosition : VertexPositions.GetRawArray())
	{
		Box += Transform.TransformPosition(FVector(VertexPosition));
	}

	FVector Center;
	FVector Extents;
	Box.GetCenterAndExtents(Center, Extents);
	float Longest = 2.0f * Extents.GetMax();
	float Shortest = 2.0f * Extents.GetMin();

	// check that the AABB is roughly a square (5% tolerance)
	if ((Longest - Shortest) / Longest > 0.05f)
	{
		// Error
		//UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddSphereGeomFromVerts (%s): Sphere bounding box not square."), ObjName);
		return false;
	}

	float Radius = 0.5f * Longest;

	// Test that all vertices are a similar radius (5%) from the sphere centre.
	float MaxR = 0;
	float MinR = BIG_NUMBER;


	for (const FVector3f& VertexPosition : VertexPositions.GetRawArray())
	{
		FVector CToV = Transform.TransformPosition(FVector(VertexPosition)) - Center;
		float RSqr = CToV.SizeSquared();

		MaxR = FMath::Max(RSqr, MaxR);

		// Sometimes vertex at centre, so reject it.
		if (RSqr > KINDA_SMALL_NUMBER)
		{
			MinR = FMath::Min(RSqr, MinR);
		}
	}

	MaxR = FMath::Sqrt(MaxR);
	MinR = FMath::Sqrt(MinR);

	if ((MaxR - MinR) / Radius > 0.05f)
	{
		// Error
		//UE_LOG(LogStaticMeshImportUtils, Log, TEXT("AddSphereGeomFromVerts (%s): Vertices not at constant radius."), ObjName);
		return false;
	}

	// Allocate sphere in array
	FKSphereElem SphereElem;
	SphereElem.Center = Center;
	SphereElem.Radius = Radius;
	AggGeom.SphereElems.Add(SphereElem);

	return true;
}


bool UInterchangeStaticMeshFactory::AddCapsuleGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom)
{
	FStaticMeshConstAttributes Attributes(MeshDescription);
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();

	if (VertexPositions.GetNumElements() == 0)
	{
		return false;
	}

	FVector AxisStart;
	FVector AxisEnd;
	float MaxDistSqr = 0.f;

	for (int32 IndexA = 0; IndexA < VertexPositions.GetNumElements() - 1; IndexA++)
	{
		for (int32 IndexB = IndexA + 1; IndexB < VertexPositions.GetNumElements(); IndexB++)
		{
			FVector TransformedA = Transform.TransformPosition(FVector(VertexPositions[IndexA]));
			FVector TransformedB = Transform.TransformPosition(FVector(VertexPositions[IndexB]));

			float DistSqr = (TransformedA - TransformedB).SizeSquared();
			if (DistSqr > MaxDistSqr)
			{
				AxisStart = TransformedA;
				AxisEnd = TransformedB;
				MaxDistSqr = DistSqr;
			}
		}
	}

	// If we got a valid axis, find vertex furthest from it
	if (MaxDistSqr > SMALL_NUMBER)
	{
		float MaxRadius = 0.0f;

		const FVector LineOrigin = AxisStart;
		const FVector LineDir = (AxisEnd - AxisStart).GetSafeNormal();

		for (int32 IndexA = 0; IndexA < VertexPositions.GetNumElements(); IndexA++)
		{
			FVector TransformedA = Transform.TransformPosition(FVector(VertexPositions[IndexA]));

			float DistToAxis = FMath::PointDistToLine(TransformedA, LineDir, LineOrigin);
			if (DistToAxis > MaxRadius)
			{
				MaxRadius = DistToAxis;
			}
		}

		if (MaxRadius > SMALL_NUMBER)
		{
			// Allocate capsule in array
			FKSphylElem SphylElem;
			SphylElem.Center = 0.5f * (AxisStart + AxisEnd);
			SphylElem.Rotation = FQuat::FindBetweenVectors(FVector::ZAxisVector, LineDir).Rotator(); // Get quat that takes you from z axis to desired axis
			SphylElem.Radius = MaxRadius;
			SphylElem.Length = FMath::Max(FMath::Sqrt(MaxDistSqr) - (2.0f * MaxRadius), 0.0f); // subtract two radii from total length to get segment length (ensure > 0)
			AggGeom.SphylElems.Add(SphylElem);
			return true;
		}
	}

	return false;

}


bool UInterchangeStaticMeshFactory::ImportBoxCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode)
{
	using namespace UE::Interchange;

	bool bResult = false;

	TArray<FString> BoxCollisionMeshUids;
	LodDataNode->GetBoxCollisionMeshUids(BoxCollisionMeshUids);

	FKAggregateGeom& AggGeo = StaticMesh->GetBodySetup()->AggGeom;

	TArray<FMeshPayload> MeshPayloads = GetMeshPayloads(Arguments, BoxCollisionMeshUids);
	for (const FMeshPayload& MeshPayload : MeshPayloads)
	{
		const FTransform Transform = FTransform::Identity;
		const TOptional<FMeshPayloadData>& PayloadData = MeshPayload.PayloadData.Get();

		if (!PayloadData.IsSet())
		{
			// warning here
			continue;
		}

		if (AddBoxGeomFromTris(PayloadData->MeshDescription, Transform, AggGeo))
		{
			bResult = true;
			FKBoxElem& NewElem = AggGeo.BoxElems.Last();

			// Now test the last element in the AggGeo list and remove it if its a duplicate
			// @TODO: determine why we have to do this. Was it to prevent duplicate boxes accumulating when reimporting?
			for (int32 ElementIndex = 0; ElementIndex < AggGeo.BoxElems.Num() - 1; ++ElementIndex)
			{
				FKBoxElem& CurrentElem = AggGeo.BoxElems[ElementIndex];

				if (CurrentElem == NewElem)
				{
					// The new element is a duplicate, remove it
					AggGeo.BoxElems.RemoveAt(AggGeo.BoxElems.Num() - 1);
					break;
				}
			}
		}
	}

	return bResult;
}


bool UInterchangeStaticMeshFactory::ImportCapsuleCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode)
{
	using namespace UE::Interchange;

	bool bResult = false;

	TArray<FString> CapsuleCollisionMeshUids;
	LodDataNode->GetCapsuleCollisionMeshUids(CapsuleCollisionMeshUids);

	FKAggregateGeom& AggGeo = StaticMesh->GetBodySetup()->AggGeom;

	TArray<FMeshPayload> MeshPayloads = GetMeshPayloads(Arguments, CapsuleCollisionMeshUids);
	for (const FMeshPayload& MeshPayload : MeshPayloads)
	{
		const FTransform& Transform = FTransform::Identity;
		const TOptional<FMeshPayloadData>& PayloadData = MeshPayload.PayloadData.Get();

		if (!PayloadData.IsSet())
		{
			// warning here
			continue;
		}

		if (AddCapsuleGeomFromVertices(Arguments, PayloadData->MeshDescription, Transform, AggGeo))
		{
			bResult = true;

			FKSphylElem& NewElem = AggGeo.SphylElems.Last();

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			// @TODO: determine why we have to do this. Was it to prevent duplicate boxes accumulating when reimporting?
			for (int32 ElementIndex = 0; ElementIndex < AggGeo.SphylElems.Num() - 1; ++ElementIndex)
			{
				FKSphylElem& CurrentElem = AggGeo.SphylElems[ElementIndex];
				if (CurrentElem == NewElem)
				{
					// The new element is a duplicate, remove it
					AggGeo.SphylElems.RemoveAt(AggGeo.SphylElems.Num() - 1);
					break;
				}
			}
		}
	}

	return bResult;
}


bool UInterchangeStaticMeshFactory::ImportSphereCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode)
{
	using namespace UE::Interchange;

	bool bResult = false;

	TArray<FString> SphereCollisionMeshUids;
	LodDataNode->GetSphereCollisionMeshUids(SphereCollisionMeshUids);

	FKAggregateGeom& AggGeo = StaticMesh->GetBodySetup()->AggGeom;

	TArray<FMeshPayload> MeshPayloads = GetMeshPayloads(Arguments, SphereCollisionMeshUids);
	for (const FMeshPayload& MeshPayload : MeshPayloads)
	{
		const FTransform& Transform = FTransform::Identity;
		const TOptional<FMeshPayloadData>& PayloadData = MeshPayload.PayloadData.Get();

		if (!PayloadData.IsSet())
		{
			// warning here
			continue;
		}

		if (AddSphereGeomFromVertices(Arguments, PayloadData->MeshDescription, Transform, AggGeo))
		{
			bResult = true;

			FKSphereElem& NewElem = AggGeo.SphereElems.Last();

			// Now test the last element in the AggGeo list and remove it if its a duplicate
			// @TODO: determine why we have to do this. Was it to prevent duplicate boxes accumulating when reimporting?
			for (int32 ElementIndex = 0; ElementIndex < AggGeo.SphereElems.Num() - 1; ++ElementIndex)
			{
				FKSphereElem& CurrentElem = AggGeo.SphereElems[ElementIndex];

				if (CurrentElem == NewElem)
				{
					// The new element is a duplicate, remove it
					AggGeo.SphereElems.RemoveAt(AggGeo.SphereElems.Num() - 1);
					break;
				}
			}
		}
	}

	return bResult;
}


bool UInterchangeStaticMeshFactory::ImportConvexCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode)
{
	using namespace UE::Interchange;

	bool bResult = false;

	TArray<FString> ConvexCollisionMeshUids;
	LodDataNode->GetConvexCollisionMeshUids(ConvexCollisionMeshUids);

	TArray<FMeshPayload> MeshPayloads = GetMeshPayloads(Arguments, ConvexCollisionMeshUids);

	bool bOneConvexHullPerUCX;
	if (!LodDataNode->GetOneConvexHullPerUCX(bOneConvexHullPerUCX) || !bOneConvexHullPerUCX)
	{
		for (const FMeshPayload& MeshPayload : MeshPayloads)
		{
			const FTransform& Transform = FTransform::Identity;
			const TOptional<FMeshPayloadData>& PayloadData = MeshPayload.PayloadData.Get();

			if (!PayloadData.IsSet())
			{
				// warning here
				continue;
			}

			if (!DecomposeConvexMesh(Arguments, PayloadData->MeshDescription, Transform, StaticMesh->GetBodySetup()))
			{
				// error: could not decompose mesh
			}
			else
			{
				bResult = true;
			}
		}
	}
	else
	{
		FKAggregateGeom& AggGeo = StaticMesh->GetBodySetup()->AggGeom;

		for (const FMeshPayload& MeshPayload : MeshPayloads)
		{
			const FTransform& Transform = FTransform::Identity;
			TOptional<FMeshPayloadData> PayloadData = MeshPayload.PayloadData.Get();

			if (!PayloadData.IsSet())
			{
				// warning here
				continue;
			}

			if (AddConvexGeomFromVertices(Arguments, PayloadData->MeshDescription, Transform, AggGeo))
			{
				bResult = true;

				FKConvexElem& NewElem = AggGeo.ConvexElems.Last();

				// Now test the late element in the AggGeo list and remove it if its a duplicate
				// @TODO: determine why the importer used to do this. Was it something to do with reimport not adding extra collision or something?
				for (int32 ElementIndex = 0; ElementIndex < AggGeo.ConvexElems.Num() - 1; ++ElementIndex)
				{
					FKConvexElem& CurrentElem = AggGeo.ConvexElems[ElementIndex];

					if (CurrentElem.VertexData.Num() == NewElem.VertexData.Num())
					{
						bool bFoundDifference = false;
						for (int32 VertexIndex = 0; VertexIndex < NewElem.VertexData.Num(); ++VertexIndex)
						{
							if (CurrentElem.VertexData[VertexIndex] != NewElem.VertexData[VertexIndex])
							{
								bFoundDifference = true;
								break;
							}
						}

						if (!bFoundDifference)
						{
							// The new collision geo is a duplicate, delete it
							AggGeo.ConvexElems.RemoveAt(AggGeo.ConvexElems.Num() - 1);
							break;
						}
					}
				}
			}
		}
	}

	return bResult;
}


bool UInterchangeStaticMeshFactory::GenerateKDopCollision(UStaticMesh* StaticMesh)
{
#if WITH_EDITOR

	constexpr float RecipSqrt2 = UE_INV_SQRT_2;
	constexpr int32 KCount = 18;
	static const FVector3f KDopDir18[KCount] =
	{
		FVector3f( 1.0f,  0.0f,  0.0f),
		FVector3f(-1.0f,  0.0f,  0.0f),
		FVector3f( 0.0f,  1.0f,  0.0f),
		FVector3f( 0.0f, -1.0f,  0.0f),
		FVector3f( 0.0f,  0.0f,  1.0f),
		FVector3f( 0.0f,  0.0f, -1.0f),
		FVector3f( 0.0f,  RecipSqrt2,  RecipSqrt2),
		FVector3f( 0.0f, -RecipSqrt2, -RecipSqrt2),
		FVector3f( 0.0f,  RecipSqrt2, -RecipSqrt2),
		FVector3f( 0.0f, -RecipSqrt2,  RecipSqrt2),
		FVector3f( RecipSqrt2,  0.0f,  RecipSqrt2),
		FVector3f(-RecipSqrt2,  0.0f, -RecipSqrt2),
		FVector3f( RecipSqrt2,  0.0f, -RecipSqrt2),
		FVector3f(-RecipSqrt2,  0.0f,  RecipSqrt2),
		FVector3f( RecipSqrt2,  RecipSqrt2,  0.0f),
		FVector3f(-RecipSqrt2, -RecipSqrt2,  0.0f),
		FVector3f( RecipSqrt2, -RecipSqrt2,  0.0f),
		FVector3f(-RecipSqrt2,  RecipSqrt2,  0.0f)
	};

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();

	// Initialize maximum distances from each kdop plane to the minimum value possible
	TArray<float> MaxDist;
	MaxDist.Reserve(KCount);
	for (int32 Count = 0; Count < KCount; Count++)
	{
		MaxDist.Add(-MAX_FLT);
	}

	// Construct temporary UModel for kdop creation.
	UModel* TempModel = nullptr;
	{
		FGCScopeGuard GCScopeGuard;
		TempModel = NewObject<UModel>();
	}
	TempModel->RootOutside = true;
	TempModel->EmptyModel(true, true);
	TempModel->Polys->ClearFlags(RF_Transactional);

	// Get the vertex positions for the final LOD0 mesh
	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(0);
	check(MeshDescription);

	FStaticMeshConstAttributes Attributes(*MeshDescription);
	TVertexAttributesRef<const FVector3f> VertexPositions = Attributes.GetVertexPositions();

	// For each vertex, project along each kdop direction, to find the max in that direction.
	for (FVector3f VertexPosition : VertexPositions.GetRawArray())
	{
		for (int32 Index = 0; Index < KCount; Index++)
		{
			float Dist = FVector3f::DotProduct(VertexPosition, KDopDir18[Index]);
			MaxDist[Index] = FMath::Max(Dist, MaxDist[Index]);
		}
	}

	// Inflate kdop to ensure it is not degenerate
	constexpr float MinSize = 0.1f;
	for (int32 Index = 0; Index < KCount; Index++)
	{
		MaxDist[Index] += MinSize;
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane4f> Planes;
	Planes.Reserve(KCount);
	for (int32 Index = 0; Index < KCount; Index++)
	{
		Planes.Add(FPlane4f(KDopDir18[Index], MaxDist[Index]));
	}

	for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num(); PlaneIndex++)
	{
		FPoly& Polygon = TempModel->Polys->Element.Emplace_GetRef();

		Polygon.Init();
		Polygon.Normal = Planes[PlaneIndex];

		FVector3f AxisX, AxisY;
		Polygon.Normal.FindBestAxisVectors(AxisX, AxisY);

		FVector3f Base = Planes[PlaneIndex] * Planes[PlaneIndex].W;

		Polygon.Vertices.Emplace(Base + AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);
		Polygon.Vertices.Emplace(Base + AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		Polygon.Vertices.Emplace(Base - AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		Polygon.Vertices.Emplace(Base - AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);

		for (int32 OtherPlaneIndex = 0; OtherPlaneIndex < Planes.Num(); OtherPlaneIndex++)
		{
			if (PlaneIndex != OtherPlaneIndex)
			{
				if (!Polygon.Split(-FVector3f(Planes[OtherPlaneIndex]), Planes[OtherPlaneIndex] * Planes[OtherPlaneIndex].W))
				{
					Polygon.Vertices.Empty();
					break;
				}
			}
		}

		if (Polygon.Vertices.Num() < 3)
		{
			// If poly resulted in no verts, remove from array
			TempModel->Polys->Element.RemoveAt(TempModel->Polys->Element.Num() - 1);
		}
		else
		{
			Polygon.iLink = PlaneIndex;

			constexpr bool bSilent = true;
			Polygon.CalcNormal(bSilent);
		}
	}

	if (TempModel->Polys->Element.Num() < 4)
	{
		return false;
	}

	// Build bounding box.
	TempModel->BuildBound();

	// Build BSP for the brush.
	FBSPOps::bspBuild(TempModel, FBSPOps::BSP_Good, 15, 70, 1, 0);

	constexpr bool bNoRemapSurfs = true;
	FBSPOps::bspRefresh(TempModel, bNoRemapSurfs);
	FBSPOps::bspBuildBounds(TempModel);

	bool bRemoveExisting = true;
	BodySetup->CreateFromModel(TempModel, bRemoveExisting);

	TempModel->ClearInternalFlags(EInternalObjectFlags::Async);
	TempModel->Polys->ClearInternalFlags(EInternalObjectFlags::Async);

	return true;

#else // #if WITH_EDITOR

	return false;

#endif
}


bool UInterchangeStaticMeshFactory::ImportSockets(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshFactoryNode* FactoryNode)
{
	TArray<FString> SocketUids;
	FactoryNode->GetSocketUids(SocketUids);

	TSet<FName> ImportedSocketNames;

	FTransform GlobalOffsetTransform = FTransform::Identity;
	bool bBakeMeshes = false;
	if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(Arguments.NodeContainer))
	{
		CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
		CommonPipelineDataFactoryNode->GetBakeMeshes(bBakeMeshes);
	}

	for (const FString& SocketUid : SocketUids)
	{
		if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Arguments.NodeContainer->GetNode(SocketUid)))
		{
			FString NodeDisplayName = SceneNode->GetDisplayLabel();
			if (NodeDisplayName.StartsWith(TEXT("SOCKET_")))
			{
				NodeDisplayName.RightChopInline(sizeof("SOCKET_") - 1, EAllowShrinking::No);
			}
			FName SocketName = FName(NodeDisplayName);
			ImportedSocketNames.Add(SocketName);

			FTransform Transform;
			if (bBakeMeshes)
			{
				SceneNode->GetCustomGlobalTransform(Arguments.NodeContainer, GlobalOffsetTransform, Transform);
			}

			UStaticMeshSocket* Socket = StaticMesh->FindSocket(SocketName);
			if (!Socket)
			{
				// If the socket didn't exist create a new one now
				Socket = NewObject<UStaticMeshSocket>(StaticMesh);
#if WITH_EDITORONLY_DATA
				Socket->bSocketCreatedAtImport = true;
#endif
				Socket->SocketName = SocketName;
				StaticMesh->AddSocket(Socket);
			}

			Socket->RelativeLocation = Transform.GetLocation();
			Socket->RelativeRotation = Transform.GetRotation().Rotator();
			Socket->RelativeScale = Transform.GetScale3D();
		}
	}

	// Delete any sockets which were previously imported but which no longer exist in the imported scene
	for (TArray<TObjectPtr<UStaticMeshSocket>>::TIterator It = StaticMesh->Sockets.CreateIterator(); It; ++It)
	{
		UStaticMeshSocket* Socket = *It;
		if (
#if WITH_EDITORONLY_DATA
			Socket->bSocketCreatedAtImport &&
#endif
			!ImportedSocketNames.Contains(Socket->SocketName))
		{
			It.RemoveCurrent();
		}
	}

	return true;
}

bool UInterchangeStaticMeshFactory::GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const
{
#if WITH_EDITORONLY_DATA
	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
	{
		return UE::Interchange::FFactoryCommon::GetSourceFilenames(StaticMesh->GetAssetImportData(), OutSourceFilenames);
	}
#endif

	return false;
}

bool UInterchangeStaticMeshFactory::SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const
{
#if WITH_EDITORONLY_DATA
	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
	{
		return UE::Interchange::FFactoryCommon::SetSourceFilename(StaticMesh->GetAssetImportData(), SourceFilename, SourceIndex);
	}
#endif

	return false;
}

