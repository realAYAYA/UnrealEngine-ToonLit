// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkVRPNConnectionSettings.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#if WITH_EDITOR
#include "IStructureDetailsView.h"
#endif //WITH_EDITOR

#include "Input/Reply.h"

struct FLiveLinkVRPNConnectionSettings;

DECLARE_DELEGATE_OneParam(FOnLiveLinkVRPNConnectionSettingsAccepted, FLiveLinkVRPNConnectionSettings);

class SLiveLinkVRPNSourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkVRPNSourceFactory)
	{}
		SLATE_EVENT(FOnLiveLinkVRPNConnectionSettingsAccepted, OnConnectionSettingsAccepted)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);


private:
	FLiveLinkVRPNConnectionSettings ConnectionSettings;

#if WITH_EDITOR
	TSharedPtr<FStructOnScope> StructOnScope;
	TSharedPtr<IStructureDetailsView> StructureDetailsView;
#endif //WITH_EDITOR

	FReply OnSettingsAccepted();
	FOnLiveLinkVRPNConnectionSettingsAccepted OnConnectionSettingsAccepted;
};