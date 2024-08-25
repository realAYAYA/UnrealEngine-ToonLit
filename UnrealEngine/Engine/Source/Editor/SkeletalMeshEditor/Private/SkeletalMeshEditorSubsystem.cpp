// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshEditorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkinnedAssetCommon.h"
#include "FbxMeshUtils.h"
#include "LODUtilities.h"
#include "ObjectTools.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ScopedTransaction.h"
#include "EditorScriptingHelpers.h"
#include "SkeletalMeshTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/ImportSubsystem.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshEditorSubsystem"

DEFINE_LOG_CATEGORY(LogSkeletalMeshEditorSubsystem);

USkeletalMeshEditorSubsystem::USkeletalMeshEditorSubsystem()
	: UEditorSubsystem()
{

}

bool USkeletalMeshEditorSubsystem::RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount /*= 0*/, bool bRegenerateEvenIfImported /*= false*/, bool bGenerateBaseLOD /*= false*/)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RegenerateLOD: The SkeletalMesh is null."));
		return false;
	}

	return FLODUtilities::RegenerateLOD(SkeletalMesh, GetTargetPlatformManagerRef().GetRunningTargetPlatform(), NewLODCount, bRegenerateEvenIfImported, bGenerateBaseLOD);
}


int32 USkeletalMeshEditorSubsystem::GetNumVerts(USkeletalMesh* SkeletalMesh, int32 LODIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return 0;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetNumVerts: The SkeletalMesh is null."));
		return 0;
	}

	if (SkeletalMesh->GetResourceForRendering() && SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0)
	{
		TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
		if (LodRenderData.IsValidIndex(LODIndex))
		{
			return LodRenderData[LODIndex].GetNumVertices();
		}
	}

	return 0;
}

int32 USkeletalMeshEditorSubsystem::GetNumSections( USkeletalMesh* SkeletalMesh, int32 LODIndex )
{
	TGuardValue<bool> UnattendedScriptGuard( GIsRunningUnattendedScript, true );

	if ( !EditorScriptingHelpers::CheckIfInEditorAndPIE() )
	{
		return INDEX_NONE;
	}

	if ( SkeletalMesh == nullptr )
	{
		UE_LOG( LogSkeletalMeshEditorSubsystem, Error, TEXT( "GetNumSections: The SkeletalMesh is null." ) );
		return INDEX_NONE;
	}

	if ( SkeletalMesh->GetResourceForRendering() && SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0 )
	{
		TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
		if ( LodRenderData.IsValidIndex( LODIndex ) )
		{
			return LodRenderData[ LODIndex ].RenderSections.Num();
		}
	}

	return INDEX_NONE;
}

bool USkeletalMeshEditorSubsystem::GetSectionRecomputeTangent(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, bool& bOutRecomputeTangent)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangent: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetResourceForRendering() || !(SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangent: The render data is null."));
		return false;
	}

	TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
	if (!LodRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangent: The LOD index is invalid."));
		return false;
	}
	if (!LodRenderData[LODIndex].RenderSections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangent: The section index is invalid."));
		return false;
	}

	bOutRecomputeTangent = LodRenderData[LODIndex].RenderSections[SectionIndex].bRecomputeTangent;
	return true;
}

bool USkeletalMeshEditorSubsystem::SetSectionRecomputeTangent(USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const bool bRecomputeTangent)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangent: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangent: The imported model is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangent: The LOD index is invalid."));
		return false;
	}
	FSkeletalMeshLODModel& LODModel = SkeletalMesh->GetImportedModel()->LODModels[LODIndex];
	if (!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangent: The section index is invalid."));
		return false;
	}


	//Make the change as a transaction and make sure the render data is rebuilt properly
	{
		FScopedTransaction Transaction(LOCTEXT("SetSectionRecomputeTangentTransactionName", "Set section recompute tangent"));
		SkeletalMesh->Modify();
		const int32 OriginalSectionIndex = LODModel.Sections[SectionIndex].OriginalDataSectionIndex;
		FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(OriginalSectionIndex);
		SectionUserData.bRecomputeTangent = bRecomputeTangent;
	}
	//Rebuild the render data
	SkeletalMesh->PostEditChange();
	//This will make the sk build synchronous
	bool bNewValue = false;
	GetSectionRecomputeTangent(SkeletalMesh, LODIndex, SectionIndex, bNewValue);
	ensure(bNewValue == bRecomputeTangent);
	return true;
}

bool USkeletalMeshEditorSubsystem::GetSectionRecomputeTangentsVertexMaskChannel(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, uint8& OutRecomputeTangentsVertexMaskChannel)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangentsVertexMaskChannel: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetResourceForRendering() || !(SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangentsVertexMaskChannel: The render data is null."));
		return false;
	}

	TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
	if (!LodRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangentsVertexMaskChannel: The LOD index is invalid."));
		return false;
	}
	if (!LodRenderData[LODIndex].RenderSections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionRecomputeTangentsVertexMaskChannel: The section index is invalid."));
		return false;
	}

	OutRecomputeTangentsVertexMaskChannel = static_cast<uint8>(LodRenderData[LODIndex].RenderSections[SectionIndex].RecomputeTangentsVertexMaskChannel);
	return true;
}

bool USkeletalMeshEditorSubsystem::SetSectionRecomputeTangentsVertexMaskChannel(USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const uint8 RecomputeTangentsVertexMaskChannel)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangentsVertexMaskChannel: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangentsVertexMaskChannel: The imported model is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangentsVertexMaskChannel: The LOD index is invalid."));
		return false;
	}
	FSkeletalMeshLODModel& LODModel = SkeletalMesh->GetImportedModel()->LODModels[LODIndex];
	if (!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionRecomputeTangentsVertexMaskChannel: The section index is invalid."));
		return false;
	}

	//Make the change as a transaction and make sure the render data is rebuilt properly
	{
		FScopedTransaction Transaction(LOCTEXT("SetSectionRecomputeTangentsVertexMaskChannelTransactionName", "Set section recompute tangents vertex mask channel"));
		SkeletalMesh->Modify();
		const int32 OriginalSectionIndex = LODModel.Sections[SectionIndex].OriginalDataSectionIndex;
		FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(OriginalSectionIndex);
		SectionUserData.RecomputeTangentsVertexMaskChannel = static_cast<ESkinVertexColorChannel>(RecomputeTangentsVertexMaskChannel);
	}
	
	SkeletalMesh->PostEditChange();
	//This will make the sk build synchronous
	uint8 NewValue;
	GetSectionRecomputeTangentsVertexMaskChannel(SkeletalMesh, LODIndex, SectionIndex, NewValue);
	ensure(NewValue == RecomputeTangentsVertexMaskChannel);
	
	return true;
}

bool USkeletalMeshEditorSubsystem::GetSectionCastShadow(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, bool& bOutCastShadow)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionCastShadow: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetResourceForRendering() || !(SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionCastShadow: The render data is null."));
		return false;
	}

	TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
	if (!LodRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionCastShadow: The LOD index is invalid."));
		return false;
	}
	if (!LodRenderData[LODIndex].RenderSections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionCastShadow: The section index is invalid."));
		return false;
	}

	bOutCastShadow = LodRenderData[LODIndex].RenderSections[SectionIndex].bCastShadow;
	return true;
}

bool USkeletalMeshEditorSubsystem::SetSectionCastShadow(USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const bool bCastShadow)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionCastShadow: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionCastShadow: The imported model is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionCastShadow: The LOD index is invalid."));
		return false;
	}
	FSkeletalMeshLODModel& LODModel = SkeletalMesh->GetImportedModel()->LODModels[LODIndex];
	if (!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionCastShadow: The section index is invalid."));
		return false;
	}

	//Make the change as a transaction and make sure the render data is rebuilt properly
	{
		FScopedTransaction Transaction(LOCTEXT("SetSectionCastShadowTransactionName", "Set section cast shadow"));
		SkeletalMesh->Modify();
		const int32 OriginalSectionIndex = LODModel.Sections[SectionIndex].OriginalDataSectionIndex;
		FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(OriginalSectionIndex);
		SectionUserData.bCastShadow = bCastShadow;
	}
	//Rebuild the render data
	SkeletalMesh->PostEditChange();
	//This will make the sk build synchronous
	bool bNewValue = false;
	GetSectionCastShadow(SkeletalMesh, LODIndex, SectionIndex, bNewValue);
	ensure(bNewValue == bCastShadow);

	return true;
}

bool USkeletalMeshEditorSubsystem::GetSectionVisibleInRayTracing(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, bool& bOutVisibleInRayTracing)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionVisibleInRayTracing: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetResourceForRendering() || !(SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionVisibleInRayTracing: The render data is null."));
		return false;
	}

	TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
	if (!LodRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionVisibleInRayTracing: The LOD index is invalid."));
		return false;
	}
	if (!LodRenderData[LODIndex].RenderSections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetSectionVisibleInRayTracing: The section index is invalid."));
		return false;
	}

	bOutVisibleInRayTracing = LodRenderData[LODIndex].RenderSections[SectionIndex].bVisibleInRayTracing;
	return true;
}

bool USkeletalMeshEditorSubsystem::SetSectionVisibleInRayTracing(USkeletalMesh* SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const bool bVisibleInRayTracing)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionVisibleInRayTracing: The SkeletalMesh is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionVisibleInRayTracing: The imported model is null."));
		return false;
	}

	if (!SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionVisibleInRayTracing: The LOD index is invalid."));
		return false;
	}
	FSkeletalMeshLODModel& LODModel = SkeletalMesh->GetImportedModel()->LODModels[LODIndex];
	if (!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetSectionVisibleInRayTracing: The section index is invalid."));
		return false;
	}

	//Make the change as a transaction and make sure the render data is rebuilt properly
	{
		FScopedTransaction Transaction(LOCTEXT("SetSectionVisibleInRayTracingTransactionName", "Set section visible in ray tracing"));
		SkeletalMesh->Modify();
		const int32 OriginalSectionIndex = LODModel.Sections[SectionIndex].OriginalDataSectionIndex;
		FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(OriginalSectionIndex);
		SectionUserData.bVisibleInRayTracing = bVisibleInRayTracing;
	}

	SkeletalMesh->PostEditChange();
	//This will make the sk build synchronous
	bool bNewValue = false;
	GetSectionVisibleInRayTracing(SkeletalMesh, LODIndex, SectionIndex, bNewValue);
	ensure(bNewValue == bVisibleInRayTracing);

	return true;
}

int32 USkeletalMeshEditorSubsystem::GetLODMaterialSlot( USkeletalMesh* SkeletalMesh, int32 LODIndex, int32 SectionIndex )
{
	TGuardValue<bool> UnattendedScriptGuard( GIsRunningUnattendedScript, true );

	if ( !EditorScriptingHelpers::CheckIfInEditorAndPIE() )
	{
		return INDEX_NONE;
	}

	if ( SkeletalMesh == nullptr )
	{
		UE_LOG( LogSkeletalMeshEditorSubsystem, Error, TEXT( "GetLODMaterialSlot: The SkeletalMesh is null." ) );
		return INDEX_NONE;
	}

	if ( SkeletalMesh->GetResourceForRendering() && SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0 )
	{
		TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;
		if ( LodRenderData.IsValidIndex( LODIndex ) )
		{
			const TArray<FSkelMeshRenderSection>& Sections = LodRenderData[ LODIndex ].RenderSections;
			if ( Sections.IsValidIndex( SectionIndex ) )
			{
				int32 MaterialIndex = Sections[ SectionIndex ].MaterialIndex;

				const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo( LODIndex );
				if ( LODInfo && LODInfo->LODMaterialMap.IsValidIndex( SectionIndex ) )
				{
					// If we have an optional LODMaterialMap, we need to reroute the material index through it
					MaterialIndex = LODInfo->LODMaterialMap[ SectionIndex ];
				}

				return MaterialIndex;
			}
			else
			{
				UE_LOG( LogSkeletalMeshEditorSubsystem, Error, TEXT( "GetLODMaterialSlot: Invalid SectionIndex." ) );
			}
		}
		else
		{
			UE_LOG( LogSkeletalMeshEditorSubsystem, Error, TEXT( "GetLODMaterialSlot: Invalid LODIndex." ) );
		}
	}

	return INDEX_NONE;
}

bool USkeletalMeshEditorSubsystem::RenameSocket(USkeletalMesh* SkeletalMesh, FName OldName, FName NewName)
{
	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The SkeletalMesh is null."));
		return false;
	}

	if (SkeletalMesh->GetSkeleton() == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The SkeletalMesh's Skeleton is null."));
		return false;
	}

	if (OldName == NAME_None)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The OldName is None."));
		return false;
	}

	if (NewName == NAME_None)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The NewName is None."));
		return false;
	}

	if (OldName == NewName)
	{
		return false;
	}

	USkeletalMeshSocket* MeshSocket = SkeletalMesh->FindSocket(OldName);
	if (MeshSocket == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The socket named '%s' does not exist on the SkeletalMesh."), *OldName.ToString());
		return false;
	}

	USkeletalMeshSocket* SkeletonSocket = SkeletalMesh->GetSkeleton()->FindSocket(OldName);
	if (SkeletonSocket == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("RenameSocket: The socket named '%s' does not exist on the Skeleton."), *OldName.ToString());
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("RenameSocket", "Rename Socket"));
	MeshSocket->SetFlags(RF_Transactional);
	MeshSocket->Modify();
	MeshSocket->SocketName = NewName;

	SkeletonSocket->SetFlags(RF_Transactional);
	SkeletonSocket->Modify();
	SkeletonSocket->SocketName = NewName;

	FPreviewAssetAttachContainer& PreviewAssetAttachContainer = SkeletalMesh->GetPreviewAttachedAssetContainer();
	bool bMeshModified = false;
	for (int AttachedObjectIndex = 0; AttachedObjectIndex < PreviewAssetAttachContainer.Num(); ++AttachedObjectIndex)
	{
		FPreviewAttachedObjectPair& Pair = PreviewAssetAttachContainer[AttachedObjectIndex];
		if (Pair.AttachedTo == OldName)
		{
			// Only modify the mesh if we actually intend to change something. Avoids dirtying
			// meshes when we don't actually update any data on them. (such as adding a new socket)
			if (!bMeshModified)
			{
				SkeletalMesh->Modify();
				bMeshModified = true;
			}
			Pair.AttachedTo = NewName;
		}
	}

	bool bSkeletonModified = false;
	for (int AttachedObjectIndex = 0; AttachedObjectIndex < SkeletalMesh->GetSkeleton()->PreviewAttachedAssetContainer.Num(); ++AttachedObjectIndex)
	{
		FPreviewAttachedObjectPair& Pair = SkeletalMesh->GetSkeleton()->PreviewAttachedAssetContainer[AttachedObjectIndex];
		if (Pair.AttachedTo == OldName)
		{
			// Only modify the skeleton if we actually intend to change something.
			if (!bSkeletonModified)
			{
				SkeletalMesh->GetSkeleton()->Modify();
				bSkeletonModified = true;
			}
			Pair.AttachedTo = NewName;
		}
	}

	return true;
}

int32 USkeletalMeshEditorSubsystem::GetLODCount(USkeletalMesh* SkeletalMesh)
{
	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh GetLODCount: The SkeletalMesh is null."));
		return INDEX_NONE;
	}

	return SkeletalMesh->GetLODNum();
}

int32 USkeletalMeshEditorSubsystem::ImportLOD(USkeletalMesh* BaseMesh, const int32 LODIndex, const FString& SourceFilename)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ImportLOD: Cannot import or re-import when editor PIE is active."));
		return INDEX_NONE;
	}

	if (BaseMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ImportLOD: The SkeletalMesh is null."));
		return INDEX_NONE;
	}

	// Make sure the LODIndex we want to add the LOD is valid
	if (BaseMesh->GetLODNum() < LODIndex)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ImportLOD: Invalid LODIndex, the LOD index cannot be greater the the number of LOD, skeletal mesh cannot have hole in the LOD array."));
		return INDEX_NONE;
	}

	FString ResolveFilename = SourceFilename;
	const bool bSourceFileExists = FPaths::FileExists(ResolveFilename);
	if (!bSourceFileExists)
	{
		if (BaseMesh->IsValidLODIndex(LODIndex))
		{
			ResolveFilename = BaseMesh->GetLODInfo(LODIndex)->SourceImportFilename.IsEmpty() ?
				BaseMesh->GetLODInfo(LODIndex)->SourceImportFilename :
				UAssetImportData::ResolveImportFilename(BaseMesh->GetLODInfo(LODIndex)->SourceImportFilename, nullptr);
		}
	}

	if (!FPaths::FileExists(ResolveFilename))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ImportLOD: Invalid source filename."));
		return INDEX_NONE;
	}

	//We want to remove the Reduction this LOD if the following conditions are met
	if (BaseMesh->IsValidLODIndex(LODIndex) //Only test valid LOD, this function can add LODs
		&& BaseMesh->IsReductionActive(LODIndex) //We remove reduction settings only if they are active
		&& BaseMesh->GetLODInfo(LODIndex) //this test is redundant (IsReductionActive test this), but to avoid static analysis
		&& BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.BaseLOD < LODIndex //We do not remove the reduction if the reduction is base on this LOD imported data
		&& (!BaseMesh->GetLODSettings() || BaseMesh->GetLODSettings()->GetNumberOfSettings() < LODIndex)) //We do not remove the reduction if the skeletal mesh is using a LODSettings for this LOD
	{
		//Remove the reduction settings
		BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.NumOfTrianglesPercentage = 1.0f;
		BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.NumOfVertPercentage = 1.0f;
		BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.MaxNumOfTrianglesPercentage = MAX_uint32;
		BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.MaxNumOfVertsPercentage = MAX_uint32;
		BaseMesh->GetLODInfo(LODIndex)->ReductionSettings.TerminationCriterion = SMTC_NumOfTriangles;

		BaseMesh->GetLODInfo(LODIndex)->bHasBeenSimplified = false;
	}

	if (!FbxMeshUtils::ImportSkeletalMeshLOD(BaseMesh, ResolveFilename, LODIndex))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ImportLOD: Cannot import mesh LOD."));
		return INDEX_NONE;
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostLODImport(BaseMesh, LODIndex);

	return LODIndex;
}

bool USkeletalMeshEditorSubsystem::ReimportAllCustomLODs(USkeletalMesh* SkeletalMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ReimportAllCustomLODs: Cannot import or re-import when editor PIE is active."));
		return false;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ReimportAllCustomLODs: The SkeletalMesh is null."));
		return false;
	}

	bool bResult = true;
	int32 LODNumber = SkeletalMesh->GetLODNum();
	//Iterate the static mesh LODs, start at index 1
	for (int32 LODIndex = 1; LODIndex < LODNumber; ++LODIndex)
	{
		//Do not reimport LOD that was re-import with the base mesh
		if (SkeletalMesh->GetLODInfo(LODIndex)->bImportWithBaseMesh)
		{
			continue;
		}
		if (SkeletalMesh->GetLODInfo(LODIndex)->bHasBeenSimplified)
		{
			continue;
		}

		if (ImportLOD(SkeletalMesh, LODIndex, SkeletalMesh->GetLODInfo(LODIndex)->SourceImportFilename) != LODIndex)
		{
			UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SkeletalMesh ReimportAllCustomLODs: Cannot re-import LOD %d."), LODIndex);
			bResult = false;
		}
	}
	return bResult;
}

void USkeletalMeshEditorSubsystem::GetLodBuildSettings(const USkeletalMesh* SkeletalMesh, const int32 LodIndex, FSkeletalMeshBuildSettings& OutBuildOptions)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetLodReductionSettings: The SkeletalMesh is null."));
		return;
	}

	// If LOD 0 does not exist, warn and return
	if (LodIndex < 0 || SkeletalMesh->GetLODNum() <= LodIndex)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetLodReductionSettings: Invalid LOD index."));
		return;
	}

	const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LodIndex);
	//LodIndex validity was verify before
	check(LODInfo);

	// Copy over the reduction settings
	OutBuildOptions = LODInfo->BuildSettings;
}

void USkeletalMeshEditorSubsystem::SetLodBuildSettings(USkeletalMesh* SkeletalMesh, const int32 LodIndex, const FSkeletalMeshBuildSettings& BuildOptions)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("SetLodReductionSettings: The SkeletalMesh is null."));
		return;
	}

	// If LOD 0 does not exist, warn and return
	if (LodIndex < 0 || SkeletalMesh->GetLODNum() <= LodIndex)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("GetLodReductionSettings: Invalid LOD index."));
		return;
	}

	FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LodIndex);
	//LodIndex validity was verify before
	check(LODInfo);

	// Close the mesh editor to prevent crashing. If changes are applied, reopen it after the mesh has been built.
	bool bSkeletalMeshIsEdited = false;
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem->FindEditorForAsset(SkeletalMesh, false))
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(SkeletalMesh);
		bSkeletalMeshIsEdited = true;
	}

	//Copy the reduction setting on the LODInfo
	{
		FScopedSkeletalMeshPostEditChange ScopedSkeletalMeshPostEditChange(SkeletalMesh);
		SkeletalMesh->Modify();

		// Copy over the reduction settings
		LODInfo->BuildSettings = BuildOptions;
	}
	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bSkeletalMeshIsEdited)
	{
		AssetEditorSubsystem->OpenEditorForAsset(SkeletalMesh);
	}
}

UPhysicsAsset* USkeletalMeshEditorSubsystem::CreatePhysicsAsset(USkeletalMesh* SkeletalMesh)
{
	if (!SkeletalMesh)
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("CreatePhysicsAsset failed: The SkeletalMesh is null."));
		return nullptr;
	}

	FString ObjectName = FString::Printf(TEXT("%s_PhysicsAsset"), *SkeletalMesh->GetName());
	FString PackageName = SkeletalMesh->GetOutermost()->GetName();

	FString ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*PackageName), *ObjectName);
	UObject* Package = CreatePackage(*ParentPath);

	// See if an object with this name exists
	UObject* Object = LoadObject<UObject>(Package, *ObjectName, nullptr, LOAD_NoWarn | LOAD_Quiet, nullptr);

	// If an object with same name but different class exists, fail and warn the user
	if ((Object != nullptr) && (Object->GetClass() != UPhysicsAsset::StaticClass()))
	{
		UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("CreatePhysicsAsset failed: An object that is not a Physics Asset already exists with the name %s."), *ObjectName);
		return nullptr;
	}

	if (Object == nullptr)
	{
		Object = NewObject<UPhysicsAsset>(Package, *ObjectName, RF_Public | RF_Standalone);

		FAssetRegistryModule::AssetCreated(Object);
	}

	UPhysicsAsset* NewPhysicsAsset = Cast<UPhysicsAsset>(Object);
	if (NewPhysicsAsset)
	{
		NewPhysicsAsset->MarkPackageDirty();

		FPhysAssetCreateParams NewBodyData;
		FText CreationErrorMessage;

		bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(NewPhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage);
		if (!bSuccess)
		{
			UE_LOG(LogSkeletalMeshEditorSubsystem, Error, TEXT("CreatePhysicsAsset failed: Couldn't create PhysicsAsset for the SkeletalMesh."));
			ObjectTools::DeleteObjects({ NewPhysicsAsset }, false);
			return nullptr;
		}
		else
		{
			RefreshSkelMeshOnPhysicsAssetChange(SkeletalMesh);
		}
	}

	return NewPhysicsAsset;
}

bool USkeletalMeshEditorSubsystem::IsPhysicsAssetCompatible(USkeletalMesh* TargetMesh, UPhysicsAsset* PhysicsAsset)
{
	if(!TargetMesh)
	{
		return false;
	}

	// If we have a physics asset, validate it. Otherwise the user is clearing the asset by passing a nullptr
	if (PhysicsAsset)
	{
		const FReferenceSkeleton& RefSkel = TargetMesh->GetRefSkeleton();

		// Check that all of the body setups correctly map to a skeletal mesh bone
		for (USkeletalBodySetup* Setup : PhysicsAsset->SkeletalBodySetups)
		{
			if (RefSkel.FindBoneIndex(Setup->BoneName) == INDEX_NONE)
			{
				UE_LOG(LogSkeletalMeshEditorSubsystem, 
				       Error, 
				       TEXT("IsPhysicsAssetCompatible failed: bone %s is not present for skeletal mesh %s"), 
				       *Setup->BoneName.ToString(), 
				       *TargetMesh->GetName());

				return false;
			}
		}

		// Check that all of the constraints match up to existing skeletal mesh bones.
		for (UPhysicsConstraintTemplate* Constraint : PhysicsAsset->ConstraintSetup)
		{
			int32 ConstraintBoneIndices[2] = {
				RefSkel.FindBoneIndex(Constraint->DefaultInstance.ConstraintBone1),
				RefSkel.FindBoneIndex(Constraint->DefaultInstance.ConstraintBone2)
			};



			if (ConstraintBoneIndices[0] == INDEX_NONE)
			{
				UE_LOG(LogSkeletalMeshEditorSubsystem, 
				       Error, 
				       TEXT("IsPhysicsAssetCompatible failed: bone %s is not present for skeletal mesh %s"), 
				       *Constraint->DefaultInstance.ConstraintBone1.ToString(), 
				       *TargetMesh->GetName());

				return false;
			}

			if (ConstraintBoneIndices[1] == INDEX_NONE)
			{
				UE_LOG(LogSkeletalMeshEditorSubsystem, 
				       Error, 
				       TEXT("IsPhysicsAssetCompatible failed: bone %s is not present for skeletal mesh %s"), 
				       *Constraint->DefaultInstance.ConstraintBone2.ToString(), 
				       *TargetMesh->GetName());

				return false;
			}
		}
	}

	return true;
}

bool USkeletalMeshEditorSubsystem::AssignPhysicsAsset(USkeletalMesh* TargetMesh, UPhysicsAsset* PhysicsAsset)
{
	if(!IsPhysicsAssetCompatible(TargetMesh, PhysicsAsset))
	{
		return false;
	}

	{
		FScopedTransaction Transaction(LOCTEXT("SetSkeletalMeshPhysicsAsset", "Set skeletal mesh physics asset"));
		TargetMesh->Modify();
		TargetMesh->SetPhysicsAsset(PhysicsAsset);
	}

	// Refresh all currently active skeletal mesh components that may have used the old setup
	RefreshSkelMeshOnPhysicsAssetChange(TargetMesh);

	return true;
}

#undef LOCTEXT_NAMESPACE
