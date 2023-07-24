// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SScrollBar.h"

class SLATE_API SLinkableScrollBar : public SScrollBar
{
public:
	enum class ELinkId : uint8
	{
		LeftLink,
		RightLink,
		Disabled
	};
	
	virtual void SetState(float InOffsetFraction, float InThumbSizeFraction, bool bCallOnUserScrolled = false) override;

	static void LinkScrollBars(TSharedRef<SLinkableScrollBar> Left, TSharedRef<SLinkableScrollBar> Right, TAttribute<TArray<FVector2f>> ScrollSyncRate);

private:
	TWeakPtr<SLinkableScrollBar> LinkedScrollBar;
	ELinkId LinkId = ELinkId::Disabled; // LeftLink is associated w/ x value in SyncScrollRate while RightLink is associated w/ y

	// list of scroll distance pairs that determine variable scroll rate.
	// scroll values of Left panel will match X components while the right panel will match Y components
	TAttribute<TArray<FVector2f>> ScrollSyncRate;
};
