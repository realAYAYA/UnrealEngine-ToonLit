// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/UnrealPortabilityHelpers.h"

#include "MuCO/ICustomizableObjectModule.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Commands/InputChord.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "Materials/Material.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include <functional>

//---------------------------------------------------------------
// Helpers to ease portability across unreal engine versions
//---------------------------------------------------------------


inline bool Helper_DoSectionsUse16BitBoneIndex( const FSkeletalMeshLODModel& m )
{
	for (int32 SectionIdx = 0; SectionIdx < m.Sections.Num(); ++SectionIdx)
	{
		if (m.Sections[SectionIdx].Use16BitBoneIndex())
		{
			return true;
		}
	}

	return false;
}

inline int Helper_GetMaxBoneInfluences(const FSkeletalMeshLODModel& m)
{
	return m.GetMaxBoneInfluences();
}


inline FMaterialRenderProxy* Helper_GetMaterialProxy( const UMaterialInterface* Material )
{
	return Material->GetRenderProxy();
}


inline const TArray<FAssetData>& Helper_GetAssets(FAssetDragDropOp* DragDropOp)
{
	return DragDropOp->GetAssets();
}

inline FName Helper_GetPropertyName(FPropertyChangedEvent& PropertyChangedEvent)
{
	return PropertyChangedEvent.GetPropertyName();
}

inline FName Helper_GetPinFName(const UEdGraphPin* Pin)
{
	return Pin ? Pin->PinName: FName();
}

inline FString Helper_GetPinName(const UEdGraphPin* Pin)
{
	return Pin ? Pin->PinName.ToString() : FString();
}

inline void Helper_SetPinName( FName& To, const FString& From )
{
	To = FName(*From);
}

inline const FName& Helper_GetPinCategory(const FName& Cat)
{
	return Cat;
}

inline void Helper_GetMeshIndices(TArray<uint32>& Indices, USkeletalMesh* SkeletalMesh, int LODIndex)
{
	Indices = Helper_GetImportedModel(SkeletalMesh)->LODModels[LODIndex].IndexBuffer;
}


inline void Helper_ForEachTextureParameterName(const UMaterial* Material, const std::function<void(const FString&)>& f)
{
	TArray<FMaterialParameterInfo> ImageNames;
	TArray<FGuid> ImageIds;
	Material->GetAllTextureParameterInfo(ImageNames, ImageIds);
	for (int32 ImageIndex = 0; ImageIndex < ImageNames.Num(); ++ImageIndex)
	{
		FString ImageName = ImageNames[ImageIndex].Name.ToString();
		f(ImageName);
	}
}


inline FStaticMeshVertexBuffer& Helper_GetStaticMeshVertexBuffer(UStaticMesh* StaticMesh, int LODIndex)
{
	return StaticMesh->GetRenderData()->LODResources[LODIndex].VertexBuffers.StaticMeshVertexBuffer;
}


inline const FPositionVertexBuffer& Helper_GetStaticMeshPositionVertexBuffer(UStaticMesh* StaticMesh, int LODIndex)
{
	return StaticMesh->GetRenderData()->LODResources[LODIndex].VertexBuffers.PositionVertexBuffer;
}

inline const TMap<FName, uint8*>& Helper_GetRowMap(UDataTable* Table)
{
	return Table->GetRowMap();
}

inline UAssetEditorSubsystem* Helper_GetEditorSubsystem()
{
	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
}



