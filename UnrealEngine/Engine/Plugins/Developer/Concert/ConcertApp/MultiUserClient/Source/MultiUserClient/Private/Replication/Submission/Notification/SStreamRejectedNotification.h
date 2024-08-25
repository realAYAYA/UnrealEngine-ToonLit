// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SBaseRejectionNotification.h"

namespace UE::MultiUserClient
{
	struct FAccumulatedStreamErrors;
	
	/** Displays FAccumulatedStreamErrors. */
	class SStreamRejectedNotification : public SBaseRejectionNotification
	{
		using Super = SBaseRejectionNotification;
	public:

		SLATE_BEGIN_ARGS(SStreamRejectedNotification)
		{}
			/** The errors to display */
			SLATE_ATTRIBUTE(const FAccumulatedStreamErrors*, Errors)
			/** Called when the close button is pressed. Is supposed to close the notification. */
			SLATE_EVENT(FOnClicked, OnCloseClicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Rebuilds the UI from the errors. */
		void Refresh();
		
		//~ Begin INotificationWidget interface
		virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override {}
		virtual TSharedRef<SWidget> AsWidget() override { return AsShared(); }
		//~ End INotificationWidget interface

	private:

		TAttribute<const FAccumulatedStreamErrors*> ErrorsAttribute;
		
		void AddTimeoutErrorWidget(TSharedRef<SVerticalBox> Result, const FAccumulatedStreamErrors& Errors);
		void AddAuthorityConflictErrorWidget(TSharedRef<SVerticalBox> Result, const FAccumulatedStreamErrors& Errors);
		void AddSemanticErrorWidget(TSharedRef<SVerticalBox> Result, const FAccumulatedStreamErrors& Errors);
		void AddStreamCreationErrorWidget(TSharedRef<SVerticalBox> Result, const FAccumulatedStreamErrors& Errors);
	};
}

