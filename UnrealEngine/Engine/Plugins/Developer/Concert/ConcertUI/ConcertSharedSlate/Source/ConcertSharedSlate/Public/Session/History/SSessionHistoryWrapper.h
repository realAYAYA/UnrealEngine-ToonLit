// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbstractSessionHistoryController.h"
#include "SSessionHistory.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/** Shows a SSessionHistory widget - keeping alive the controller as long as this widget lives. Useful for closeable tabs. */
class CONCERTSHAREDSLATE_API SSessionHistoryWrapper : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionHistoryWrapper) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FAbstractSessionHistoryController> InController);

private:

	/** Manages SessionHistory */
	TSharedPtr<FAbstractSessionHistoryController> Controller;
};
