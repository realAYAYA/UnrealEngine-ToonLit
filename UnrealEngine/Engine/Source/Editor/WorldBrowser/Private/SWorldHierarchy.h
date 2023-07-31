// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FLevelCollectionModel;
struct FSlateBrush;


class SWorldHierarchy
	: public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorldHierarchy)
		:_InWorld(nullptr)
		{}
		SLATE_ARGUMENT(UWorld*, InWorld)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	SWorldHierarchy();
	~SWorldHierarchy();

private:
	void OnBrowseWorld(UWorld* InWorld);

	/**  */
	FReply OnSummonDetails();
	/**  */
	EVisibility GetCompositionButtonVisibility() const;
	FReply OnSummonComposition();
	const FSlateBrush* GetSummonCompositionBrush() const;

	/**  */
	TSharedRef<SWidget> GetFileButtonContent();

private:
	TSharedPtr<FLevelCollectionModel> WorldModel;
};
