// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SActorPaletteViewport;
class FActorPaletteViewportClient;

//////////////////////////////////////////////////////////////////////

class SActorPalette : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActorPalette) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, int32 InTabIndex);

private:
	TSharedPtr<SActorPaletteViewport> ActorPaletteViewport;
	TSharedPtr<FActorPaletteViewportClient> ActorPaletteViewportClient;
	int32 TabIndex;
};
