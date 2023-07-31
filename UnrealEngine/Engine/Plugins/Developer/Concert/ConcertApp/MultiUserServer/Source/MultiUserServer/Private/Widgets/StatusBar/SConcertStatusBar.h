// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SWidgetDrawer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SConcertStatusBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SConcertStatusBar) { }
	SLATE_END_ARGS()
	
	~SConcertStatusBar();

	void Construct(const FArguments& InArgs, FName StatusBarId);

private:

	TSharedPtr<SWidgetDrawer> WidgetDrawer;

	TSharedRef<SWidgetDrawer> MakeWidgetDrawer(FName StatusBarId);
};
