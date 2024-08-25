// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AnimNextParameterBlockGraph.generated.h"

class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;
class UAnimNextParameterBlock_EdGraph;

UCLASS(Category = "Parameter Graphs")
class UAnimNextParameterBlockGraph : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override { return GraphName; }
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextRigVMGraphInterface interface
	virtual URigVMGraph* GetRigVMGraph() const override;
	virtual URigVMEdGraph* GetEdGraph() const override;

	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName GraphName;

	/** Graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;

	/** Graph */
	UPROPERTY()
	TObjectPtr<UAnimNextParameterBlock_EdGraph> EdGraph;
};