// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMasterLockitFactory.h"
#include "LiveLinkMasterLockitConnectionSettings.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SLiveLinkMasterLockitSourcePanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkMasterLockitSourcePanel) {}
		SLATE_ARGUMENT(const ULiveLinkMasterLockitSourceFactory*, Factory)
		SLATE_EVENT(ULiveLinkSourceFactory::FOnLiveLinkSourceCreated, OnSourceCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

public:
	FReply CreateNewSource(bool bShouldCreateSource);

private:
	FLiveLinkMasterLockitConnectionSettings ConnectionSettings;
	TWeakObjectPtr<const ULiveLinkMasterLockitSourceFactory> SourceFactory;
	ULiveLinkSourceFactory::FOnLiveLinkSourceCreated OnSourceCreated;
};