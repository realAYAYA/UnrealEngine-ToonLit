// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FWidgetBlueprintEditor;
class UBlueprint;
class UObject;

namespace UE
{
namespace UMG
{
	class SBindWidgetView;
}
}

/**
 * 
 */
class SBindWidgetView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBindWidgetView){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	virtual ~SBindWidgetView();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void HandleBlueprintChanged(UBlueprint* InBlueprint);
	void HandleObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

private:
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;
	TWeakPtr<UE::UMG::SBindWidgetView> ListView;
	bool bRefreshRequested;
};
