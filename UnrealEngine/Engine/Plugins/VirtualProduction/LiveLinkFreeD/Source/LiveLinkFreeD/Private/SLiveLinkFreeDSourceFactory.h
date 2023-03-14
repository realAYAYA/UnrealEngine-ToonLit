// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFreeDConnectionSettings.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#if WITH_EDITOR
#include "IStructureDetailsView.h"
#endif //WITH_EDITOR

#include "Input/Reply.h"

struct FLiveLinkFreeDConnectionSettings;

DECLARE_DELEGATE_OneParam(FOnLiveLinkFreeDConnectionSettingsAccepted, FLiveLinkFreeDConnectionSettings);

class SLiveLinkFreeDSourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkFreeDSourceFactory)
	{}
		SLATE_EVENT(FOnLiveLinkFreeDConnectionSettingsAccepted, OnConnectionSettingsAccepted)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);


private:
	FLiveLinkFreeDConnectionSettings ConnectionSettings;

#if WITH_EDITOR
	TSharedPtr<FStructOnScope> StructOnScope;
	TSharedPtr<IStructureDetailsView> StructureDetailsView;
#endif //WITH_EDITOR

	FReply OnSettingsAccepted();
	FOnLiveLinkFreeDConnectionSettingsAccepted OnConnectionSettingsAccepted;
};