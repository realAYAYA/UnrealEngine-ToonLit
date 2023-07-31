// Copyright Epic Games, Inc. All Rights Reserved.
/**
* Hold the View for the Motion Trail Options Widget
*/
#pragma once

#include "CoreMinimal.h"
#include "IDetailsView.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class IDetailsView;

class SEQUENCER_API SMotionTrailOptions : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMotionTrailOptions) {}

	SLATE_END_ARGS()
	~SMotionTrailOptions()
	{
	}

	void Construct(const FArguments& InArgs);

	TSharedPtr<IDetailsView> DetailsView;
};

