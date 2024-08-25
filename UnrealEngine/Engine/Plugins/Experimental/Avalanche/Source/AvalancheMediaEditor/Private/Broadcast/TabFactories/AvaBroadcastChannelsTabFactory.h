// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaBroadcastTabFactory.h"

class FAvaBroadcastChannelsTabFactory : public FAvaBroadcastTabFactory
{
public:
	static const FName TabID;
	
	FAvaBroadcastChannelsTabFactory(const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);

	//~ Begin FWorkflowTabFactory Interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual const FSlateBrush* GetTabIcon(const FWorkflowTabSpawnInfo& Info) const override;
	//~ End FWorkflowTabFactory Interface
};
