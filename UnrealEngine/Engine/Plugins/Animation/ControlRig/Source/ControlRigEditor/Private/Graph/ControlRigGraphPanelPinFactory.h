// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/RigVMEdGraphPanelPinFactory.h"

class FControlRigGraphPanelPinFactory : public FRigVMEdGraphPanelPinFactory
{
public:
	// FGraphPanelPinFactory interface
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;

	// FRigVMEdGraphPanelPinFactory interface
	virtual FName GetFactoryName() const override;
	virtual TSharedPtr<class SGraphPin> CreatePin_Internal(class UEdGraphPin* InPin) const override;
};
