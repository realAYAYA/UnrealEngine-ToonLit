// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorStaticMeshLibrary.h"

#include "EditorScriptingUtils.h"

#include "ActorEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/MeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "IMeshMergeUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "LevelEditorViewport.h"
#include "Engine/MapBuildDataRegistry.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "MeshMergeModule.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "Async/ParallelFor.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"

#include "UnrealEdGlobals.h"
#include "GeomFitUtils.h"
#include "ConvexDecompTool.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorStaticMeshLibrary)

#define LOCTEXT_NAMESPACE "EditorStaticMeshLibrary"

/**
 *
 * Editor Scripting | Dataprep
 *
 **/

FStaticMeshReductionOptions UDEPRECATED_EditorStaticMeshLibrary::ConvertReductionOptions(const FEditorScriptingMeshReductionOptions_Deprecated& ReductionOptions)
{
	FStaticMeshReductionOptions MeshReductionOptions;

	MeshReductionOptions.bAutoComputeLODScreenSize = ReductionOptions.bAutoComputeLODScreenSize;

	for (int32 i = 0; i < ReductionOptions.ReductionSettings.Num(); i++)
	{
		FStaticMeshReductionSettings ReductionSetting;

		ReductionSetting.PercentTriangles = ReductionOptions.ReductionSettings[i].PercentTriangles;
		ReductionSetting.ScreenSize = ReductionOptions.ReductionSettings[i].ScreenSize;

		MeshReductionOptions.ReductionSettings.Add(ReductionSetting);
	}

	return MeshReductionOptions;
}

// Converts the deprecated EScriptingCollisionShapeType_Deprecated to the new EScriptCollisionShapeType
EScriptCollisionShapeType UDEPRECATED_EditorStaticMeshLibrary::ConvertCollisionShape(const EScriptingCollisionShapeType_Deprecated& CollisionShape)
{
	switch (CollisionShape)
	{
	case EScriptingCollisionShapeType_Deprecated::Box:
	{
		return EScriptCollisionShapeType::Box;
	}
	case EScriptingCollisionShapeType_Deprecated::Sphere:
	{
		return EScriptCollisionShapeType::Sphere;
	}
	case EScriptingCollisionShapeType_Deprecated::Capsule:
	{
		return EScriptCollisionShapeType::Capsule;
	}
	case EScriptingCollisionShapeType_Deprecated::NDOP10_X:
	{
		return EScriptCollisionShapeType::NDOP10_X;
	}
	case EScriptingCollisionShapeType_Deprecated::NDOP10_Y:
	{
		return EScriptCollisionShapeType::NDOP10_Y;
	}
	case EScriptingCollisionShapeType_Deprecated::NDOP10_Z:
	{
		return EScriptCollisionShapeType::NDOP10_Z;
	}
	case EScriptingCollisionShapeType_Deprecated::NDOP18:
	{
		return EScriptCollisionShapeType::NDOP18;
	}
	case EScriptingCollisionShapeType_Deprecated::NDOP26:
	{
		return EScriptCollisionShapeType::NDOP26;
	}
	default:
		return EScriptCollisionShapeType::Box;
	}
}

int32 UDEPRECATED_EditorStaticMeshLibrary::SetLodsWithNotification(UStaticMesh* StaticMesh, const FEditorScriptingMeshReductionOptions_Deprecated& ReductionOptions, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetLodsWithNotification(StaticMesh, ConvertReductionOptions(ReductionOptions), bApplyChanges) : -1;
}

void UDEPRECATED_EditorStaticMeshLibrary::GetLodReductionSettings(const UStaticMesh* StaticMesh, const int32 LodIndex, FMeshReductionSettings& OutReductionOptions)
{
	if (UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
	{
		StaticMeshEditorSubsystem->GetLodReductionSettings(StaticMesh, LodIndex, OutReductionOptions);
	}
}

void UDEPRECATED_EditorStaticMeshLibrary::SetLodReductionSettings(UStaticMesh* StaticMesh, const int32 LodIndex, const FMeshReductionSettings& ReductionOptions)
{
	if (UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
	{
		StaticMeshEditorSubsystem->SetLodReductionSettings(StaticMesh, LodIndex, ReductionOptions);
	}
}

void UDEPRECATED_EditorStaticMeshLibrary::GetLodBuildSettings(const UStaticMesh* StaticMesh, const int32 LodIndex, FMeshBuildSettings& OutBuildOptions)
{
	if (UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
	{
		StaticMeshEditorSubsystem->GetLodBuildSettings(StaticMesh, LodIndex, OutBuildOptions);
	}
}

void UDEPRECATED_EditorStaticMeshLibrary::SetLodBuildSettings(UStaticMesh* StaticMesh, const int32 LodIndex, const FMeshBuildSettings& BuildOptions)
{
	if (UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
	{
		StaticMeshEditorSubsystem->SetLodBuildSettings(StaticMesh, LodIndex, BuildOptions);
	}
}

int32 UDEPRECATED_EditorStaticMeshLibrary::ImportLOD(UStaticMesh* BaseStaticMesh, const int32 LODIndex, const FString& SourceFilename)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->ImportLOD(BaseStaticMesh, LODIndex, SourceFilename) : INDEX_NONE;
}

bool UDEPRECATED_EditorStaticMeshLibrary::ReimportAllCustomLODs(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->ReimportAllCustomLODs(StaticMesh) : false;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::SetLodFromStaticMesh(UStaticMesh* DestinationStaticMesh, int32 DestinationLodIndex, UStaticMesh* SourceStaticMesh, int32 SourceLodIndex, bool bReuseExistingMaterialSlots)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetLodFromStaticMesh(DestinationStaticMesh, DestinationLodIndex, SourceStaticMesh, SourceLodIndex, bReuseExistingMaterialSlots) : -1;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetLodCount(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetLodCount(StaticMesh) : -1;
}

bool UDEPRECATED_EditorStaticMeshLibrary::RemoveLods(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->RemoveLods(StaticMesh) : false;
}

TArray<float> UDEPRECATED_EditorStaticMeshLibrary::GetLodScreenSizes(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	TArray<float> ScreenSizes;

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetLodScreenSizes(StaticMesh) : ScreenSizes;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::AddSimpleCollisionsWithNotification(UStaticMesh* StaticMesh, const EScriptingCollisionShapeType_Deprecated ShapeType, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->AddSimpleCollisionsWithNotification(StaticMesh, ConvertCollisionShape(ShapeType), bApplyChanges) : INDEX_NONE;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetSimpleCollisionCount(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetSimpleCollisionCount(StaticMesh) : -1;
}

TEnumAsByte<ECollisionTraceFlag> UDEPRECATED_EditorStaticMeshLibrary::GetCollisionComplexity(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		return StaticMeshEditorSubsystem->GetCollisionComplexity(StaticMesh);
	}
	return ECollisionTraceFlag::CTF_UseDefault;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetConvexCollisionCount(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetConvexCollisionCount(StaticMesh) : -1;
}

bool UDEPRECATED_EditorStaticMeshLibrary::BulkSetConvexDecompositionCollisionsWithNotification(const TArray<UStaticMesh*>& InStaticMeshes, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->BulkSetConvexDecompositionCollisionsWithNotification(InStaticMeshes, HullCount, MaxHullVerts, HullPrecision, bApplyChanges) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::SetConvexDecompositionCollisionsWithNotification(UStaticMesh* StaticMesh, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetConvexDecompositionCollisionsWithNotification(StaticMesh, HullCount, MaxHullVerts, HullPrecision, bApplyChanges) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::RemoveCollisionsWithNotification(UStaticMesh* StaticMesh, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->RemoveCollisionsWithNotification(StaticMesh, bApplyChanges) : false;
}

void UDEPRECATED_EditorStaticMeshLibrary::EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		return StaticMeshEditorSubsystem->EnableSectionCollision(StaticMesh, bCollisionEnabled, LODIndex, SectionIndex);
	}
}

bool UDEPRECATED_EditorStaticMeshLibrary::IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->IsSectionCollisionEnabled(StaticMesh, LODIndex, SectionIndex) : false;
}

void UDEPRECATED_EditorStaticMeshLibrary::EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		return StaticMeshEditorSubsystem->EnableSectionCastShadow(StaticMesh, bCastShadow, LODIndex, SectionIndex);
	}
}

bool UDEPRECATED_EditorStaticMeshLibrary::HasVertexColors(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->HasVertexColors(StaticMesh) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->HasInstanceVertexColors(StaticMeshComponent) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetGenerateLightmapUVs(StaticMesh, bGenerateLightmapUVs) : false;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetNumberVerts(StaticMesh, LODIndex) : 0;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetNumberMaterials(UStaticMesh* StaticMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetNumberMaterials(StaticMesh) : 0;
}

void UDEPRECATED_EditorStaticMeshLibrary::SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		return StaticMeshEditorSubsystem->SetAllowCPUAccess(StaticMesh, bAllowCPUAccess);
	}
}

int32 UDEPRECATED_EditorStaticMeshLibrary::GetNumUVChannels(UStaticMesh* StaticMesh, int32 LODIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GetNumUVChannels(StaticMesh, LODIndex) : 0;
}

bool UDEPRECATED_EditorStaticMeshLibrary::AddUVChannel(UStaticMesh* StaticMesh, int32 LODIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->AddUVChannel(StaticMesh, LODIndex) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::InsertUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->InsertUVChannel(StaticMesh, LODIndex, UVChannelIndex) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::RemoveUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->RemoveUVChannel(StaticMesh, LODIndex, UVChannelIndex) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::GeneratePlanarUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GeneratePlanarUVChannel(StaticMesh, LODIndex, UVChannelIndex, Position, Orientation, Tiling) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::GenerateCylindricalUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GenerateCylindricalUVChannel(StaticMesh, LODIndex, UVChannelIndex, Position, Orientation, Tiling) : false;
}

bool UDEPRECATED_EditorStaticMeshLibrary::GenerateBoxUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector& Size)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->GenerateBoxUVChannel(StaticMesh, LODIndex, UVChannelIndex, Position, Orientation, Size) : false;
}

// The functions below are BP exposed copies of functions that use deprecated structs, updated to the new structs in StaticMeshEditorSubsytem
// The old structs redirect to the new ones, so this makes blueprints that use the old structs still work
// The old functions are still available as an overload, which makes old code that uses them compatible

int32 UDEPRECATED_EditorStaticMeshLibrary::SetLodsWithNotification(UStaticMesh* StaticMesh, const FStaticMeshReductionOptions& ReductionOptions, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetLodsWithNotification(StaticMesh, ReductionOptions, bApplyChanges) : -1;
}

int32 UDEPRECATED_EditorStaticMeshLibrary::AddSimpleCollisionsWithNotification(UStaticMesh* StaticMesh, const EScriptCollisionShapeType ShapeType, bool bApplyChanges)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->AddSimpleCollisionsWithNotification(StaticMesh, ShapeType, bApplyChanges) : INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE

