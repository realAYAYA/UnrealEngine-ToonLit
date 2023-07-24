// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"

class SContextualAnimViewport;

class SContextualAnimViewportToolBar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SContextualAnimViewportToolBar)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SContextualAnimViewport> InViewport);

	// ~SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	// ~End of SCommonEditorViewportToolbarBase interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
