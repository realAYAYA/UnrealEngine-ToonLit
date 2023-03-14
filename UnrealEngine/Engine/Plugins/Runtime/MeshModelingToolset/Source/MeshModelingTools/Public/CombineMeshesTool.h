// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h" // IInteractiveToolExclusiveToolAPI
#include "MeshOpPreviewHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "BaseTools/BaseCreateFromSelectedTool.h"
#include "PropertySets/OnAcceptProperties.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "CombineMeshesTool.generated.h"

// Forward declarations
struct FMeshDescription;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UCombineMeshesToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	bool bIsDuplicateTool = false;

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


/**
 * Common properties
 */
UCLASS()
class MESHMODELINGTOOLS_API UCombineMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (TransientToolProperty))
	bool bIsDuplicateMode = false;

	/** Defines the object the tool output is written to. */
	UPROPERTY(EditAnywhere, Category = OutputObject, meta = (DisplayName = "Write To",
		EditCondition = "bIsDuplicateMode == false", EditConditionHides, HideEditConditionToggle))
	EBaseCreateFromSelectedTargetType OutputWriteTo = EBaseCreateFromSelectedTargetType::NewObject;

	/** Base name of the newly generated object to which the output is written to. */
	UPROPERTY(EditAnywhere, Category = OutputObject, meta = (TransientToolProperty, DisplayName = "Name",
		EditCondition = "bIsDuplicateMode || OutputWriteTo == EBaseCreateFromSelectedTargetType::NewObject", EditConditionHides, NoResetToDefault))
	FString OutputNewName;

	/** Name of the existing object to which the output is written to. */
	UPROPERTY(VisibleAnywhere, Category = OutputObject, meta = (TransientToolProperty, DisplayName = "Name",
		EditCondition = "bIsDuplicateMode == false && OutputWriteTo != EBaseCreateFromSelectedTargetType::NewObject", EditConditionHides))
	FString OutputExistingName;
};


/**
 * Simple tool to combine multiple meshes into a single mesh asset
 */
UCLASS()
class MESHMODELINGTOOLS_API UCombineMeshesTool : public UMultiSelectionMeshEditingTool, 
	// Disallow auto-accept switch-away because it's easy to accidentally make an extra asset in duplicate mode,
	// and it's not great in combine mode either.
	public IInteractiveToolExclusiveToolAPI
{
	GENERATED_BODY()

public:
	virtual void SetDuplicateMode(bool bDuplicateMode);

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

protected:

	UPROPERTY()
	TObjectPtr<UCombineMeshesToolProperties> BasicProperties;

	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	UPROPERTY()
	TObjectPtr<UOnAcceptHandleSourcesPropertiesBase> HandleSourceProperties;

	bool bDuplicateMode;

	void CreateNewAsset();
	void UpdateExistingAsset();

	void BuildCombinedMaterialSet(TArray<UMaterialInterface*>& NewMaterialsOut, TArray<TArray<int32>>& MaterialIDRemapsOut);
};
