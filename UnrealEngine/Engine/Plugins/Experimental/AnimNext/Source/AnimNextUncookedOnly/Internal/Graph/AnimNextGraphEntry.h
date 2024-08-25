// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AnimNextGraphEntry.generated.h"

class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
enum class ERigVMGraphNotifType : uint8;

namespace UE::AnimNext::Editor
{
	struct FUtils;
}

/** A single entry in an AnimNext graph asset */
UCLASS(MinimalAPI, Category = "Animation Graphs")
class UAnimNextGraphEntry : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::Editor::FUtils;	

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override;
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	
	// IAnimNextRigVMGraphInterface interface
	virtual URigVMGraph* GetRigVMGraph() const override;
	virtual URigVMEdGraph* GetEdGraph() const override;

protected:
	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName GraphName;

	/** RigVM graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;

	/** Editor graph */
	UPROPERTY()
	TObjectPtr<UAnimNextGraph_EdGraph> EdGraph;
};