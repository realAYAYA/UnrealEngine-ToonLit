// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkPrestonMDRFactory.h"
#include "LiveLinkPrestonMDRConnectionSettings.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SLiveLinkPrestonMDRSourcePanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkPrestonMDRSourcePanel) {}
		SLATE_ARGUMENT(const ULiveLinkPrestonMDRSourceFactory*, Factory)
		SLATE_EVENT(ULiveLinkSourceFactory::FOnLiveLinkSourceCreated, OnSourceCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

public:
	FReply CreateNewSource(bool bShouldCreateSource);

private:
	FLiveLinkPrestonMDRConnectionSettings ConnectionSettings;
	TWeakObjectPtr<const ULiveLinkPrestonMDRSourceFactory> SourceFactory;
	ULiveLinkSourceFactory::FOnLiveLinkSourceCreated OnSourceCreated;
};