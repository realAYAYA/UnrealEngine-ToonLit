// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"

class RIGVMEDITOR_API FRigVMEdGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual FName GetFactoryName() const;
private:
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override;
};
