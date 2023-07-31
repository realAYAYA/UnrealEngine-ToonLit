// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeFactory.h"

class SGraphNode;
class UEdGraphNode;

class FNiagaraOverviewGraphNodeFactory : public FGraphNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNodeWidget(UEdGraphNode* InNode) override;
};