// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "EditorDynamicMeshUtilityFunctions.generated.h"

namespace UE::Geometry
{ 
	struct FDynamicMeshChangeContainerInternals;
}


/**
 * FDynamicMeshChangeContainer is a temporary struct usable in Blueprints to
 * emit "change" transactions for a UDynamicMesh. The internals are hidden
 * and this type is only intended to be used with the BeginTrackedMeshChange()
 * and EmitTrackedMeshChange() functions
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Dynamic Mesh ChangeTracker"))
struct GEOMETRYSCRIPTINGEDITOR_API FDynamicMeshChangeContainer
{
	GENERATED_BODY()
public:
	bool bIsActive = false;

	// This is only used as a token to ensure that the ChangeContainer is not applied to a different UDynamicMesh,
	// so it is not a uproperty/etc
	UDynamicMesh* TargetMesh;

	// internals are only available inside the cpp file and are likely to change
	TSharedPtr<UE::Geometry::FDynamicMeshChangeContainerInternals> ChangeInternals;
};





UCLASS(meta = (ScriptName = "GeometryScript_EditorDynamicMeshUtil"))
class GEOMETRYSCRIPTINGEDITOR_API UGeometryScriptLibrary_EditorDynamicMeshFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Save current state of TargetMesh so that an undoable/redoable Change can be emitted 
	 * after TargetMesh is modified, using EmitTrackedMeshChange().
	 * @param ChangeTracker output structure containing initial TargetMesh state
	 */
	UFUNCTION(BlueprintCallable, Category = "DynamicMesh|Changes")
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	BeginTrackedMeshChange(UDynamicMesh* TargetMesh, FDynamicMeshChangeContainer& ChangeTracker);

	/**
	 * Emit an undo/redo Change for a modified TargetMesh, based on the ChangeTracker information that was 
	 * saved (via call to BeginTrackedMeshChange) before TargetMesh was modified. This function must be
	 * called in the context of a Transaction (ie BeginTransaction / EndTransaction pair)
	 */
	UFUNCTION(BlueprintCallable, Category = "DynamicMesh|Changes")
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	EmitTrackedMeshChange(UDynamicMesh* TargetMesh, UPARAM(ref) FDynamicMeshChangeContainer& ChangeTracker);

};