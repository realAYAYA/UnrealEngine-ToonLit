// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/RigVMEdGraphPanelPinFactory.h"

class SGraphPin;

namespace UE::AnimNext::Editor
{

class FParametersGraphPanelPinFactory : public FRigVMEdGraphPanelPinFactory
{
	virtual FName GetFactoryName() const;
	virtual TSharedPtr<SGraphPin> CreatePin_Internal(UEdGraphPin* InPin) const override;
};

}