// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SDebugPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDebugPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnReloadTexturesClicked();
	FReply OnDisplayTextureAtlases();
	FReply OnDisplayFontAtlases();
	FReply OnFlushFontCacheClicked();
	FReply OnTestSuiteClicked();
};

