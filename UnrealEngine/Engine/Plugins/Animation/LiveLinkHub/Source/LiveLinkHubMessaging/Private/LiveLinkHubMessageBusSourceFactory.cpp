// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessageBusSourceFactory.h"

#include "LiveLinkHubMessageBusSource.h"

#include "SLiveLinkMessageBusSourceFactory.h"

#ifndef WITH_LIVELINK_HUB
#define WITH_LIVELINK_HUB 0
#endif


#define LOCTEXT_NAMESPACE "LiveLinkHubMessageBusSourceFactory"

FText ULiveLinkHubMessageBusSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "LiveLink Hub");
}

FText ULiveLinkHubMessageBusSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to a LiveLink Hub instance.");
}

TSharedPtr<SWidget> ULiveLinkHubMessageBusSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(SLiveLinkMessageBusSourceFactory)
		.OnSourceSelected(FOnLiveLinkMessageBusSourceSelected::CreateUObject(this, &ULiveLinkHubMessageBusSourceFactory::OnSourceSelected, InOnLiveLinkSourceCreated))
		.FactoryClass(GetClass());
}

TSharedPtr<FLiveLinkMessageBusSource> ULiveLinkHubMessageBusSourceFactory::MakeSource(const FText& Name,
																				   const FText& MachineName,
																				   const FMessageAddress& Address,
																				   double TimeOffset) const
{
	return MakeShared<FLiveLinkHubMessageBusSource>(Name, MachineName, Address, TimeOffset);
}

bool ULiveLinkHubMessageBusSourceFactory::IsEnabled() const
{
#if WITH_LIVELINK_HUB
return false;
#else
return true;
#endif
}

#undef LOCTEXT_NAMESPACE
