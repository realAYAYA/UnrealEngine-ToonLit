// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Misc/Attribute.h"
#include "Internationalization/Text.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/DeclarativeSyntaxSupport.h" 

#include "ITraceObject.h"

class STableViewBase;

class STraceObjectRowWidget : public STableRow<TSharedPtr<ITraceObject>>
{
	SLATE_BEGIN_ARGS(STraceObjectRowWidget) {}
		/** The current text to highlight */
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<ITraceObject> InObject);
	virtual ~STraceObjectRowWidget() {}

protected:
	/* The trace object that this tree item represents */
	TSharedPtr<ITraceObject> Object;
	TAttribute<FText> HighlightTextAttribute;

};
