// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackTabFactory.h"

class FAvaPlaybackDetailsTabFactory : public FAvaPlaybackTabFactory
{
public:
	static const FName TabID;
	
	FAvaPlaybackDetailsTabFactory(const TSharedPtr<FAvaPlaybackGraphEditor>& InPlaybackEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
