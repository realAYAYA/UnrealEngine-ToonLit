// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSmartObjectViewportToolbar.h"
#include "SSmartObjectViewport.h"
#include "PreviewProfileController.h"

#define LOCTEXT_NAMESPACE "SmartObjectViewportToolBar"

void SSmartObjectViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<SSmartObjectViewport> InViewport)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments().AddRealtimeButton(false).PreviewProfileController(MakeShared<FPreviewProfileController>()), InViewport);
}

#undef LOCTEXT_NAMESPACE