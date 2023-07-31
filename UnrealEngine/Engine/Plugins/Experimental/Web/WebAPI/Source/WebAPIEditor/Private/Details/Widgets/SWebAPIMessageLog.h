// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebAPIMessageLog.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

/** Lists WebAPI log entries. */
class WEBAPIEDITOR_API SWebAPIMessageLog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWebAPIMessageLog)
	{ }
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIMessageLog>& InMessageLog);

private:
	/** Pointer to log widget */
	TSharedPtr<SWidget> LogListingWidget;

	TSharedPtr<FWebAPIMessageLog> MessageLog;
};
