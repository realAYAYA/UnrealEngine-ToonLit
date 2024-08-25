// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownTabFactory.h"

class FAvaRundownChannelStatusListTabFactory : public FAvaRundownTabFactory
{
public:
	static const FName TabID;
	
	FAvaRundownChannelStatusListTabFactory(const TSharedPtr<FAvaRundownEditor>& InRundownEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
