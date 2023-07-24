// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "ISourceControlProvider.h"
#include "ISourceControlOperation.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

/////////////////////////////////////////////////////
// SPlatformSetupMessage

// This widget displays a setup message indicating if the game project is configured for a platform or not

class SHAREDSETTINGSWIDGETS_API SPlatformSetupMessage : public SCompoundWidget
{
	enum ESetupState
	{
		MissingFiles,
		NeedsCheckout,
		ReadOnlyFiles,
		ReadyToModify,
		GettingStatus
	};

	SLATE_BEGIN_ARGS(SPlatformSetupMessage)
		{}

		// Name of the platform
		SLATE_ARGUMENT(FText, PlatformName)

		// Called when the Setup button is clicked
		SLATE_EVENT(FSimpleDelegate, OnSetupClicked)

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FString& InTargetFilename);

	TAttribute<bool> GetReadyToGoAttribute() const;

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of Swidget interface
private:
	int32 GetSetupStateAsInt() const;
	bool IsReadyToGo() const;
	FSlateColor GetBorderColor() const;

	TSharedRef<SWidget> MakeRow(FName IconName, FText Message, FText ButtonMessage);

	FReply OnButtonPressed();

	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	// Returns the setup state of a specified file
	ESetupState GetSetupStateBasedOnFile(bool bInitStatus);

	// Updates the cache CachedSetupState 
	void UpdateCache(bool bForceUpdate);
private:
	FString TargetFilename;
	ESetupState CachedSetupState;
	FSimpleDelegate OnSetupClicked;
};
