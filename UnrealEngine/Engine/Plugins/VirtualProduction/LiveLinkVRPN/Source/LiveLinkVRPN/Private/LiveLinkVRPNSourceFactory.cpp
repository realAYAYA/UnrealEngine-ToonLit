// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkVRPNSourceFactory.h"
#include "LiveLinkVRPNSource.h"
#include "SLiveLinkVRPNSourceFactory.h"
#include "LiveLinkVRPNSourceSettings.h"

#include "ILiveLinkClient.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "LiveLinkVRPNSourceFactory"

FText ULiveLinkVRPNSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "LiveLinkVRPN Source");	
}

FText ULiveLinkVRPNSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Allows creation of multiple LiveLink sources using the VRPN protocol");
}

TSharedPtr<SWidget> ULiveLinkVRPNSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(SLiveLinkVRPNSourceFactory)
		.OnConnectionSettingsAccepted(FOnLiveLinkVRPNConnectionSettingsAccepted::CreateUObject(this, &ULiveLinkVRPNSourceFactory::CreateSourceFromSettings, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> ULiveLinkVRPNSourceFactory::CreateSource(const FString& ConnectionString) const
{
	FLiveLinkVRPNConnectionSettings ConnectionSettings;
	if (!ConnectionString.IsEmpty())
	{
		FLiveLinkVRPNConnectionSettings::StaticStruct()->ImportText(*ConnectionString, &ConnectionSettings, nullptr, PPF_None, GLog, TEXT("ULiveLinkVRPNSourceFactory"));
	}
	return MakeShared<FLiveLinkVRPNSource>(ConnectionSettings);
}

void ULiveLinkVRPNSourceFactory::CreateSourceFromSettings(FLiveLinkVRPNConnectionSettings InConnectionSettings, FOnLiveLinkSourceCreated OnSourceCreated) const
{
	FString ConnectionString;
	FLiveLinkVRPNConnectionSettings::StaticStruct()->ExportText(ConnectionString, &InConnectionSettings, nullptr, nullptr, PPF_None, nullptr);

	TSharedPtr<FLiveLinkVRPNSource> SharedPtr = MakeShared<FLiveLinkVRPNSource>(InConnectionSettings);
	OnSourceCreated.ExecuteIfBound(SharedPtr, MoveTemp(ConnectionString));
}

#undef LOCTEXT_NAMESPACE
