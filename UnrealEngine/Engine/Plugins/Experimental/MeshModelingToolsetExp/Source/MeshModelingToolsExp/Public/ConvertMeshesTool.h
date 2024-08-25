// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h" // IInteractiveToolExclusiveToolAPI
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "TargetInterfaces/MeshTargetInterfaceTypes.h"

#include "ConvertMeshesTool.generated.h"

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertMeshesToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const;
};

/**
 * Standard properties of the Transfer operation
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "bShowTransferMaterials == true", EditConditionHides, HideEditConditionToggle))
	bool bTransferMaterials = true;

	// control whether the transfer materials option is displayed
	UPROPERTY(meta = (TransientToolProperty))
	bool bShowTransferMaterials = true;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bTransferCollision = true;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertMeshesTool : public UInteractiveTool,
	// Disallow auto-accept switch-away for the tool
	public IInteractiveToolExclusiveToolAPI
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	UPROPERTY()
	TObjectPtr<UConvertMeshesToolProperties> BasicProperties;

	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	void InitializeInputs(TArray<TWeakObjectPtr<UPrimitiveComponent>>&& InInputs)
	{
		Inputs = MoveTemp(InInputs);
	}

	void SetTargetLOD(EMeshLODIdentifier LOD)
	{
		TargetLOD = LOD;
	}

private:
 	UPROPERTY()
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Inputs;

	EMeshLODIdentifier TargetLOD = EMeshLODIdentifier::Default;
};
