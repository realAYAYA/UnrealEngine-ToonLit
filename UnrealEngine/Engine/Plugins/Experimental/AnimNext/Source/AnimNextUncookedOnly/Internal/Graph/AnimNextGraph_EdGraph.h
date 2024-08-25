// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraph.h"
#include "AnimNextGraph_EdGraph.generated.h"

class UAnimNextGraphEntry;
class UAnimNextRigVMAssetEditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/**
  * Wraps UEdGraph which represents the node graph
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraph : public URigVMEdGraph
{
	GENERATED_BODY()

	friend class UAnimNextGraphEntry;
	friend class UAnimNextGraph_EditorData;

	// UObject interface
	virtual void PostLoad() override;

	// URigVMEdGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UAnimNextRigVMAssetEditorData* InEditorData);
};