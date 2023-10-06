// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "SVisualLogger.h"
#include "Widgets/Docking/SDockTab.h"
#include "LogVisualizerPublic.h"

class SVisualLoggerTab : public SDockTab
{
public:
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return FLogVisualizer::Get().GetEvents().OnKeyboardEvent.Execute(MyGeometry, InKeyEvent);
	}
};
