// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraph.h"
#include "AnimNextGraph_EdGraph.generated.h"

class UAnimNextGraph_EditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraph : public UControlRigGraph
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;

	// UControlRigGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UAnimNextGraph_EditorData* InEditorData);
};