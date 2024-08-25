// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "Templates/SharedPointer.h"

class FTG_EditorGraphPanelPinFactory :
	public FGraphPanelPinFactory
{
public:
	// FGraphPanelPinFactory interface
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;

private:
	TSharedPtr<class SGraphPin> CreateEnumPin(class UEdGraphPin* InPin) const;
	TSharedPtr<SWidget> FindWidgetInChildren(TSharedPtr<SWidget> Parent, FName ChildType) const;
};
