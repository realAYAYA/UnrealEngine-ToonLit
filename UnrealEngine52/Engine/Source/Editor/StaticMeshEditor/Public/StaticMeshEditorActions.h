// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FUICommandInfo;

/**
 * Unreal StaticMesh editor actions
 */
class FStaticMeshEditorCommands : public TCommands<FStaticMeshEditorCommands>
{

public:
	FStaticMeshEditorCommands() : TCommands<FStaticMeshEditorCommands>
		(
			"StaticMeshEditor", // Context name for fast lookup
			NSLOCTEXT("Contexts", "StaticMeshEditor", "StaticMesh Editor"), // Localized context name for displaying
			"EditorViewport",  // Parent
			FAppStyle::GetAppStyleSetName() // Icon Style Set
			)
	{
	}

	/**
	 * StaticMesh Editor Commands
	 */

	 /**  */
	TSharedPtr<FUICommandInfo> SetShowNaniteFallback;
	TSharedPtr<FUICommandInfo> SetShowWireframe;
	TSharedPtr<FUICommandInfo> SetShowVertexColor;
	TSharedPtr<FUICommandInfo> SetShowPhysicalMaterialMasks;
	TSharedPtr<FUICommandInfo> SetDrawUVs;
	TSharedPtr<FUICommandInfo> SetShowGrid;
	TSharedPtr<FUICommandInfo> SetShowBounds;
	TSharedPtr<FUICommandInfo> SetShowSimpleCollision;
	TSharedPtr<FUICommandInfo> SetShowComplexCollision;
	TSharedPtr<FUICommandInfo> ResetCamera;
	TSharedPtr<FUICommandInfo> SetShowSockets;
	TSharedPtr<FUICommandInfo> SetDrawAdditionalData;

	// Mesh toolbar Commands
	TSharedPtr<FUICommandInfo> ReimportMesh;
	TSharedPtr<FUICommandInfo> ReimportMeshWithNewFile;
	TSharedPtr<FUICommandInfo> ReimportAllMesh;
	TSharedPtr<FUICommandInfo> ReimportAllMeshWithNewFile;

	// toolbar commands
	TSharedPtr<FUICommandInfo> ToggleShowNormals;
	TSharedPtr<FUICommandInfo> ToggleShowTangents;
	TSharedPtr<FUICommandInfo> ToggleShowBinormals;
	TSharedPtr<FUICommandInfo> ToggleShowPivots;
	TSharedPtr<FUICommandInfo> ToggleShowVertices;
	TSharedPtr<FUICommandInfo> ToggleShowGrids;
	TSharedPtr<FUICommandInfo> ToggleShowBounds;
	TSharedPtr<FUICommandInfo> ToggleShowSimpleCollisions;
	TSharedPtr<FUICommandInfo> ToggleShowComplexCollisions;
	TSharedPtr<FUICommandInfo> ToggleShowSockets;
	TSharedPtr<FUICommandInfo> ToggleShowWireframes;
	TSharedPtr<FUICommandInfo> ToggleShowVertexColors;

	// View Menu Commands
	TSharedPtr<FUICommandInfo> SetShowNormals;
	TSharedPtr<FUICommandInfo> SetShowTangents;
	TSharedPtr<FUICommandInfo> SetShowBinormals;
	TSharedPtr<FUICommandInfo> SetShowPivot;
	TSharedPtr<FUICommandInfo> SetShowVertices;

	// Collision Menu Commands
	TSharedPtr<FUICommandInfo> CreateDOP10X;
	TSharedPtr<FUICommandInfo> CreateDOP10Y;
	TSharedPtr<FUICommandInfo> CreateDOP10Z;
	TSharedPtr<FUICommandInfo> CreateDOP18;
	TSharedPtr<FUICommandInfo> CreateDOP26;
	TSharedPtr<FUICommandInfo> CreateBoxCollision;
	TSharedPtr<FUICommandInfo> CreateSphereCollision;
	TSharedPtr<FUICommandInfo> CreateSphylCollision;
	TSharedPtr<FUICommandInfo> CreateAutoConvexCollision;
	TSharedPtr<FUICommandInfo> RemoveCollision;
	TSharedPtr<FUICommandInfo> ConvertBoxesToConvex;
	TSharedPtr<FUICommandInfo> CopyCollisionFromSelectedMesh;

	// Mesh Menu Commands
	TSharedPtr<FUICommandInfo> FindSource;

	TSharedPtr<FUICommandInfo> ChangeMesh;

	TSharedPtr<FUICommandInfo> BakeMaterials;

	TSharedPtr<FUICommandInfo> SaveGeneratedLODs;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};
