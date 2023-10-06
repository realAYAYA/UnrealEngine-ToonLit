// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraph.h"
#include "AnimNextParameterBlock_EdGraph.generated.h"

class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EdGraph : public UControlRigGraph
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;

	// UControlRigGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UAnimNextParameterBlock_EditorData* InEditorData);
};