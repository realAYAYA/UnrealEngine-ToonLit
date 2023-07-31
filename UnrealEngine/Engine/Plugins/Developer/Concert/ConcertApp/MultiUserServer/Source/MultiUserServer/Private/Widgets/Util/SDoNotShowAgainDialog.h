// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dialog/SCustomDialog.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FArchivedSessionHistoryController;
class IConcertSyncServer;
class SCheckBox;

namespace UE::ConcertSyncCore
{
	struct FHistoryAnalysisResult;
}

namespace UE::MultiUserServer
{
	/** Text message with a "Do not show again" checkbox. */
	class SDoNotShowAgainDialog : public SCustomDialog
	{
	public:

		DECLARE_DELEGATE_OneParam(FOnClosed, bool /*bDoNotShowAgain*/);

		SLATE_BEGIN_ARGS(SDoNotShowAgainDialog)
		{}
			SLATE_ARGUMENT(FText, Title)
			SLATE_ARGUMENT(TArray<FButton>, Buttons)
			SLATE_NAMED_SLOT(FArguments, Content)
			SLATE_EVENT(FOnClosed, DoNotShowAgainCallback)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		TSharedPtr<SCheckBox> DoNotShowAgainCheckbox;
	};
}
