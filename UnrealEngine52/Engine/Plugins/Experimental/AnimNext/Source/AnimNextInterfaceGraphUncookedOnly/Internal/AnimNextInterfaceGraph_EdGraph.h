// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraph.h"
#include "AnimNextInterfaceGraph_EdGraph.generated.h"

class UAnimNextInterfaceGraph_EditorData;

namespace UE::AnimNext::InterfaceGraphUncookedOnly
{
	struct FUtils;
}

UCLASS(MinimalAPI)
class UAnimNextInterfaceGraph_EdGraph : public UControlRigGraph
{
	GENERATED_BODY()

	friend class UAnimNextInterfaceGraph_EditorData;

	// UControlRigGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UAnimNextInterfaceGraph_EditorData* InEditorData);
};