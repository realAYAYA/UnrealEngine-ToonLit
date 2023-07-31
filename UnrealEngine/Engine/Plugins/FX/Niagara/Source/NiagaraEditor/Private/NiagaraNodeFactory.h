// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NodeFactory.h"

class FNiagaraNodeFactory : public FGraphNodeFactory
{
public:
    virtual ~FNiagaraNodeFactory();
	/** Create a widget for the supplied node */
	virtual TSharedPtr<class SGraphNode> CreateNodeWidget(class UEdGraphNode* InNode) override;
};
