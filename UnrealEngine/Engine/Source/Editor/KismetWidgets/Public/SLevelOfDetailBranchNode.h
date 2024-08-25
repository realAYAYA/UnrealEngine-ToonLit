// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class SWidget;
struct FGeometry;

/////////////////////////////////////////////////////
// SLevelOfDetailBranchNode

DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGetActiveDetailSlotContent, bool);

class KISMETWIDGETS_API SLevelOfDetailBranchNode : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLevelOfDetailBranchNode)
		: _UseLowDetailSlot(false)
		{}

		// Should the low detail or high detail slot be shown?
		SLATE_ATTRIBUTE(bool, UseLowDetailSlot)

		// The low-detail slot
		SLATE_NAMED_SLOT(FArguments, LowDetail)

		// The high-detail slot
		SLATE_NAMED_SLOT(FArguments, HighDetail)

		SLATE_EVENT(FOnGetActiveDetailSlotContent, OnGetActiveDetailSlotContent)
	SLATE_END_ARGS()

public:
	SLevelOfDetailBranchNode();

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	// Determine whether we need to show the low-detail slot or high-detail slot
	void RefreshLODSlotContent();

protected:
	// What kind of slot was shown last frame
	int LastCachedValue;

	// The attribute indicating the kind of slot to show
	TAttribute<bool> ShowLowDetailAttr;

	// The low-detail child slot
	TSharedRef<SWidget> ChildSlotLowDetail;

	// The high-detail child slot
	TSharedRef<SWidget> ChildSlotHighDetail;

	FOnGetActiveDetailSlotContent OnGetActiveDetailSlotContent;

};
