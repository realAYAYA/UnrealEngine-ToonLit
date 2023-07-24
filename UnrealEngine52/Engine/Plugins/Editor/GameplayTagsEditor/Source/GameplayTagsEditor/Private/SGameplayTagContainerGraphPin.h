// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGameplayTagGraphPin.h"

/** Almost the same as a tag pin, but supports multiple tags */
class SGameplayTagContainerGraphPin : public SGameplayTagGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGameplayTagContainerGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGameplayTagGraphPin Interface
	virtual void ParseDefaultValueData() override;
	virtual void SaveDefaultValueData() override;
	virtual TSharedRef<SWidget> GetEditContent() override;
	//~ End SGraphPin Interface
};
