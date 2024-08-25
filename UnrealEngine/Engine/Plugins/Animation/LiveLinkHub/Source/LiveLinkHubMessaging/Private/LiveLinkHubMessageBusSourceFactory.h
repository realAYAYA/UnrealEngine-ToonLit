// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMessageBusFinder.h"		// For FProviderPollResultPtr typedef
#include "LiveLinkMessageBusSourceFactory.h"
#include "LiveLinkHubMessageBusSourceFactory.generated.h"

struct FMessageAddress;


UCLASS()
class LIVELINKHUBMESSAGING_API ULiveLinkHubMessageBusSourceFactory : public ULiveLinkMessageBusSourceFactory
{
public:
	GENERATED_BODY()

	//~ Begin ULiveLinkSourceFactory interface
	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual bool IsEnabled() const override;
	//~ End ULiveLinkSourceFactory interface

protected:
	//~ ULiveLinkMessageBusSourceFactory interface
	virtual TSharedPtr<class FLiveLinkMessageBusSource> MakeSource(const FText& Name,
																   const FText& MachineName,
																   const FMessageAddress& Address,
																   double TimeOffset) const override;
};
