// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaBroadcastTabFactory.h"

class FAvaBroadcastDetailsTabFactory : public FAvaBroadcastTabFactory
{
public:
	static const FName TabID;
	
	FAvaBroadcastDetailsTabFactory(const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
