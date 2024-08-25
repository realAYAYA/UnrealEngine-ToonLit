// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMessageBusFinder.h"		// For FProviderPollResultPtr typedef
#include "LiveLinkSourceFactory.h"
#include "LiveLinkMessageBusSourceFactory.generated.h"

struct FMessageAddress;


UCLASS()
class LIVELINK_API ULiveLinkMessageBusSourceFactory : public ULiveLinkSourceFactory
{
public:
	GENERATED_BODY()

	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;

	virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;

	static FString CreateConnectionString(const struct FProviderPollResult& Result);

protected:
	// This allows the creation of a message bus source derived from FLiveLinkMessageBusSource
	virtual TSharedPtr<class FLiveLinkMessageBusSource> MakeSource(const FText& Name,
																   const FText& MachineName,
																   const FMessageAddress& Address,
																   double TimeOffset) const;

	void OnSourceSelected(FProviderPollResultPtr SelectedSource, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};
