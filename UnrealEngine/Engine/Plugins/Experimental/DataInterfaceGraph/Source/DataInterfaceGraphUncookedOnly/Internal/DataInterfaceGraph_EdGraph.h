// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraph.h"
#include "DataInterfaceGraph_EdGraph.generated.h"

class UDataInterfaceGraph_EditorData;

namespace UE::DataInterfaceUncookedOnly
{
struct FUtils;
}

UCLASS(MinimalAPI)
class UDataInterfaceGraph_EdGraph : public UControlRigGraph
{
	GENERATED_BODY()

	friend class UDataInterfaceGraph_EditorData;

	// UControlRigGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UDataInterfaceGraph_EditorData* InEditorData);
};