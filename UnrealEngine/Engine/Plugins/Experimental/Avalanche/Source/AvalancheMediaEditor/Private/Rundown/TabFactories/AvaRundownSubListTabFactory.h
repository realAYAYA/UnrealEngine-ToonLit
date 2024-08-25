// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownTabFactory.h"

class FAvaRundownEditor;

//Base class for all Tab Factories in Ava SubList Editor
class FAvaRundownSubListTabFactory : public FAvaRundownTabFactory
{
public:
	static const FName TabID;

	FAvaRundownSubListTabFactory(const TSharedPtr<FAvaRundownEditor>& InRundownEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;
};

