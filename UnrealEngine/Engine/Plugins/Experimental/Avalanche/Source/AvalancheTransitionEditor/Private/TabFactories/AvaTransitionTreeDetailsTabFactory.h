// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionTabFactory.h"

class FAvaTransitionTreeDetailsTabFactory : public FAvaTransitionTabFactory
{
public:
	static const FName TabId;

	explicit FAvaTransitionTreeDetailsTabFactory(const TSharedRef<FAvaTransitionEditor>& InEditor);

	//~ Begin FWorkflowTabFactory
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;
	//~ End FWorkflowTabFactory
};
