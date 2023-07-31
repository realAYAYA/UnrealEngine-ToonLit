// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SViewportToolBar.h"

class FExtender;
class FUICommandList;
class SUVEditor3DViewport;

/**
 * Toolbar that shows up at the top of the 3d viewport (has camera controls)
 */
class SUVEditor3DViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SUVEditor3DViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SUVEditor3DViewport> InUVEditor3DViewport);

private:
	TSharedRef<SWidget> MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeToolBar(const TSharedPtr<FExtender> InExtenders);

	/** The viewport that we are in */
	TWeakPtr<class SUVEditor3DViewport> UVEditor3DViewportPtr;

	TSharedPtr<FUICommandList> CommandList;
};
