// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigGraphNode.h"
#include "GraphEditorDragDropAction.h"
#include "RigVMModel/RigVMGraph.h"
#include "EdGraphSchema_K2_Actions.h"
#include "EdGraph/RigVMEdGraphSchema.h"

#include "ControlRigGraphSchema.generated.h"

class UControlRigBlueprint;
class UControlRigGraph;
class UControlRigGraphNode;
class UControlRigGraphNode_Unit;
class UControlRigGraphNode_Property;

UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraphSchema : public URigVMEdGraphSchema
{
	GENERATED_BODY()

public:
	/** Name constants */
	static const FName GraphName_ControlRig;

public:
	UControlRigGraphSchema();

	// UEdGraphSchema interface
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;

	// URigVMEdGraphSchema interface
	virtual const FName& GetRootGraphName() const override { return GraphName_ControlRig; }
	virtual bool IsRigVMDefaultEvent(const FName& InEventName) const override;
};
