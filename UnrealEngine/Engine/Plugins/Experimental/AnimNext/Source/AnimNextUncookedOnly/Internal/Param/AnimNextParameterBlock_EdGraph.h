// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraph.h"
#include "AnimNextParameterBlock_EdGraph.generated.h"

class UAnimNextParameterBlockGraph;
class UAnimNextRigVMAssetEditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EdGraph : public URigVMEdGraph
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlockGraph;
	friend class UAnimNextParameterBlock_EditorData;

	// UObject interface
	virtual void PostLoad() override;

	// UControlRigGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;

	void Initialize(UAnimNextRigVMAssetEditorData* InEditorData);
};